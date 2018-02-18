#define ASM 1

/*
 * void longjmp(jmp_buf env, int status)
 *
 * Note that this relies on the GCC ABI - large structs
 * will be "exploded" onto the stack in x86.
 */
.globl longjmp
longjmp:
    movl 12(%esp), %ebp
    movl 16(%esp), %ebx
    movl 20(%esp), %esi
    movl 24(%esp), %edi
    movl 28(%esp), %eax
    movl 4(%esp), %ecx
    movl 8(%esp), %esp
    movl %ecx, 0(%esp)
    ret

/*
 * int __setjmp_ptr(jmp_buf *env)
 *
 * Implementation for setjmp, since setjmp does not take
 * a pointer. Instead we have a macro "thunk" take care of
 * that for us, we just worry about filling in the struct.
 */
.globl __setjmp_ptr
__setjmp_ptr:
    movl 4(%esp), %eax
    movl 0(%esp), %ecx
    movl %ecx, 0(%eax)
    movl %esp, 4(%eax)
    movl %ebp, 8(%eax)
    movl %ebx, 12(%eax)
    movl %esi, 16(%eax)
    movl %edi, 20(%eax)
    xorl %eax, %eax
    ret
