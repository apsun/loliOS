#define ASM 1

#include <syscall.h>

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
MAKE_SYS(sigaction, SYS_SIGACTION)
MAKE_SYS(sigreturn, SYS_SIGRETURN)
MAKE_SYS(sigraise, SYS_SIGRAISE)
MAKE_SYS(sigmask, SYS_SIGMASK)
MAKE_SYS(ioctl, SYS_IOCTL)
MAKE_SYS(time, SYS_TIME)
MAKE_SYS(sbrk, SYS_SBRK)

.globl _start
_start:
    subl    $16, %esp /* Prevents page faults from syscalls since ESP is at end of user page */
    call    main
    pushl   %eax
    call    exit
