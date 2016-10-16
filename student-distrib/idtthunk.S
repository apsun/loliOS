#define ASM 1
#include "idt.h"

/* Interrupt handler thunk without error code */
#define MAKE_THUNK(code, name)                   \
    .globl name;                                 \
    name:                                        \
        pushl $0; /* Dummy value */              \
        pushl $ code;                            \
        jmp handle_int_thunk_common

/* Interrupt handler thunk with error code */
#define MAKE_THUNK_WITH_ERR(code, name)          \
    .globl name;                                 \
    name:                                        \
        pushl $ code;                            \
        jmp handle_int_thunk_common

.text

/* Generate interrupt handlers */
MAKE_THUNK         (EXC_DE, handle_exc_de)
MAKE_THUNK         (EXC_DB, handle_exc_db)
MAKE_THUNK         (EXC_NI, handle_exc_ni)
MAKE_THUNK         (EXC_BP, handle_exc_bp)
MAKE_THUNK         (EXC_OF, handle_exc_of)
MAKE_THUNK         (EXC_BR, handle_exc_br)
MAKE_THUNK         (EXC_UD, handle_exc_ud)
MAKE_THUNK         (EXC_NM, handle_exc_nm)
MAKE_THUNK_WITH_ERR(EXC_DF, handle_exc_df)
MAKE_THUNK         (EXC_CO, handle_exc_co)
MAKE_THUNK_WITH_ERR(EXC_TS, handle_exc_ts)
MAKE_THUNK_WITH_ERR(EXC_NP, handle_exc_np)
MAKE_THUNK_WITH_ERR(EXC_SS, handle_exc_ss)
MAKE_THUNK_WITH_ERR(EXC_GP, handle_exc_gp)
MAKE_THUNK_WITH_ERR(EXC_PF, handle_exc_pf)
MAKE_THUNK         (EXC_RE, handle_exc_re)
MAKE_THUNK         (EXC_MF, handle_exc_mf)
MAKE_THUNK_WITH_ERR(EXC_AC, handle_exc_ac)
MAKE_THUNK         (EXC_MC, handle_exc_mc)
MAKE_THUNK         (EXC_XF, handle_exc_xf)
MAKE_THUNK         (INT_IRQ0, handle_int_irq0)
MAKE_THUNK         (INT_IRQ1, handle_int_irq1)
MAKE_THUNK         (INT_IRQ2, handle_int_irq2)
MAKE_THUNK         (INT_IRQ3, handle_int_irq3)
MAKE_THUNK         (INT_IRQ4, handle_int_irq4)
MAKE_THUNK         (INT_IRQ5, handle_int_irq5)
MAKE_THUNK         (INT_IRQ6, handle_int_irq6)
MAKE_THUNK         (INT_IRQ7, handle_int_irq7)
MAKE_THUNK         (INT_IRQ8, handle_int_irq8)
MAKE_THUNK         (INT_IRQ9, handle_int_irq9)
MAKE_THUNK         (INT_IRQ10, handle_int_irq10)
MAKE_THUNK         (INT_IRQ11, handle_int_irq11)
MAKE_THUNK         (INT_IRQ12, handle_int_irq12)
MAKE_THUNK         (INT_IRQ13, handle_int_irq13)
MAKE_THUNK         (INT_IRQ14, handle_int_irq14)
MAKE_THUNK         (INT_IRQ15, handle_int_irq15)
MAKE_THUNK         (INT_SYSCALL, handle_int_syscall)
MAKE_THUNK         (INT_UNKNOWN, handle_int_unknown)

/* Common interrupt handler routine */
handle_int_thunk_common:
    /*
     * Save all registers. The order must correspond to the
     * ordering in int_regs_t, since the stack is interpreted
     * as an instance of that struct.
     */
    pushw %gs
    pushw %fs
    pushw %es
    pushw %ds
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax

    /* Call the actual interrupt handler routine */
    pushl %esp
    call handle_interrupt
    addl $4, %esp

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

    /* Pop IRQ vector number and error code */
    addl $8, %esp

    /* Return from interrupt */
    iret
