CFLAGS += -std=gnu90 -Wall -Wextra -Werror \
	-Wno-unused-parameter -Wshadow -Wpointer-arith \
	-fno-builtin -fno-stack-protector -fno-omit-frame-pointer \
	-fno-asynchronous-unwind-tables -fno-strict-aliasing \
	-ffreestanding -fno-pie \
	-m32 -march=i386 -mno-sse -nostdinc -Ilolibc
ASFLAGS += -Wall -Wextra -m32 -Ilolibc
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

build:
	mkdir -p $@

.PRECIOUS: build/missiles
build/missiles: $(MISSILES_OBJ) $(LOLIBC_OBJ) | build
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: build/fish
build/fish: $(FISH_OBJ) $(LOLIBC_OBJ) | build
	$(LD) $(LDFLAGS) -o $@ $^

.PRECIOUS: build/%
build/%: %.o $(LOLIBC_OBJ) | build
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) *.o */*.o
	$(RM) -r build
