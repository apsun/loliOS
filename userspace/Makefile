CFLAGS += -Ilolibc -Wall -Wextra -Werror -Wno-unused-parameter \
	-Wshadow -Wpointer-arith \
	-fno-builtin -fno-stack-protector -fno-omit-frame-pointer \
	-ffreestanding -O0 -m32 -mno-sse
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

.PRECIOUS: build/missiles.elf
build/missiles.elf: $(MISSILES_OBJ) $(LOLIBC_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: build/fish.elf
build/fish.elf: $(FISH_OBJ) $(LOLIBC_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: build/%.elf
build/%.elf: %.o $(LOLIBC_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

build/%: build/%.elf
	../elfconvert $<
	mv $<.converted $@

.PHONY: clean
clean:
	$(RM) *.o */*.o
	$(RM) build/*
