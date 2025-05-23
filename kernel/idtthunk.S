#define ASM 1

#include "idt.h"
#include "x86_desc.h"

.text

/* Interrupt handler thunk without error code */
#define MAKE_THUNK(num, name)                    \
    .globl name;                                 \
    name:                                        \
        pushl $0; /* Dummy value */              \
        pushl $num;                              \
        jmp idt_handle_common_thunk;             \
    .type name, %function;                       \
    .size name, .-name

/* Interrupt handler thunk with error code */
#define MAKE_THUNK_WITH_ERR(num, name)           \
    .globl name;                                 \
    name:                                        \
        pushl $num;                              \
        jmp idt_handle_common_thunk;             \
    .type name, %function;                       \
    .size name, .-name

/* Generate interrupt handler thunks */
MAKE_THUNK(EXC_DE, idt_handle_exc_de)
MAKE_THUNK(EXC_DB, idt_handle_exc_db)
MAKE_THUNK(EXC_NI, idt_handle_exc_ni)
MAKE_THUNK(EXC_BP, idt_handle_exc_bp)
MAKE_THUNK(EXC_OF, idt_handle_exc_of)
MAKE_THUNK(EXC_BR, idt_handle_exc_br)
MAKE_THUNK(EXC_UD, idt_handle_exc_ud)
MAKE_THUNK(EXC_NM, idt_handle_exc_nm)
MAKE_THUNK_WITH_ERR(EXC_DF, idt_handle_exc_df)
MAKE_THUNK(EXC_CO, idt_handle_exc_co)
MAKE_THUNK_WITH_ERR(EXC_TS, idt_handle_exc_ts)
MAKE_THUNK_WITH_ERR(EXC_NP, idt_handle_exc_np)
MAKE_THUNK_WITH_ERR(EXC_SS, idt_handle_exc_ss)
MAKE_THUNK_WITH_ERR(EXC_GP, idt_handle_exc_gp)
MAKE_THUNK_WITH_ERR(EXC_PF, idt_handle_exc_pf)
MAKE_THUNK(EXC_RE, idt_handle_exc_re)
MAKE_THUNK(EXC_MF, idt_handle_exc_mf)
MAKE_THUNK_WITH_ERR(EXC_AC, idt_handle_exc_ac)
MAKE_THUNK(EXC_MC, idt_handle_exc_mc)
MAKE_THUNK(EXC_XF, idt_handle_exc_xf)
MAKE_THUNK(INT_IRQ0, idt_handle_int_irq0)
MAKE_THUNK(INT_IRQ1, idt_handle_int_irq1)
MAKE_THUNK(INT_IRQ2, idt_handle_int_irq2)
MAKE_THUNK(INT_IRQ3, idt_handle_int_irq3)
MAKE_THUNK(INT_IRQ4, idt_handle_int_irq4)
MAKE_THUNK(INT_IRQ5, idt_handle_int_irq5)
MAKE_THUNK(INT_IRQ6, idt_handle_int_irq6)
MAKE_THUNK(INT_IRQ7, idt_handle_int_irq7)
MAKE_THUNK(INT_IRQ8, idt_handle_int_irq8)
MAKE_THUNK(INT_IRQ9, idt_handle_int_irq9)
MAKE_THUNK(INT_IRQ10, idt_handle_int_irq10)
MAKE_THUNK(INT_IRQ11, idt_handle_int_irq11)
MAKE_THUNK(INT_IRQ12, idt_handle_int_irq12)
MAKE_THUNK(INT_IRQ13, idt_handle_int_irq13)
MAKE_THUNK(INT_IRQ14, idt_handle_int_irq14)
MAKE_THUNK(INT_IRQ15, idt_handle_int_irq15)
MAKE_THUNK(INT_SYSCALL, idt_handle_int_syscall)
MAKE_THUNK(INT_UNKNOWN, idt_handle_int_unknown)

/* Common interrupt handler thunk */
idt_handle_common_thunk:
    /*
     * Save all registers. The order must correspond to the
     * ordering in int_regs_t, since the stack is interpreted
     * as an instance of that struct.
     */
    pushw %ds
    pushw %es
    pushw %fs
    pushw %gs
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp

    /* Clear direction flag */
    cld

    /* Switch to kernel data segment */
    movw $KERNEL_DS, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* Call the actual interrupt handler routine */
    pushl %esp
    call idt_handle_interrupt
    addl $4, %esp

.globl idt_unwind_stack
idt_unwind_stack:
    /* Restore all registers */
    popl %ebp
    popl %edi
    popl %esi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    popw %gs
    popw %fs
    popw %es
    popw %ds

    /* Pop interrupt number and error code */
    addl $8, %esp

    /* Return from interrupt */
    iret
.type idt_unwind_stack, %function
.size idt_unwind_stack, .-idt_unwind_stack
.type idt_handle_common_thunk, %function
.size idt_handle_common_thunk, .-idt_handle_common_thunk
