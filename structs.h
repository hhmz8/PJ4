#ifndef STRUCTS_H
#define STRUCTS_H
#define BUF_SIZE 1024
#define SHM_KEY 806040
#define MSG_KEY 806041

struct clock {
	int clockSecs;
	int clockNS;
};

struct process {
	int pid;
	int blockTime;
};

// Process control block
struct shmseg {
	char buf[BUF_SIZE];
   
	// Clock
	struct clock ossclock;
   
	// Process table
	struct process processTable[18];
};

struct msgbuf {
	long type;
	int queueType;
	struct clock msgclock;
} msg_t;

#endif