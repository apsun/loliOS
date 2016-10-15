#define ASM 1
#include "idt.h"

/* Laziness is a virtue */
#define MAKE_INT_HANDLER(name, code) \
    .globl name;                     \
    name:                            \
        pushl $ code;                \
        jmp handle_int_thunk_common


.text

/* Generate interrupt handlers */
MAKE_INT_HANDLER(handle_exc_de, EXC_DE)
MAKE_INT_HANDLER(handle_exc_db, EXC_DB)
MAKE_INT_HANDLER(handle_exc_ni, EXC_NI)
MAKE_INT_HANDLER(handle_exc_bp,	EXC_BP)
MAKE_INT_HANDLER(handle_exc_of, EXC_OF)
MAKE_INT_HANDLER(handle_exc_br, EXC_BR)
MAKE_INT_HANDLER(handle_exc_ud, EXC_UD)
MAKE_INT_HANDLER(handle_exc_nm, EXC_NM)
MAKE_INT_HANDLER(handle_exc_df, EXC_DF)
MAKE_INT_HANDLER(handle_exc_co, EXC_CO)
MAKE_INT_HANDLER(handle_exc_ts, EXC_TS)
MAKE_INT_HANDLER(handle_exc_np, EXC_NP)
MAKE_INT_HANDLER(handle_exc_ss, EXC_SS)
MAKE_INT_HANDLER(handle_exc_gp, EXC_GP)
MAKE_INT_HANDLER(handle_exc_pf, EXC_PF)
MAKE_INT_HANDLER(handle_exc_re, EXC_RE)
MAKE_INT_HANDLER(handle_exc_mf, EXC_MF)
MAKE_INT_HANDLER(handle_exc_ac, EXC_AC)
MAKE_INT_HANDLER(handle_exc_mc, EXC_MC)
MAKE_INT_HANDLER(handle_exc_xf, EXC_XF)
MAKE_INT_HANDLER(handle_int_syscall, INT_SYSCALL)
MAKE_INT_HANDLER(handle_int_unknown, INT_UNKNOWN)

/* Common interrupt handler routine */
handle_int_thunk_common:
    // Save all registers
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

    // Call C interrupt handler, the stack is reinterpreted
    // as an instance of int_regs_t (so do not change the
    // ordering of either one!)
    pushl %esp
    call handle_interrupt
    addl $4, %esp

    // Restore all registers
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

    // Pop IRQ vector number
    addl $4, %esp

    // Return from interrupt
    iret
