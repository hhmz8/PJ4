/*
Hanzhe Huang
10/20/2021
runsim.h
*/

#ifndef OSS_H
#define OSS_H
#define MAX_PRO 18
#define TOTAL_PRO 1

int getLast(int array[MAX_PRO]);

void logexit();
void sigint_parent(int sig);
void sigint(int sig);
void sigalrm(int sig);

void parent();
void child();
void deallocate();
void incrementClock(struct shmseg* shmp, int incS, int incNS);

struct shmseg* shmobj();
void initshmobj(struct shmseg* shmp);


#endif