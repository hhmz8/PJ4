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
#include <signal.h>
#include <sys/msg.h>
#include <time.h>
#include <ctype.h> //isprint
#include <unistd.h> //sleep
#include "structs.h"

#define CPU_PCT 70
#define CPU_BLOCK 10
#define IO_BLOCK 60
#define MAX_NS 50000000 // Max run time in NS 

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
	
	struct shmseg *shmp;
    int shmid = shmget(SHM_KEY, BUF_SIZE, 0666|IPC_CREAT);
	if (shmid == -1) {
		perror("Error: shmget");
		exit(-1);
	}
	
	shmp = shmat(shmid, 0, 0);	
	
	int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
	
	 // CPU / IO
	if ((rand() % 100) < CPU_PCT){
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
		if ((rand() % 100) > blockChance || (maxRunTime - currentTime) < 1000){ // Don't block is remaining time is less than 1000 NS
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
		
		// Wait for message of type pid
		msgrcv(msgid, &msg_t, sizeof(msg_t), pid, 0);
		
		// Set message
		msg_t.mtype = 1;
		msg_t.queueType = queueType;
		msg_t.msgclock.clockSecs = 0;
		msg_t.msgclock.clockNS = runTime;
		
		// Send message
		msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
	}
	
	//shm
	shmp->numberProcesses--;
	kill(pid, SIGKILL);
	exit(0);
}