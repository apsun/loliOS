# Flags to use when compiling, preprocessing, assembling, and linking
CFLAGS += -Wall -Wextra -Werror -Wno-unused-parameter \
	-Wshadow -Wpointer-arith \
	-fno-builtin -fno-stack-protector -fno-omit-frame-pointer \
	-fno-asynchronous-unwind-tables -ffreestanding -fno-pie \
	-m32 -mno-sse
ASFLAGS += -Wall -Wextra -m32
LDFLAGS += -nostdlib -static -melf_i386

# If you have any .h files in another directory, add -I<dir> to this line
CPPFLAGS += -nostdinc -g

# This generates the list of source files
SRC = $(wildcard *.S) $(wildcard *.c) $(wildcard */*.S) $(wildcard */*.c)

# This generates the list of .o files. The order matters, boot.o must be first
OBJS = boot.o
OBJS += $(filter-out boot.o,$(patsubst %.S,%.o,$(filter %.S,$(SRC))))
OBJS += $(patsubst %.c,%.o,$(filter %.c,$(SRC)))

bootimg: Makefile $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -Tmemory.lds -o bootimg

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
