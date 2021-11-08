/*
Hanzhe Huang
10/20/2021
oss.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <time.h> //time
#include <sys/wait.h> //wait
#include <signal.h>
#include <ctype.h> //isprint
#include <unistd.h> //sleep, alarm
#include "queue.c"
#include "structs.h"
#include "oss.h"

// Reference: https://www.tutorialspoint.com/inter_process_communication/inter_process_communication_shared_memory.htm
// Reference: https://stackoverflow.com/questions/19461744/how-to-make-parent-wait-for-all-child-processes-to-finish
// Reference: https://www.tutorialspoint.com/inter_process_communication/inter_process_communication_message_queues.htm
// Reference: https://www.geeksforgeeks.org/ipc-using-message-queues/

extern int errno;
static char* logName;

int main(int argc, char** argv) {
	// Signal handlers;
	signal(SIGINT, sigint_parent);
	signal(SIGALRM, sigalrm);
	
	// Interval constants
	const int maxTimeBetweenNewProcsSecs = 1;
	const int maxTimeBetweenNewProcsNS = 50000000;
	const int maxTimeBetweenUnblockSecs = 2;
	const int maxTimeBetweenUnblockNS = 1000;
	
	// Statistics
	int dispatchNum = 0, processNum = 0, idle = 1;
	struct clock sumWaitTime = {0, 0}, sumSystemTime = {0, 0}, sumBlockedTimeIO = {0, 0}, sumBlockedTimeCPU = {0, 0}, CPUIdleTime = {0, 0};
	
	// Message queue init
	int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
	if (msgid == -1) {
		perror("Error: msgget");
		exit(-1);
	}
	
	FILE* fptr;
	int i;
	int pid = 1, status = 0;
	int option = 0;
	int terminationTime = 30;
	int queueNum, dispatchTime;
	struct clock tempClock = {0, 0};
	struct clock lastNewProcessTime = {0, 0}, lastUnblockTime = {0, 0};
	time_t t;
	srand((unsigned) time(&t));
	
	// Initialize queue items to 0 
	int queues[3][MAX_PRO];
	initQueue(queues[0], MAX_PRO);
	initQueue(queues[1], MAX_PRO);
	initQueue(queues[2], MAX_PRO);
	
	lastUnblockTime.clockSecs = 0;
	lastUnblockTime.clockNS = 0;
	
	logName = malloc(200);
	logName = "logfile";

	// Reference: https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
	while ((option = getopt(argc, argv, "hs:l:")) != -1) {
		switch (option) {

		case 'h':
			printf("-----------------------------------\n");
			printf("Usage:\n");
			printf("%s [-h] [-s t] [-l f]\n", argv[0]);
			printf("-----------------------------------\n");
			printf("Default: Opens OSS.\n");
			printf("[-h]: Prints usage of this program.\n");
			printf("[-s t]: Indicates max seconds before the program will terminate.\n");
			printf("[-l f]: Specifies a name for the logfile.\n");
			printf("-----------------------------------\n");
			return 0;
			break;
			
		case 's':
			terminationTime = atoi(optarg);
			if (terminationTime > 60 || terminationTime < 1) {
				perror("Invalid or large sleep duration. sec must be between 1-60.");
				return -1;
			}
			alarm(terminationTime);
			break;
			
		case 'l':
			logName = optarg;
			break;
			
		case '?':
			if (isprint(optopt))
				perror("Unknown option");
			else
				perror("Unknown option character");
			return -1;
			break;
		}
	}
	
	// Clear log file
	fptr = fopen(logName, "w");
	fclose(fptr);
	fptr = fopen(logName, "a");
	
	// Shm Init 
	initshmobj(shmobj());
	
	struct shmseg *shmp;
    int shmid = shmget(SHM_KEY, BUF_SIZE, 0666|IPC_CREAT);
	if (shmid == -1) {
		perror("Error: shmget");
		exit(-1);
	}
	shmp = shmat(shmid, 0, 0);
	
	// Main loop
	while(1){
		idle = 1;
		
		// If process limit isn't reached and clock has advanced, fork a process
		for (i = 0; i < MAX_PRO; i++){
			if (shmp->processTable[i].processPid == 0) {
				break;
			}
		}
		if (i != MAX_PRO && (isClockLarger(shmp->ossclock, lastNewProcessTime) == 0) && processNum < TOTAL_PRO){
			idle = 0;
			pid = fork();
			switch ( pid )
			{
			case -1:
				perror("Error: fork");
				return -1;

			case 0: // Child, terminates
				child();
				break;

			default: // Parent
				// Increment total processes created
				processNum++;
				if (maxTimeBetweenNewProcsSecs != 0){
					lastNewProcessTime.clockSecs = shmp->ossclock.clockSecs + (rand() % maxTimeBetweenNewProcsSecs);
				}
				else {
					lastNewProcessTime.clockSecs = shmp->ossclock.clockSecs + 0;
				}
				lastNewProcessTime.clockNS = shmp->ossclock.clockNS + (rand() % maxTimeBetweenNewProcsNS);
				if (lastNewProcessTime.clockNS >= 1000000000){
					lastNewProcessTime.clockSecs++;
					lastNewProcessTime.clockNS -= 1000000000;
				}
				
				// Store pid to process table, put into queue 0 or 1
				shmp->processTable[i].processPid = pid;
				shmp->processTable[i].processInitTime = shmp->ossclock;
				shmp->processTable[i].processInitWaitTime = shmp->ossclock;
				shmp->processTable[i].processBlockedTime.clockSecs = 0;
				shmp->processTable[i].processBlockedTime.clockNS = 0;
				if (enqueue(queues[1], MAX_PRO, pid) != -1){
					printf("Saving: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
					fprintf(fptr, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				}
				else if (enqueue(queues[0], MAX_PRO, pid) != -1){
					printf("Saving: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
					fprintf(fptr, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				}
				else { 
					perror("Error: enqueue");
					exit(-1);
				}
				break;
			}
		}
		
		// Grab item from highest priority non-empty queue, dispatch it
		if ((pid = dequeue(queues[0], MAX_PRO)) != -1){
			queueNum = 0;
		}
		else if ((pid = dequeue(queues[1], MAX_PRO)) != -1){
			queueNum = 1;
		}
		else {
			queueNum = -1;
		}
		if (queueNum != -1){
			// Dispatch process
			dispatchTime = (rand() % MAX_DISPATCH);
			incrementClockShm(shmobj(), 0, dispatchTime);
			printf("Saving: Dispatching process with PID %d from queue %d at time %d:%d,\n", pid, queueNum, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			printf("Saving: total time this dispatch was %d nanoseconds.\n", dispatchTime);
			fprintf(fptr, "OSS: Dispatching process with PID %d from queue %d at time %d:%d,\n", pid, queueNum, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			fprintf(fptr, "OSS: total time this dispatch was %d nanoseconds.\n", dispatchTime);
			
			// Record wait time
			sumWaitTime = mathClock(0, sumWaitTime, mathClock(1, shmp->ossclock,shmp->processTable[getPidIndex(shmobj(),pid)].processInitWaitTime));
			dispatchNum++;
			
			// Send message to child if item is found, wait for child
			msg_t.mtype = pid;
			msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
			msgrcv(msgid, &msg_t, sizeof(msg_t), 1, 0);
			
			if (msg_t.queueType != 2) {
				// Remove from process table if process is finished, record system and blocked time
				sumSystemTime = mathClock(0, sumSystemTime, mathClock(1, shmp->ossclock, shmp->processTable[getPidIndex(shmobj(),pid)].processInitTime));
				if (shmp->processTable[getPidIndex(shmobj(),pid)].processBlockedTime.clockSecs > 0) {
					sumBlockedTimeIO = mathClock(0, sumBlockedTimeIO, shmp->processTable[getPidIndex(shmobj(),pid)].processBlockedTime);
				}
				else {
					sumBlockedTimeCPU = mathClock(0, sumBlockedTimeCPU, shmp->processTable[getPidIndex(shmobj(),pid)].processBlockedTime);
				}
				
				printf("Saving: Receiving that process with PID %d ran for %d nanoseconds.\n", pid, msg_t.msgclock.clockNS);
				fprintf(fptr, "OSS: Receiving that process with PID %d ran for %d nanoseconds.\n", pid, msg_t.msgclock.clockNS);
				shmp->processTable[getPidIndex(shmobj(),pid)].processPid = 0;
			}
			else {
				// Put process into blocked queue
				printf("Saving: Receiving that process with PID %d ran for %d nanoseconds,\n", pid, msg_t.msgclock.clockNS);
				printf("Saving: not using its entire time quantum.\n");
				fprintf(fptr, "OSS: Receiving that process with PID %d ran for %d nanoseconds,\n", pid, msg_t.msgclock.clockNS);
				fprintf(fptr, "OSS: not using its entire time quantum.\n");
				if (enqueue(queues[2], MAX_PRO, pid) != -1){
					// Record block time
					shmp->processTable[getPidIndex(shmobj(),pid)].processInitBlockedTime = shmp->ossclock;
					printf("Saving: Putting process with PID %d into blocked queue.\n", pid);
					fprintf(fptr, "OSS: Putting process with PID %d into blocked queue.\n", pid);
				}
				else {
					perror("Error: enqueue");
					exit(-1);
				}
			}
			incrementClockShm(shmobj(), msg_t.msgclock.clockSecs, msg_t.msgclock.clockNS);
			idle = 0;
		}
		
		// Check if block queue has items and time has advanced, move to ready
		if (queues[2][0] != 0 && (isClockLarger(shmp->ossclock, lastUnblockTime) == 0)){
			// Record block stats, Move to queue 0 or 1 depending on time spent in blocked queue
			// Also increment system clock by some time
			dispatchTime = (rand() % MAX_UNBLOCK);
			incrementClockShm(shmobj(), 0, dispatchTime);
			
			pid = dequeue(queues[2], MAX_PRO);
			tempClock = shmp->processTable[getPidIndex(shmobj(),pid)].processBlockedTime;
			shmp->processTable[getPidIndex(shmobj(),pid)].processBlockedTime = mathClock(0, tempClock, mathClock(1, shmp->ossclock, shmp->processTable[getPidIndex(shmobj(),pid)].processInitBlockedTime));
			if (shmp->processTable[getPidIndex(shmobj(),pid)].processBlockedTime.clockSecs >= 1){
				if (enqueue(queues[0], MAX_PRO, pid) != -1){
					printf("Saving: Moving process with PID %d from blocked queue to queue %d at time %d:%d,\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
					fprintf(fptr, "OSS: Moving process with PID %d from blocked queue to queue %d at time %d:%d,\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				}
				else { 
					perror("Error: enqueue");
					exit(-1);
				}
			}
			else if (enqueue(queues[1], MAX_PRO, pid) != -1){
				printf("Saving: Moving process with PID %d from blocked queue to queue %d at time %d:%d,\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				fprintf(fptr, "OSS: Moving process with PID %d from blocked queue to queue %d at time %d:%d,\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			}
			else { 
				perror("Error: enqueue");
				exit(-1);
			}
			printf("Saving: total time transferring was %d nanoseconds.\n", dispatchTime);
			fprintf(fptr, "OSS: total time transferring was %d nanoseconds.\n", dispatchTime);
			
			// Record wait stats & refresh unblock period
			shmp->processTable[getPidIndex(shmobj(),pid)].processInitWaitTime = shmp->ossclock;
			lastUnblockTime.clockSecs = shmp->ossclock.clockSecs + (rand() % maxTimeBetweenUnblockSecs);
			lastUnblockTime.clockNS = shmp->ossclock.clockNS + (rand() % maxTimeBetweenUnblockNS);
			if (lastUnblockTime.clockNS >= 1000000000){
				lastUnblockTime.clockSecs++;
				lastUnblockTime.clockNS -= 1000000000;
			}
			idle = 0;
		}
		
		// Check if no action was taken, increment system clock by a small amount
		if (idle == 1 && !(processNum >= TOTAL_PRO && (queues[0][0] == 0) && (queues[1][0] == 0) && (queues[2][0] == 0))){
			tempClock.clockSecs = 0;
			tempClock.clockNS = 50000000;
			incrementClockShm(shmobj(), tempClock.clockSecs,  tempClock.clockNS);
			CPUIdleTime = mathClock(0, CPUIdleTime, tempClock);
		}
		
		// Check if oss is due for termination
		if (processNum >= TOTAL_PRO){
			if (waitpid(0, &status, WNOHANG) < 1 && (queues[0][0] == 0) && (queues[1][0] == 0) && (queues[2][0] == 0)){
				// Normal termination, show stats
				printf("-----------------------------------\n");
				printf("Number of processes: %d\n", processNum);
				printf("Average process wait time: %d:%d\n", sumWaitTime.clockSecs / dispatchNum, sumWaitTime.clockNS / dispatchNum);
				printf("Average process system time: %d:%d\n", sumSystemTime.clockSecs / processNum, sumSystemTime.clockNS / processNum);
				printf("Average IO-bound process blocked time: %d:%d\n", sumBlockedTimeIO.clockSecs / processNum, sumBlockedTimeIO.clockNS / processNum);
				printf("Average CPU-bound process blocked time: %d:%d\n", sumBlockedTimeCPU.clockSecs / processNum, sumBlockedTimeCPU.clockNS / processNum);
				printf("CPU idle time: %d:%d\n", CPUIdleTime.clockSecs, CPUIdleTime.clockNS);
				printf("CPU termination time: %d:%d\n", shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				printf("-----------------------------------\n");
				fclose(fptr);
				logexit();
				return 0;
			}
		}
	}
	return -1;
}

// Logs termination time
void logexit(){
	FILE* fptr;
	char timeBuffer[40];
	time_t tempTime = time(NULL);
	fptr = fopen(logName, "a");
	strftime(timeBuffer, 40, "%H:%M:%S", localtime(&tempTime));
	printf("Saving: %s %d terminated\n", timeBuffer, getpid());
	fprintf(fptr, "OSS: %s %d terminated\n", timeBuffer, getpid());
	fclose(fptr);
}

// Signals
void sigint_parent(int sig){
	printf("Process %d exiting...\n",getpid());
	deallocate();
	printf("Terminating child processes...\n");
	kill(0, SIGINT);
	parent();
	logexit();
	exit(0);
}

void sigint(int sig){
	kill(0, SIGINT);
	exit(0);
}

void sigalrm(int sig){
	printf("Program timed out.\n");
	kill(0, SIGINT);
	exit(0);
}

// Parent function to wait for children processes
void parent(){
	int childpid;
	while ((childpid = (wait(NULL))) > 0);
	printf("Stopped waiting for children.\n");
}

// Reference: http://www.cs.umsl.edu/~sanjiv/classes/cs4760/src/shm.c
// Reference: https://www.geeksforgeeks.org/signals-c-set-2/
void child(){
	signal(SIGINT, sigint);
	signal(SIGALRM, SIG_IGN);
	
	// Exec user process
	if ((execl("userprocess", "userprocess", (char*)NULL)) == -1){
		perror("Error: execl/stdin");
		exit(-1);
	}
	exit(1);
}

// Deallocates shared memory & message queue
void deallocate(){
	//shm
    int shmid = shmget(SHM_KEY, BUF_SIZE, 0666|IPC_CREAT);
	if (shmid == -1) {
		perror("Error: shmget");
		exit(-1);
	}
	if (shmctl(shmid, IPC_RMID, 0) == -1) {
		perror("Error: shmctl");
		exit(-1);
	}
	//msg
	int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
	if (msgid == -1) {
		perror("Error: msgget");
		exit(-1);
	}
	if (msgctl(msgid, IPC_RMID, NULL) == -1) {
		perror("Error: msgctl");
		exit(-1);
	}
	printf("Shared memory & message queue deallocated.\n");
}

// Returns the shared memory segment
struct shmseg* shmobj(){
	struct shmseg *shmp;
    int shmid = shmget(SHM_KEY, BUF_SIZE, 0666|IPC_CREAT);
	if (shmid == -1) {
		perror("Error: shmget");
		exit(-1);
	}
	shmp = shmat(shmid, 0, 0);
	return shmp;
}

// Initializes shared memory segment
void initshmobj(struct shmseg* shmp){
	int i;
	shmp->ossclock.clockSecs = 0;
	shmp->ossclock.clockNS = 0;
	for (i = 0; i < MAX_PRO; i++){
		shmp->processTable[i].processPid = 0;
		shmp->processTable[i].processBlockNS = 0;
	}
}

// Math one clock to another 
struct clock mathClock(int operation, struct clock inClock, struct clock mathClock){
	if (operation == 0) {
		inClock.clockSecs += mathClock.clockSecs;
		inClock.clockNS += mathClock.clockNS;
	}
	else {
		inClock.clockSecs -= mathClock.clockSecs;
		inClock.clockNS -= mathClock.clockNS;
	}
	if (inClock.clockNS >= 1000000000){
		inClock.clockSecs++;
		inClock.clockNS -= 1000000000;
	}
	if (inClock.clockNS < 0){
		inClock.clockSecs--;
		inClock.clockNS += 1000000000;
	}
	return inClock;
}

// Increments the clock by seconds and nanoseconds
void incrementClockShm(struct shmseg* shmp, int incS, int incNS){
	shmp->ossclock.clockSecs += incS;
	shmp->ossclock.clockNS += incNS;
	if (shmp->ossclock.clockNS >= 1000000000){
		shmp->ossclock.clockSecs++;
		shmp->ossclock.clockNS -= 1000000000;
	}
}

// Check if clockA is larger than clockB
int isClockLarger(struct clock clockA, struct clock clockB){
	if (clockA.clockSecs > clockB.clockSecs){
		return 0;
	}
	else if (clockA.clockSecs < clockB.clockSecs){
		return -1;
	}
	else if (clockA.clockNS > clockB.clockNS){
		return 0;
	}
	else {
		return -1;
	}
}

// Returns the index of a process in the process table
int getPidIndex(struct shmseg* shmp, int pid){
	int i;
	for (i = 0; i < MAX_PRO; i++){
		if (shmp->processTable[i].processPid == pid){
			return i;
		}
	}
	return -1;
}