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

#define MAX_PRO 20

#endif