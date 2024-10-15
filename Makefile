CFLAGS = -Wall -pedantic
LFLAGS = -lX11
CC=gcc

all: rectwm

rectwm: rectwm.c
	$(CC) $(CFLAGS) $(LFLAGS) rectwm.c -o rectwm

clean:
	rm rectwm
