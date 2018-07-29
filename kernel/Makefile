# Flags to use when compiling, preprocessing, assembling, and linking
CFLAGS += -Wall -Wextra -Werror -Wno-unused-parameter \
	-Wshadow -Wpointer-arith \
	-fno-builtin -fno-stack-protector -fno-omit-frame-pointer \
	-ffreestanding -O0 -m32 -mno-sse
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
	rm -f bootimg
	$(LD) $(LDFLAGS) $(OBJS) -Ttext=0x400000 -o bootimg
	sudo ./debug.sh

dep: Makefile.dep

Makefile.dep: $(SRC)
	$(CC) -MM $(CPPFLAGS) $(SRC) > $@

.PHONY: clean
clean:
	rm -f *.o */*.o Makefile.dep
	rm -f bootimg
	rm -f mp3.img
	rm -f filesys_img.new

ifneq ($(MAKECMDGOALS),dep)
ifneq ($(MAKECMDGOALS),clean)
include Makefile.dep
endif
endif
