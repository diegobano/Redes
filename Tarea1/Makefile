CC=gcc
CFLAGS=-g -Wall

all: bwcs

bwcs: bwcs.o Data-tcp.o jsocket6.4.o Data.h jsocket6.4.h 
	$(CC) $(CFLAGS) bwcs.o jsocket6.4.o Data-tcp.o -o $@ -lpthread

clean:
	rm -f *.o
	rm -f bwcs
