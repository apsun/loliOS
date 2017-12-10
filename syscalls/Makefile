CFLAGS += -Ilolibc -Wall -Wextra -ffreestanding -fno-stack-protector -O3 -m32
ASFLAGS += -Ilolibc -Wall -Wextra -m32
LDFLAGS += -nostdlib -static -m32

objs = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(wildcard $(1)/*.c $(1)/*.S)))
LOLIBC_OBJ = $(call objs,lolibc)
MISSILES_OBJ = $(call objs,missiles)
FISH_OBJ = $(call objs,fish)

OUT_BIN = $(patsubst %.c,to_fsdir/%,$(wildcard *.c))
OUT_BIN += to_fsdir/missiles
OUT_BIN += to_fsdir/fish

.PHONY: all
all: $(OUT_BIN)

.INTERMEDIATE: to_fsdir/missiles.elf
to_fsdir/missiles.elf: $(MISSILES_OBJ) $(LOLIBC_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

.INTERMEDIATE: to_fsdir/fish.elf
to_fsdir/fish.elf: $(FISH_OBJ) $(LOLIBC_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

to_fsdir/%.elf: %.o $(LOLIBC_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

to_fsdir/%: to_fsdir/%.elf
	../elfconvert $<
	mv $<.converted $@

.PHONY: clean
clean:
	$(RM) *.o */*.o
	$(RM) to_fsdir/*
