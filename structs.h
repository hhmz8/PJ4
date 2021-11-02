#ifndef STRUCTS_H
#define STRUCTS_H

struct clock {
	int clockSecs;
	int clockNS;
}

struct process {
	int pid;
	int blockTime;
}

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