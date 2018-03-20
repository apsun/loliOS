#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"
#include "idt.h"

#define NUM_SYSCALL     22

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
#define SYS_SIGRAISE    11
#define SYS_SIGMASK     12
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

#ifndef ASM

__cdecl int
syscall_handle(
    uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e,
    int_regs_t *regs, int num);

#endif /* ASM */

#endif /* _SYSCALL_H */
