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

int main(int argc, char** argv){
	pid_t pid = getpid();
	int runtime = 5000000;
	int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
	
	// Wait for message of type pid
	msgrcv(msgid, &msg_t, sizeof(msg_t), pid, 0);
	printf("Message recieved at child.\n");
	
	// Set message
	msg_t.mtype = 1;
	msg_t.queueType = 0;
	msg_t.msgclock.clockSecs = 0;
	msg_t.msgclock.clockNS = runtime;
	
	// Send message
	msgsnd(msgid, &msg_t, sizeof(msg_t), 0);
	return 0;
}