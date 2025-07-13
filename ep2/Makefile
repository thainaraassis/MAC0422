CC = gcc
CFLAGS = -Wall -g
PTHREAD = -lpthread

all: ep2

ep2: ep2.c ep2.h
	$(CC) $(CFLAGS) -o ep2 ep2.c $(PTHREAD)

clean:
	rm -f *.o ep2