#define ASM 1
/* 
 * Pop the context of next process.
 * When interrupt occur in kernel, only eip, cs and eflags
 * are pushed onto stack and IRET only pop these three things
 * since there is no privilege change.
 */
.globl sched_kernel_interrupt_thunk;
sched_kernel_interrupt_thunk:
    /* Restore control registers */
    popl %eax
    movl %eax, %cr0
    popl %eax
    movl %eax, %cr2
    popl %eax
    movl %eax, %cr3
    popl %eax
    movl %eax, %cr4

    /* Restore all registers */    
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    popl %esi
    popl %edi
    popl %ebp
    popw %ds
    popw %es
    popw %fs
    popw %gs

    /* Pop IRQ vector number and esp */
    addl $8, %esp

    /* Return from interrupt */
    iret
