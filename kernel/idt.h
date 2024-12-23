#ifndef _IDT_H
#define _IDT_H

#include "types.h"

/* Interrupt codes */
#define EXC_DE 0
#define EXC_DB 1
#define EXC_NI 2
#define EXC_BP 3
#define EXC_OF 4
#define EXC_BR 5
#define EXC_UD 6
#define EXC_NM 7
#define EXC_DF 8
#define EXC_CO 9
#define EXC_TS 10
#define EXC_NP 11
#define EXC_SS 12
#define EXC_GP 13
#define EXC_PF 14
#define EXC_RE 15
#define EXC_MF 16
#define EXC_AC 17
#define EXC_MC 18
#define EXC_XF 19
#define INT_IRQ0 0x20
#define INT_IRQ1 0x21
#define INT_IRQ2 0x22
#define INT_IRQ3 0x23
#define INT_IRQ4 0x24
#define INT_IRQ5 0x25
#define INT_IRQ6 0x26
#define INT_IRQ7 0x27
#define INT_IRQ8 0x28
#define INT_IRQ9 0x29
#define INT_IRQ10 0x2A
#define INT_IRQ11 0x2B
#define INT_IRQ12 0x2C
#define INT_IRQ13 0x2D
#define INT_IRQ14 0x2E
#define INT_IRQ15 0x2F
#define INT_SYSCALL 0x80
#define INT_UNKNOWN -1

#ifndef ASM

/* Interrupt registers */
typedef struct {
    /* Pushed by idt_handle_common_thunk */
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint16_t gs;
    uint16_t fs;
    uint16_t es;
    uint16_t ds;

    /* Pushed by per-interrupt idt_handle_* thunk */
    int32_t int_num;

    /*
     * Pushed automatically by processor for some
     * interrupts, for other ones we manually push
     * a dummy value (0x0)
     */
    uint32_t error_code;

    /* Pushed automatically by processor */
    uint32_t eip;
    uint16_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint16_t ss;
} int_regs_t;

/* Initializes the IDT */
void idt_init(void);

/* Triggers a kernel panic */
__noreturn void idt_panic(const char *fmt, ...);

/* Interrupt handler routine */
__cdecl void idt_handle_interrupt(int_regs_t *regs);

#endif /* ASM */

#endif /* _IDT_H */
