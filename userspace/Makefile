CFLAGS += -Ilolibc -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -O2 -m32
ASFLAGS += -Ilolibc -Wall -Wextra -m32
LDFLAGS += -nostdlib -static -melf_i386

objs = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(wildcard $(1)/*.c $(1)/*.S)))
LOLIBC_OBJ = $(call objs,lolibc)
MISSILES_OBJ = $(call objs,missiles)
FISH_OBJ = $(call objs,fish)

OUT_BIN = $(patsubst %.c,build/%,$(wildcard *.c))
OUT_BIN += build/missiles
OUT_BIN += build/fish

.PHONY: all
all: $(OUT_BIN)

.INTERMEDIATE: build/missiles.elf
build/missiles.elf: $(MISSILES_OBJ) $(LOLIBC_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

.INTERMEDIATE: build/fish.elf
build/fish.elf: $(FISH_OBJ) $(LOLIBC_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

build/%.elf: %.o $(LOLIBC_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

build/%: build/%.elf
	../elfconvert $<
	mv $<.converted $@

.PHONY: clean
clean:
	$(RM) *.o */*.o
	$(RM) build/*
