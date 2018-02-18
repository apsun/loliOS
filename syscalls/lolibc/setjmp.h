#ifndef _LOLIBC_SETJMP_H
#define _LOLIBC_SETJMP_H

typedef struct {
    int eip;
    int esp;
    int ebp;
    int ebx;
    int esi;
    int edi;
} jmp_buf;

void longjmp(jmp_buf env, int status);
int __setjmp_ptr(jmp_buf *env);
#define setjmp(env) __setjmp_ptr(&(env))

#endif /* _LOLIBC_SETJMP_H */
