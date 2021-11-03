/*
Hanzhe Huang
11/1/2021
userprocess.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <time.h>
#include <ctype.h> //isprint
#include <unistd.h> //sleep
#include "structs.h"

#define EXIT_PCT 10
#define CPU_PCT 60
#define CPU_BLOCK 10
#define IO_BLOCK 70
#define MAX_NS 500000000

// Reference: https://www.tutorialspoint.com/c_standard_library/c_function_rand.htm
int main(int argc, char** argv){
	pid_t pid = getpid();
	time_t t;
	srand((unsigned) time(&t) ^ (getpid()<<16)); // Reference: https://stackoverflow.com/questions/8623131/why-is-rand-not-so-random-after-fork
	int processType;
	int blockChance;
	int queueType;
	int maxRunTime = rand() % MAX_NS;
	int currentTime = 0;
	int runTime;
	
	// Termination
	if ((rand() % 100) < EXIT_PCT){
		msg_t.queueType = -1;
	}
	
	 // CPU / IO
	else if ((rand() % 100) < CPU_PCT){
		processType = 1;
	}
	else {
		processType = 0;
	}
	
	if (processType == 0){
		blockChance = IO_BLOCK;
	}
	else {
		blockChance = CPU_BLOCK;
	}
	
	while (currentTime < maxRunTime){
		// Block
		if ((rand() % 100) < blockChance || (maxRunTime - currentTime) < 100){ // Don't block is remaining time is less than 100 NS
			queueType = 0;
			runTime = maxRunTime - currentTime;
			currentTime = maxRunTime;
		}
		else {
			queueType = 2;
			runTime = (rand() % ((maxRunTime - 1) - currentTime));
			if (runTime == 0){ // Time ran cannot be 0
				runTime++;
			}
			currentTime += runTime;
		}
		
		int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
		
		printf("%d WAITING\n", pid);
		// Wait for message of type pid
		msgrcv(msgid, &msg_t, sizeof(msg_t), pid, 0);
		printf("%d FINISHED WAITING. %d:%d\n", pid, currentTime, maxRunTime);
		
		// Set message
		msg_t.mtype = 1;
		msg_t.queueType = queueType;
		msg_t.msgclock.clockSecs = 0;
		msg_t.msgclock.clockNS = runTime;
		msg_t.maxNS = maxRunTime;
		
		// Send message
		msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
	}
	return 0;
}