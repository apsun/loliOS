#ifndef _LOLIBC_SETJMP_H
#define _LOLIBC_SETJMP_H

#include <stdint.h>

typedef struct {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
} jmp_buf;

__attribute__((cdecl, noreturn)) void longjmp(jmp_buf env, int status);
__attribute__((cdecl)) int __setjmp_ptr(jmp_buf *env);
#define setjmp(env) __setjmp_ptr(&(env))

#endif /* _LOLIBC_SETJMP_H */
