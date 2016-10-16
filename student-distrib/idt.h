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
#define INT_KEYBOARD 0x21
#define INT_RTC 0x28
#define INT_SYSCALL 0x80
#define INT_UNKNOWN -1

#ifndef ASM

/* Exception info */
typedef struct
{
    uint8_t index;
    const char *desc;
} exc_info_t;

/* Interrupt registers */
typedef struct
{
    /* Pushed by handle_int_thunk_common */
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;

    /* Pushed by per-interrupt handle_* thunk */
    uint32_t int_num;
    uint32_t error_code;

    /* Pushed automatically by processor */
    uint32_t eip;
    uint16_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint16_t ss;
} int_regs_t;

/* Exception info table */
exc_info_t exc_info_table[20];

/* Initializes the IDT */
void idt_init(void);

/* Interrupt handler routine */
void handle_interrupt(int_regs_t *regs);

#endif /* ASM */

#endif /* _IDT_H */
