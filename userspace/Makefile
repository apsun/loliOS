CFLAGS += -Wall -Wextra -ffreestanding -fno-stack-protector -O2 -m32
ASFLAGS += -Wall -Wextra -m32
LDFLAGS += -nostdlib -static -m32
LOLIBC_OBJ = $(patsubst %.c,%.o,$(wildcard lolibc/*.c))
LOLIBC_OBJ += $(patsubst %.S,%.o,$(wildcard lolibc/*.S))
ELF_BIN = $(patsubst %.c,elf/%,$(wildcard *.c))
OUT_BIN = $(patsubst elf/%,out/%,$(ELF_BIN))

.PHONY: all
all: $(OUT_BIN)

elf/%: %.o $(LOLIBC_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

out/%: elf/%
	../elfconvert $<
	mv $<.converted $@

.PHONY: clean
clean:
	$(RM) *.o
	$(RM) $(LOLIBC_OBJ)
	$(RM) $(ELF_BIN)
	$(RM) $(OUT_BIN)
