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
CPPFLAGS += -MMD -MP -nostdinc
ASFLAGS += -m32

# This generates the list of source files
SRC = $(wildcard *.S) $(wildcard *.c) $(wildcard */*.S) $(wildcard */*.c)

# This generates the list of .o files. The order matters, boot.o must be first
OBJS = boot.o
OBJS += $(filter-out boot.o,$(patsubst %.S,%.o,$(filter %.S,$(SRC))))
OBJS += $(patsubst %.c,%.o,$(filter %.c,$(SRC)))

bootimg: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -Tmemory.lds -o $@

.PHONY: clean
clean:
	$(RM) *.o */*.o
	$(RM) *.d */*.d
	$(RM) bootimg

-include $(OBJS:%.o=%.d)
