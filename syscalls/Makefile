CFLAGS += -Ilolibc -Wall -Wextra -ffreestanding -fno-stack-protector -O2 -m32
ASFLAGS += -Ilolibc -Wall -Wextra -m32
LDFLAGS += -nostdlib -static -m32

objs = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(wildcard $(1)/*.c $(1)/*.S)))
LOLIBC_OBJ = $(call objs,lolibc)
MISSILES_OBJ = $(call objs,missiles)
FISH_OBJ = $(call objs,fish)

ELF_BIN = $(patsubst %.c,to_fsdir/%.elf,$(wildcard *.c))
ELF_BIN += to_fsdir/missiles.elf
ELF_BIN += to_fsdir/fish.elf
OUT_BIN = $(patsubst to_fsdir/%.elf,to_fsdir/%,$(ELF_BIN))

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
