#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"

#define NUM_SYSCALL     10

#define SYS_HALT        1
#define SYS_EXECUTE     2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_GETARGS     7
#define SYS_VIDMAP      8
#define SYS_SET_HANDLER 9
#define SYS_SIGRETURN   10

#ifndef ASM

#define __cdecl __attribute__((cdecl))

int32_t syscall_handle(uint32_t a, uint32_t b, uint32_t c, uint32_t num);

#endif /* ASM */

#endif /* _SYSCALL_H */
