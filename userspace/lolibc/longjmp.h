#ifndef _LOLIBC_LONGJMP
#define _LOLIBC_LONGJMP

#include "types.h"

typedef struct {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
} jmp_buf;

void longjmp(jmp_buf env, int32_t status);
int32_t __setjmp_ptr(jmp_buf *env);
#define setjmp(env) __setjmp_ptr(&(env))

#endif /* _LOLIBC_LONGJMP */
