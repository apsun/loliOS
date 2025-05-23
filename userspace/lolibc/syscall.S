#define ASM 1

#include <syscall.h>

.text

syscall_bottom:
    pushl   %ebx
    pushl   %esi
    pushl   %edi
    movl    16(%esp), %ebx
    movl    20(%esp), %ecx
    movl    24(%esp), %edx
    movl    28(%esp), %esi
    movl    32(%esp), %edi
    int     $0x80
    popl    %edi
    popl    %esi
    popl    %ebx
    ret
.type syscall_bottom, %function
.size syscall_bottom, .-syscall_bottom

#define MAKE_SYS(name, number)     \
    .globl name;                   \
    name:                          \
        movl    $number, %eax;     \
        jmp     syscall_bottom;    \
    .type name, %function;         \
    .size name, .-name

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
MAKE_SYS(seek, SYS_SEEK)
MAKE_SYS(truncate, SYS_TRUNCATE)
MAKE_SYS(unlink, SYS_UNLINK)
MAKE_SYS(stat, SYS_STAT)
MAKE_SYS(realtime, SYS_REALTIME)
MAKE_SYS(monotime, SYS_MONOTIME)
MAKE_SYS(sleep, SYS_SLEEP)
MAKE_SYS(fbmap, SYS_FBMAP)
MAKE_SYS(fbunmap, SYS_FBUNMAP)
MAKE_SYS(fbflip, SYS_FBFLIP)
MAKE_SYS(poll, SYS_POLL)

.globl _start
_start:
    subl    $24, %esp /* Prevent page faults from syscall wrapper */
    call    main
    pushl   %eax
    call    exit
.type _start, %function
.size _start, .-_start

/* Disable compatibility mode */
.section ".note.lolios.ident", "a"
    .align 4
    .long .Lnote_name_bottom - .Lnote_name
    .long 0
    .long 1337
.Lnote_name:
    .asciz "loliOS"
.Lnote_name_bottom:
    .align 4
