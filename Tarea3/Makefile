CC=gcc
CFLAGS=-g -Wall

all: bwcs

bwcs: bwcs.o Data-tcp.o jsocket6.4.o bufbox.o Data.h jsocket6.4.h bufbox.h
	$(CC) $(CFLAGS) bwcs.o bufbox.o jsocket6.4.o Data-tcp.o  -o $@ -lpthread

clean:
	rm -f *.o
	rm -f bwcs
