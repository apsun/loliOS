#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"
#include "idt.h"

#define NUM_SYSCALL     15

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

#ifndef ASM

__cdecl int32_t
syscall_handle(uint32_t a, uint32_t b, uint32_t c, int_regs_t *regs, uint32_t num);

#endif /* ASM */

#endif /* _SYSCALL_H */
