CFLAGS += -Wall -Wextra -nostdlib -ffreestanding
LDFLAGS += -nostdlib -ffreestanding
CC = gcc

ALL: exit.o io.o rand.o string.o sys.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -Wall -o $@ $<
