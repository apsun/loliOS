CFLAGS += -Wall -Wextra -ffreestanding -fno-stack-protector -m32 -O2
ASFLAGS += -Wall -Wextra -m32
CC = gcc
SRC = $(wildcard *.c) $(wildcard *.S)
OBJ =
OBJ += $(patsubst %.S,%.o,$(filter %.S,$(SRC)))
OBJ += $(patsubst %.c,%.o,$(filter %.c,$(SRC)))

.PHONY: all
all: $(OBJ)

.PHONY: clean
clean:
	$(RM) *.o
