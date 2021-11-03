#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

// Fixed size queue functions
void initQueue(int* array, int size){
	int i;
	for (i = 0; i < size; i++){
		array[i] = 0;
	}
}

int getLast(int array[], int size){
	int i;
	for (i = 0; i < size; i++){
		if (array[i] == 0) {
			return i;
		}
	}
	return -1;
}

int enqueue(int* array, int size, int item){
	if (getLast(array, size) != -1) {
		array[getLast(array, size)] = item;
		return 0;
	}
	return -1;
}

int dequeue(int* array, int size){
	if (array[0] == 0) {
		return -1;
	}
	int i;
	int item = array[0];
	array[0] = 0;
	for (i = 1; i < size; i++){
		if (array[i] != 0) {
			array[i-1] = array[i];
			array[i] = 0;
		}
	}
	return item;
}