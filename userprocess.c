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
#define MAX_NS 9000000

// Reference: https://www.tutorialspoint.com/c_standard_library/c_function_rand.htm
int main(int argc, char** argv){
	pid_t pid = getpid();
	time_t t;
	srand((unsigned) time(&t) ^ (getpid()<<16)); // Reference: https://stackoverflow.com/questions/8623131/why-is-rand-not-so-random-after-fork
	int processType;
	int blockChance;
	int queueType;
	
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
	
	// Block
	if ((rand() % 100) < blockChance){
		queueType = 0;
	}
	else {
		queueType = 2;
	}
	
	int runtime = rand() % MAX_NS;
	int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
	
	// Wait for message of type pid
	msgrcv(msgid, &msg_t, sizeof(msg_t), pid, 0);
	
	// Set message
	msg_t.mtype = 1;
	msg_t.queueType = queueType;
	msg_t.msgclock.clockSecs = 0;
	msg_t.msgclock.clockNS = runtime;
	msg_t.maxNS = runtime;
	
	// Send message
	msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
	return 0;
}