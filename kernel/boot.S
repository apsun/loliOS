#define ASM 1

#include "multiboot.h"
#include "x86_desc.h"

.text

# Multiboot header (required for GRUB to boot us)
multiboot_header:
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long -(MULTIBOOT_HEADER_MAGIC+MULTIBOOT_HEADER_FLAGS)
.type multiboot_header, %object
.size multiboot_header, .-multiboot_header

# Entrypoint to the kernel
.globl start, _start
start:
_start:
    # Load the GDT
    lgdt    gdt_desc_ptr

    # Load CS with the new descriptor value
    ljmp    $KERNEL_CS, $.Lkeep_going

.Lkeep_going:
    # Set up ESP so we can have an initial stack
    movl    $0x800000, %esp

    # Set up the rest of the segment selector registers
    movw    $KERNEL_DS, %cx
    movw    %cx, %ss
    movw    %cx, %ds
    movw    %cx, %es
    movw    %cx, %fs
    movw    %cx, %gs

    # Push the parameters that entry() expects (see kernel.c):
    # eax = multiboot magic
    # ebx = address of multiboot info struct
    pushl   %ebx
    pushl   %eax

    # Jump to the C entrypoint to the kernel.
    call    entry

    # We'll never get back here, but we put in a hlt anyway.
.Lhalt:
    hlt
    jmp     .Lhalt
.type _start, %function
.size _start, .-_start
.type start, %function
.size start, .-start
