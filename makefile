CC = gcc
CFLAGS = -Wall
OBJ = userprocess.o oss.o

all: oss userprocess

oss.o: oss.c oss.h structs.h
	$(CC) $(CFLAGS) -c -o $@ $<
	
userprocess.o: userprocess.c structs.h
	$(CC) $(CFLAGS) -c -o $@ $<
	
oss: oss.o
	$(CC) $(CFLAGS) -o $@ $^
	
userprocess: userprocess.o
	$(CC) $(CFLAGS) -o $@ $^
	
clean:
	rm *.h *.c *.o