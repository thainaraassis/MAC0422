CC = gcc
CFLAGS = -Wall -g
READLINE = -lreadline
PTHREAD = -lpthread

all: uspsh ep1

uspsh: uspsh.c uspsh.h
	$(CC) $(CFLAGS) -o uspsh uspsh.c $(READLINE)

ep1: ep1.c ep1.h
	$(CC) $(CFLAGS) -o ep1 ep1.c $(PTHREAD)

clean:
	rm -f *.o uspsh ep1