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
	const int maxTimeBetweenNewProcsNS = 1;
	
	// Message queue init
	int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
	if (msgid == -1) {
		perror("Error: msgget");
		exit(-1);
	}
	
	/*
	msg_t.type = 1;
	msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
	*/
	
	FILE* fptr;
	int i;
	int pid = 1;
	int status;
	int option = 0;
	int terminationTime = 0;
	int processNum = 0;
	int queueNum;
	int dispatchTime;
	struct clock lastClockTime;
	time_t t;
	srand((unsigned) time(&t));
	
	// Initialize queue items to 0 
	int queues[3][MAX_PRO];
	initQueue(queues[0], MAX_PRO);
	initQueue(queues[1], MAX_PRO);
	initQueue(queues[2], MAX_PRO);
	
	logName = malloc(200);
	logName = "logfile";

	// Reference: https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
	while ((option = getopt(argc, argv, "ht:")) != -1) {
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
		
		// If process limit isn't reached and clock has advanced, fork a process
		for (i = 0; i < MAX_PRO; i++){
			if (shmp->processTable[i].processPid == 0) {
				break;
			}
		}
		if (i != MAX_PRO && (isClockLarger(shmp->ossclock, lastClockTime) == 0) && processNum < TOTAL_PRO){
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
				lastClockTime.clockSecs = shmp->ossclock.clockSecs + (rand() % maxTimeBetweenNewProcsSecs);
				lastClockTime.clockNS = shmp->ossclock.clockNS + (rand() % maxTimeBetweenNewProcsNS);
				if (lastClockTime.clockNS >= 1000000000){
					lastClockTime.clockSecs++;
					lastClockTime.clockNS -= 1000000000;
				}
				
				// Store pid to process table, put into queue 0 or 1
				shmp->processTable[i].processPid = pid;
				if (enqueue(queues[0], MAX_PRO, pid) != -1){
					printf("Saving: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
					fprintf(fptr, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				}
				else if (enqueue(queues[1], MAX_PRO, pid) != -1){
					printf("Saving: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
					fprintf(fptr, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				}
				else { 
					perror("Error: enqueue");
					exit(-1);
				}
				break;
			}
		}
		else {
			// Advance clock by a small amount if no process was forked
			incrementClock(shmobj(), 1, 0);
			printf("Saving: No process forked, passing time to %d:%d.\n", shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			fprintf(fptr, "OSS: No process forked, passing time to %d:%d.\n", shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
		}
		
		// Grab item from highest priority non-empty queue
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
			dispatchTime = (rand() % MAX_DISPATCH);
			incrementClock(shmobj(), 0, dispatchTime);
			printf("Saving: Dispatching process with PID %d from queue %d at time %d:%d,\n", pid, queueNum, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			printf("Saving: total time this dispatch was %d nanoseconds.\n", dispatchTime);
			fprintf(fptr, "OSS: Dispatching process with PID %d from queue %d at time %d:%d,\n", pid, queueNum, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			fprintf(fptr, "OSS: total time this dispatch was %d nanoseconds.\n", dispatchTime);
			
			// Send message to child if item is found, wait for child
			msg_t.mtype = pid;
			msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
			msgrcv(msgid, &msg_t, sizeof(msg_t), 1, 0);
			
			if (msg_t.queueType != 2) {
				// Remove from process table if process is finished
				printf("Saving: Receiving that process with PID %d ran for %d nanoseconds.\n", pid, msg_t.msgclock.clockNS);
				fprintf(fptr, "OSS: Receiving that process with PID %d ran for %d nanoseconds.\n", pid, msg_t.msgclock.clockNS);
				for (i = 0; i < MAX_PRO; i++){
					if (shmp->processTable[i].processPid == pid) {
						shmp->processTable[i].processPid = 0;
						shmp->processTable[i].processBlockNS = 0;
						break;
					}
				}
			}
			else {
				// Put process into blocked queue
				printf("Saving: Receiving that process with PID %d ran for %d nanoseconds,\n", pid, msg_t.msgclock.clockNS);
				printf("Saving: not using its entire time quantum.\n");
				fprintf(fptr, "OSS: Receiving that process with PID %d ran for %d nanoseconds,\n", pid, msg_t.msgclock.clockNS);
				fprintf(fptr, "OSS: not using its entire time quantum.\n");
				if (enqueue(queues[2], MAX_PRO, pid) != -1){
					printf("Saving: Putting process with PID %d into blocked queue.", pid);
					fprintf(fptr, "OSS: Putting process with PID %d into blocked queue.", pid);
				}
				else {
					perror("Error: enqueue");
					exit(-1);
				}
			}
			incrementClock(shmobj(), msg_t.msgclock.clockSecs, msg_t.msgclock.clockNS);
		}
		
		// Check if block queue has items, move to ready
		if ((pid = dequeue(queues[2], MAX_PRO)) != -1){
			printf("%d\n",queues[2][getLast(queues[2], MAX_PRO)]);
			// Store pid to process table, put into queue 0 or 1
			if (enqueue(queues[0], MAX_PRO, pid) != -1){
				printf("Saving: Moving process with PID %d from blocked queue to queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				fprintf(fptr, "OSS: Moving process with PID %d from blocked queue to queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			}
			else if (enqueue(queues[1], MAX_PRO, pid) != -1){
				printf("Saving: Moving process with PID %d from blocked queue to queue %d at time %d:%d.\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
				fprintf(fptr, "OSS: Moving process with PID %d from blocked queue to queue %d at time %d:%d.\n", pid, 1, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
			}
			else { 
				perror("Error: enqueue");
				exit(-1);
			}
		}
		
		if (processNum >= TOTAL_PRO){
			sleep(1); // Catch any slow child processes
			if (waitpid(0, &status, WNOHANG) < 1){
				printf("%d\n",(waitpid(0, &status, WNOHANG)));
				printf("%d\n",getLast(queues[0], MAX_PRO));
				printf("%d\n",getLast(queues[1], MAX_PRO));
				printf("%d\n",getLast(queues[2], MAX_PRO));
				printf("OSS: Reached process limit.\n");
				break;
			}
		}
	}
	fclose(fptr);
	logexit();
	return -1;
}

// Logs termination time
void logexit(){
	FILE* fptr;
	char timeBuffer[40];
	time_t tempTime = time(NULL);
	fptr = fopen(logName, "a");
	strftime(timeBuffer, 40, "%H:%M:%S", localtime(&tempTime));
	printf("OSS: %s %d terminated\n", timeBuffer, getpid());
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
	parent();
	deallocate();
	logexit();
	exit(0);
}

// Parent function to wait for children processes
void parent(){
	printf("Waiting for children...\n");
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

// Increments the clock by seconds and nanoseconds
void incrementClock(struct shmseg* shmp, int incS, int incNS){
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