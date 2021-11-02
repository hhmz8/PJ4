/*
Hanzhe Huang
10/20/2021
runsim.h
*/

#ifndef OSS_H
#define OSS_H

void logexit();
void sigint_parent(int sig);
void sigint(int sig);
void sigalrm(int sig);

void parent();
void child(int id);
void deallocate();

struct shmseg* shmobj();
void initshmobj(struct shmseg* shmp);

#define BUF_SIZE 1024
#define SHM_KEY 806040
#define MSG_KEY 806041
#define MAX_CHAR 20
#define MAX_LINES 10000
#define MAX_PRO 20
#define MAX_TIME 3

#endif