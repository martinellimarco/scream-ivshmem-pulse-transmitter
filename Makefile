CC=gcc
CFLAGS = -Wall

scream-pulse: scream-ivshmem-pulse-transmitter.c
	$(CC) $(CFLAGS) -o scream-ivshmem-pulse-transmitter scream-ivshmem-pulse-transmitter.c -lpulse-simple -lpulse
