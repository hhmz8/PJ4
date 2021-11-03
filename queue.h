/*
Hanzhe Huang
11/2/2021
queue.h
*/

#ifndef QUEUE_H
#define QUEUE_H

void initQueue(int* array, int size);
int getLast(int array[], int size);
int enqueue(int* array, int size, int item);
int dequeue(int* array, int size);

#endif