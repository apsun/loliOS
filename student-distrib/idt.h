#ifndef IDT_H
#define IDT_H

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
#define INT_SYSCALL 0x80
#define INT_UNKNOWN -1

/* Exception info */
typedef struct
{
    uint8_t index;
    const char *desc;
} exception_info_t;

/* Exception info table */
exception_info_t exception_table[20] = {
    {EXC_DE, "Divide Error Exception"},
    {EXC_DB, "Debug Exception"},
    {EXC_NI, "Nonmaskable Interrupt"},
    {EXC_BP, "Breakpoint Exception"},
    {EXC_OF, "Overflow Exception"},
    {EXC_BR, "BOUND Range Exceeded Exception"},
    {EXC_UD, "Invalid Opcode Exception"},
    {EXC_NM, "Device Not Available Exception"},
    {EXC_DF, "Double Fault Exception"},
    {EXC_CO, "Coprocessor Segment Overrun"},
    {EXC_TS, "Invalid TSS Exception"},
    {EXC_NP, "Segment Not Present"},
    {EXC_SS, "Stack Fault Exception"},
    {EXC_GP, "General Protection Exception"},
    {EXC_PF, "Page-Fault Exception"},
    {EXC_RE, "Entry Reserved"},
    {EXC_MF, "Floating-Point Error"},
    {EXC_AC, "Alignment Check Exception"},
    {EXC_MC, "Machine-Check Exception"},
    {EXC_XF, "SIMD Floating-Point Exception"},
};

/* Interrupt registers */
typedef struct
{
    /* Pushed by handle_int_common */
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

    /* Pushed by handle_int_{ID} */
    uint32_t int_num;

    /* Pushed automatically by processor */
    uint32_t eip;
    uint16_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint16_t ss;
} int_regs_t;

/* Initializes the IDT */
void
idt_init(void);

#endif
