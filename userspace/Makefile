CFLAGS += -Ilolibc -Wall -Wextra -Werror -Wno-unused-parameter \
	-Wshadow -Wpointer-arith \
	-fno-builtin -fno-stack-protector -fno-omit-frame-pointer \
	-fno-asynchronous-unwind-tables -ffreestanding \
	-m32 -mno-sse
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

elf build:
	mkdir -p $@

.PRECIOUS: elf/missiles
elf/missiles: $(MISSILES_OBJ) $(LOLIBC_OBJ) | elf
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: elf/fish
elf/fish: $(FISH_OBJ) $(LOLIBC_OBJ) | elf
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: elf/%
elf/%: %.o $(LOLIBC_OBJ) | elf
	$(LD) $(LDFLAGS) -o $@ $^

build/%: elf/% | build
	../elfconvert $<
	mv $<.converted $@

.PHONY: clean
clean:
	$(RM) *.o */*.o
	$(RM) -r build
	$(RM) -r elf
