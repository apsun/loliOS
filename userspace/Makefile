CFLAGS += \
	-std=gnu90 \
	-m32 -march=i386 -mno-80387 \
	-Wall -Wextra -Werror \
	-Wno-unused-parameter \
	-Wshadow \
	-Wpointer-arith \
	-Wwrite-strings \
	-ffreestanding \
	-fno-builtin \
	-fno-strict-aliasing \
	-fno-pie \
	-fno-stack-protector \
	-fno-asynchronous-unwind-tables \
	-fno-omit-frame-pointer \
	-static \
	-nostdlib
CPPFLAGS += -MMD -MP -nostdinc -Ilolibc
ASFLAGS += -m32

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
	$(CC) $(CFLAGS) -o $@ $^

.PRECIOUS: build/fish
build/fish: $(FISH_OBJ) $(LOLIBC_OBJ) | build
	$(CC) $(CFLAGS) -o $@ $^

.PRECIOUS: build/%
build/%: %.o $(LOLIBC_OBJ) | build
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) *.o */*.o
	$(RM) *.d */*.d
	$(RM) -r build

-include $(LOLIBC_OBJ:%.o=%.d)
-include $(MISSILES_OBJ:%.o=%.d)
-include $(FISH_OBJ:%.o=%.d)
