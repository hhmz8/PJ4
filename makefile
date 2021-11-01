CC = gcc
CFLAGS = -Wall
OBJ = testsim.o oss.o

all: oss testsim

oss.o: oss.c oss.h
	$(CC) $(CFLAGS) -c -o $@ $<
	
testsim.o: testsim.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
oss: oss.o
	$(CC) $(CFLAGS) -o $@ $^
	
testsim: testsim.o
	$(CC) $(CFLAGS) -o $@ $^
	
clean:
	rm *.h *.c *.o