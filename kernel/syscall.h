#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"
#include "idt.h"

#define SYS_HALT        1
#define SYS_EXECUTE     2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_GETARGS     7
#define SYS_VIDMAP      8
#define SYS_SIGACTION   9
#define SYS_SIGRETURN   10
#define SYS_SIGMASK     11
#define SYS_KILL        12
#define SYS_IOCTL       13
#define SYS_SBRK        15
#define SYS_SOCKET      16
#define SYS_BIND        17
#define SYS_CONNECT     18
#define SYS_LISTEN      19
#define SYS_ACCEPT      20
#define SYS_RECVFROM    21
#define SYS_SENDTO      22
#define SYS_SHUTDOWN    23
#define SYS_GETSOCKNAME 24
#define SYS_GETPEERNAME 25
#define SYS_DUP         26
#define SYS_FORK        27
#define SYS_EXEC        28
#define SYS_WAIT        29
#define SYS_GETPID      30
#define SYS_GETPGRP     31
#define SYS_SETPGRP     32
#define SYS_TCGETPGRP   33
#define SYS_TCSETPGRP   34
#define SYS_PIPE        35
#define SYS_CREATE      36
#define SYS_FCNTL       37
#define SYS_SEEK        39
#define SYS_TRUNCATE    40
#define SYS_UNLINK      41
#define SYS_STAT        42
#define SYS_REALTIME    43
#define SYS_MONOTIME    44
#define SYS_SLEEP       45
#define SYS_FBMAP       46
#define SYS_FBUNMAP     47
#define SYS_FBFLIP      48
#define SYS_POLL        49
#define NUM_SYSCALL     49

#ifndef ASM

__cdecl int
syscall_handle(
    intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e,
    int_regs_t *regs, int num);

#endif /* ASM */

#endif /* _SYSCALL_H */
