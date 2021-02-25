CC=gcc
CFLAGS=-Wall -g -lpthread

all: bus station-manager comptroller mystation

bus: bus.c
	$(CC) bus.c -o bus $(CFLAGS)

station-manager: station-manager.c
	$(CC) station-manager.c -o station-manager $(CFLAGS)

comptroller: comptroller.c
	$(CC) comptroller.c -o comptroller $(CFLAGS)

mystation: mystation.c
	$(CC) mystation.c -o mystation $(CFLAGS)

.PHONY: clean

clean:
	rm -f bus station-manager comptroller mystation