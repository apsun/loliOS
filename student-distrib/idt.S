#include "idt.h"

/* Laziness is a virtue */
#define MAKE_INT_HANDLER(id)           \
    .global handle_int_thunk_##id;     \
    handle_int_thunk_##id:             \
        pushl $ id;                    \
        jmp handle_int_thunk_common

.text

/* Generate interrupt handlers */
MAKE_INT_HANDLER(EXC_DE)
MAKE_INT_HANDLER(EXC_DB)
MAKE_INT_HANDLER(EXC_NI)
MAKE_INT_HANDLER(EXC_BP)
MAKE_INT_HANDLER(EXC_OF)
MAKE_INT_HANDLER(EXC_BR)
MAKE_INT_HANDLER(EXC_UD)
MAKE_INT_HANDLER(EXC_NM)
MAKE_INT_HANDLER(EXC_DF)
MAKE_INT_HANDLER(EXC_CO)
MAKE_INT_HANDLER(EXC_TS)
MAKE_INT_HANDLER(EXC_NP)
MAKE_INT_HANDLER(EXC_SS)
MAKE_INT_HANDLER(EXC_GP)
MAKE_INT_HANDLER(EXC_PF)
MAKE_INT_HANDLER(EXC_RE)
MAKE_INT_HANDLER(EXC_MF)
MAKE_INT_HANDLER(EXC_AC)
MAKE_INT_HANDLER(EXC_MC)
MAKE_INT_HANDLER(EXC_XF)
MAKE_INT_HANDLER(INT_SYSCALL)
MAKE_INT_HANDLER(INT_UNKNOWN)

/* Common interrupt handler routine */
handle_int_thunk_common:
    // Save all registers
    // This must correspond to the member order
    // in int_regs_t.
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

    // Call C interrupt handler with "register struct" as argument
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
