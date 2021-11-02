﻿/*
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
	//const int maxTimeBetweenNewProcsSecs = 1;
	//const int maxTimeBetweenNewProcsNS = 1;
	
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
	int option = 0;
	int terminationTime = 0;
	int processNum = 0;
	int queues[3][MAX_PRO];
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
	
	// Shm Init 
	initshmobj(shmobj());
	
	struct shmseg *shmp;
    int shmid = shmget(SHM_KEY, BUF_SIZE, 0666|IPC_CREAT);
	if (shmid == -1) {
		perror("Error: shmget");
		exit(-1);
	}
	shmp = shmat(shmid, 0, 0);
	
	// Fork loop
	while(1){
		
		for (i = 0; i < MAX_PRO; i++){
			if (shmp->processTable[i].processPid < 1) {
				break;
			}
		}
		if (i != MAX_PRO){
			pid = fork();
			switch ( pid )
			{
			case -1:
				perror("Error: fork");
				return -1;

			case 0: // Child, terminates
				child();
				break;

			default: // Parent, loops
				break;
			}
		}
		// Store pid to process table
		shmp->processTable[i].processPid = pid;
		fptr = fopen(logName, "a");
		printf("Saving: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
		fprintf(fptr, "OSS: Generating process with PID %d and putting it in queue %d at time %d:%d.\n", pid, 0, shmp->ossclock.clockSecs, shmp->ossclock.clockNS);
		fclose(fptr);
		
		// Grab item from highest priority non-empty queue
		
		
		// Send message to child if item is found, wait for child
		msg_t.mtype = pid;
		msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
		msgrcv(msgid, &msg_t, sizeof(msg_t), 1, 0);
		fptr = fopen(logName, "a");
		printf("Saving: Receiving that process with PID %d ran for %d nanoseconds.\n", pid, msg_t.msgclock.clockNS);
		fprintf(fptr, "OSS: Receiving that process with PID %d ran for %d nanoseconds.\n", pid, msg_t.msgclock.clockNS);
		fclose(fptr);
		incrementClock(shmobj(), msg_t.msgclock.clockSecs, msg_t.msgclock.clockNS);
		
		sleep(1);
		
		processNum++;
		if (processNum > TOTAL_PRO){
			printf("OSS: Reached process limit.\n");
			break;
		}
	}
	logexit();
	return -1;
}

int getLast(int array[MAX_PRO]){
	int i;
	for (i = 0; i < MAX_PRO; i++){
		if (array[i] < 1) {
			return i;
		}
	}
	return -1;
}

/* Log
	fptr = fopen(logName, "a");
	fprintf(fptr, "");
	fclose(fptr);
*/

// Logs termination time
void logexit(){
	FILE* fptr;
	char timeBuffer[40];
	time_t tempTime = time(NULL);
	fptr = fopen(logName, "a");
	strftime(timeBuffer, 40, "%H:%M:%S", localtime(&tempTime));
	fprintf(fptr, "%s %d terminated\n", timeBuffer, getpid());
	fclose(fptr);
}

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
	shmp->ossclock.clockSecs = 0;
	shmp->ossclock.clockNS = 0;
	//shmp->processTable[18];
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