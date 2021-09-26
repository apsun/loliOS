#define ASM 1

#include <syscall.h>

.globl syscall_bottom;
syscall_bottom:
    pushl   %ebx;              \
    pushl   %esi;              \
    pushl   %edi;              \
    movl    16(%esp), %ebx;    \
    movl    20(%esp), %ecx;    \
    movl    24(%esp), %edx;    \
    movl    28(%esp), %esi;    \
    movl    32(%esp), %edi;    \
    int     $0x80;             \
    popl    %edi;              \
    popl    %esi;              \
    popl    %ebx;              \
    ret;

#define MAKE_SYS(name, number) \
.globl name;                   \
name:                          \
    movl    $number, %eax;     \
    jmp     syscall_bottom;

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
MAKE_SYS(sigmask, SYS_SIGMASK)
MAKE_SYS(kill, SYS_KILL)
MAKE_SYS(ioctl, SYS_IOCTL)
MAKE_SYS(sbrk, SYS_SBRK)
MAKE_SYS(socket, SYS_SOCKET)
MAKE_SYS(bind, SYS_BIND)
MAKE_SYS(connect, SYS_CONNECT)
MAKE_SYS(listen, SYS_LISTEN)
MAKE_SYS(accept, SYS_ACCEPT)
MAKE_SYS(recvfrom, SYS_RECVFROM)
MAKE_SYS(sendto, SYS_SENDTO)
MAKE_SYS(shutdown, SYS_SHUTDOWN)
MAKE_SYS(getsockname, SYS_GETSOCKNAME)
MAKE_SYS(getpeername, SYS_GETPEERNAME)
MAKE_SYS(dup, SYS_DUP)
MAKE_SYS(fork, SYS_FORK)
MAKE_SYS(exec, SYS_EXEC)
MAKE_SYS(wait, SYS_WAIT)
MAKE_SYS(getpid, SYS_GETPID)
MAKE_SYS(getpgrp, SYS_GETPGRP)
MAKE_SYS(setpgrp, SYS_SETPGRP)
MAKE_SYS(tcgetpgrp, SYS_TCGETPGRP)
MAKE_SYS(tcsetpgrp, SYS_TCSETPGRP)
MAKE_SYS(pipe, SYS_PIPE)
MAKE_SYS(create, SYS_CREATE)
MAKE_SYS(fcntl, SYS_FCNTL)
MAKE_SYS(yield, SYS_YIELD)
MAKE_SYS(seek, SYS_SEEK)
MAKE_SYS(truncate, SYS_TRUNCATE)
MAKE_SYS(unlink, SYS_UNLINK)
MAKE_SYS(stat, SYS_STAT)
MAKE_SYS(realtime, SYS_REALTIME)
MAKE_SYS(monotime, SYS_MONOTIME)
MAKE_SYS(monosleep, SYS_MONOSLEEP)
MAKE_SYS(fbmap, SYS_FBMAP)
MAKE_SYS(fbunmap, SYS_FBUNMAP)
MAKE_SYS(fbflip, SYS_FBFLIP)

.globl _start
_start:
    subl    $24, %esp /* Prevent page faults from syscall wrapper */
    call    main
    pushl   %eax
    call    exit

/* Disable compatibility mode */
.section ".note.lolios.ident", "a"
    .align 4
    .long note_name_bottom - note_name
    .long 0
    .long 1337
note_name:
    .asciz "loliOS"
note_name_bottom:
    .align 4
