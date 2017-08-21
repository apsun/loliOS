#define ASM 1

#include "sys.h"

#define MAKE_SYS(name, number) \
.globl name;                   \
name:                          \
    pushl   %ebx;              \
    movl    $number, %eax;     \
    movl    8(%esp), %ebx;     \
    movl    12(%esp), %ecx;    \
    movl    16(%esp), %edx;    \
    int     $0x80;             \
    popl    %ebx;              \
    ret;

MAKE_SYS(halt, SYS_HALT)
MAKE_SYS(execute, SYS_EXECUTE)
MAKE_SYS(read, SYS_READ)
MAKE_SYS(write, SYS_WRITE)
MAKE_SYS(open, SYS_OPEN)
MAKE_SYS(close, SYS_CLOSE)
MAKE_SYS(getargs, SYS_GETARGS)
MAKE_SYS(vidmap, SYS_VIDMAP)
MAKE_SYS(set_handler, SYS_SET_HANDLER)
MAKE_SYS(sigreturn, SYS_SIGRETURN)
MAKE_SYS(ioctl, SYS_IOCTL)
MAKE_SYS(time, SYS_TIME)

.globl _start
_start:
    call    main
    pushl   %eax
    call    halt
