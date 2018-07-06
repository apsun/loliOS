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
#define SYS_TIME        14
#define SYS_SBRK        15
#define SYS_SOCKET      16
#define SYS_BIND        17
#define SYS_CONNECT     18
#define SYS_LISTEN      19
#define SYS_ACCEPT      20
#define SYS_RECVFROM    21
#define SYS_SENDTO      22
#define SYS_GETSOCKNAME 23
#define SYS_GETPEERNAME 24
#define SYS_DUP         25
#define SYS_FORK        26
#define SYS_EXEC        27
#define SYS_WAIT        28
#define SYS_GETPID      29
#define SYS_GETPGRP     30
#define SYS_SETPGRP     31
#define SYS_TCGETPGRP   32
#define SYS_TCSETPGRP   33
#define SYS_PIPE        34
#define NUM_SYSCALL     34

#define EINTR  2
#define EAGAIN 3

#ifndef ASM

__cdecl int
syscall_handle(
    uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e,
    int_regs_t *regs, int num);

#endif /* ASM */

#endif /* _SYSCALL_H */
