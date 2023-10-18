CC=gcc

all: rectwm.c
	$(CC) rectwm.c -lX11 -o rectwm
