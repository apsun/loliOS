CFLAGS += \
	-std=gnu90 \
	-m32 -march=i386 \
	-Wall -Wextra -Werror \
	-Wno-unused-parameter \
	-Wshadow \
	-Wpointer-arith \
	-ffreestanding \
	-fno-builtin \
	-fno-strict-aliasing \
	-fno-pie \
	-fno-stack-protector \
	-fno-asynchronous-unwind-tables \
	-fno-omit-frame-pointer
CPPFLAGS += -nostdinc
ASFLAGS += -m32
LDFLAGS += -nostdlib -static -melf_i386

# This generates the list of source files
SRC = $(wildcard *.S) $(wildcard *.c) $(wildcard */*.S) $(wildcard */*.c)

# This generates the list of .o files. The order matters, boot.o must be first
OBJS = boot.o
OBJS += $(filter-out boot.o,$(patsubst %.S,%.o,$(filter %.S,$(SRC))))
OBJS += $(patsubst %.c,%.o,$(filter %.c,$(SRC)))

bootimg: Makefile $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -Tmemory.lds -o $@

Makefile.dep: $(SRC)
	$(CC) -MM $(CPPFLAGS) $(SRC) > $@

.PHONY: dep
dep: Makefile.dep

.PHONY: clean
clean:
	rm -f *.o */*.o Makefile.dep
	rm -f bootimg

ifneq ($(MAKECMDGOALS),dep)
ifneq ($(MAKECMDGOALS),clean)
include Makefile.dep
endif
endif
