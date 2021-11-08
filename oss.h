/*
Hanzhe Huang
10/20/2021
runsim.h
*/

#ifndef OSS_H
#define OSS_H
#define MAX_PRO 18
#define TOTAL_PRO 100
#define MAX_DISPATCH 10000
#define MAX_UNBLOCK 20000

void logexit();
void sigint_parent(int sig);
void sigint(int sig);
void sigalrm(int sig);

void parent();
void child();
void deallocate();

int getPidIndex(struct shmseg* shmp, int pid);

struct clock mathClock(int operation, struct clock inClock, struct clock mathClock);
void incrementClockShm(struct shmseg* shmp, int incS, int incNS);
int isClockLarger(struct clock clockA, struct clock clockB);

struct shmseg* shmobj();
void initshmobj(struct shmseg* shmp);


#endif