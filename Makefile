CFLAGS = -Wall -pedantic
LFLAGS = -lX11
BINDIR = /usr/bin
CC=gcc

all: rectwm

rectwm: rectwm.c
	$(CC) $(CFLAGS) $(LFLAGS) rectwm.c -o rectwm

install: all
	install -D rectwm $(BINDIR)/rectwm

uninstall: 
	rm -f $(BINDIR)/rectwm

clean:
	rm rectwm
