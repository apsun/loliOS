#ifndef _LOLIBC_SETJMP_H
#define _LOLIBC_SETJMP_H

#include <stdint.h>

#define __cdecl __attribute__((cdecl))

typedef struct {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
} jmp_buf;

__cdecl void longjmp(jmp_buf env, int status);
__cdecl int __setjmp_ptr(jmp_buf *env);
#define setjmp(env) __setjmp_ptr(&(env))

#endif /* _LOLIBC_SETJMP_H */
