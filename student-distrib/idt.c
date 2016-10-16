#include "idt.h"
#include "lib.h"
#include "debug.h"
#include "x86_desc.h"
#include "keyboard.h"

/* Exception info table */
exc_info_t exc_info_table[20] = {
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

/* Convenience wrapper around SET_IDT_ENTRY */
#define WRITE_IDT_ENTRY(i, name)               \
do {                                           \
    extern void name(void);                    \
    SET_IDT_ENTRY(idt[i], name);               \
} while (0)

/* Prints all interrupt registers */
static void
dump_registers(int_regs_t *regs)
{
    debugf("int_num: 0x%#x              \n", regs->int_num);
    debugf("error_code: 0x%#x           \n", regs->error_code);
    debugf("eax: 0x%#x                  \n", regs->eax);
    debugf("ebx: 0x%#x                  \n", regs->ebx);
    debugf("ecx: 0x%#x                  \n", regs->ecx);
    debugf("edx: 0x%#x                  \n", regs->edx);
    debugf("esi: 0x%#x                  \n", regs->esi);
    debugf("edi: 0x%#x                  \n", regs->edi);
    debugf("ebp: 0x%#x                  \n", regs->ebp);
    debugf("esp: 0x%#x                  \n", regs->esp);
    debugf("eip: 0x%#x                  \n", regs->eip);
    debugf("eflags: 0x%#x               \n", regs->eflags);
    debugf("cs: 0x%#x                   \n", regs->cs);
    debugf("ds: 0x%#x                   \n", regs->ds);
    debugf("es: 0x%#x                   \n", regs->es);
    debugf("fs: 0x%#x                   \n", regs->fs);
    debugf("gs: 0x%#x                   \n", regs->gs);
    debugf("ss: 0x%#x                   \n", regs->ss);
}

/* Exception handler */
static void
handle_exception(int_regs_t *regs)
{
    debugf("Exception: %s\n", exc_info_table[regs->int_num].desc);
    while (1);
}

/* Syscall handler */
static void
handle_syscall(int_regs_t *regs)
{
    debugf("Syscall: %d\n", regs->eax);
    switch (regs->eax) {
        /* TODO */
    }
}

/* Keyboard interrupt handler */
static void
handle_keyboard(int_regs_t *regs)
{
    debugf("Keyboard interrupt\n");
    keyboard_handle_interrupt();
}

/* RTC interrupt handler */
static void
handle_rtc(int_regs_t *regs)
{
    debugf("RTC interrupt\n");
}

/*
 * Called when an interrupt occurs (from idtthunk.S).
 * The registers in regs should not be modified unless
 * the interrupt is a syscall.
 */
void
handle_interrupt(int_regs_t *regs)
{
    dump_registers(regs);
    if (regs->int_num == INT_SYSCALL) {
        handle_syscall(regs);
    } else if (regs->int_num == INT_IRQ1) {
        handle_keyboard(regs);
    } else if (regs->int_num == INT_IRQ8) {
        handle_rtc(regs);
    } else if (regs->int_num < NUM_EXC) {
        handle_exception(regs);
    } else {
        debugf("Unknown interrupt: %d\n", regs->int_num);
    }
}

/*
 * Initializes the interrupt descriptor table.
 */
void
idt_init(void)
{
    idt_desc_t desc;
    int i;

    /* Initialize template interrupt descriptor */
    desc.present      = 1;
    desc.dpl          = 0;
    desc.reserved0    = 0;
    desc.size         = 1;
    desc.reserved1    = 1;
    desc.reserved2    = 1;
    desc.reserved3    = 1;
    desc.reserved4    = 0;
    desc.seg_selector = KERNEL_CS;
    desc.offset_15_00 = 0;
    desc.offset_31_16 = 0;

    /* Load the IDT */
    lidt(idt_desc_ptr);

    /* Initialize exception (trap) gates */
    desc.dpl = 0;
    desc.reserved3 = 1;
    for (i = 0; i < NUM_EXC; ++i) {
        idt[i] = desc;
    }

    /* Write exception handlers */
    WRITE_IDT_ENTRY(EXC_DE, handle_exc_de);
    WRITE_IDT_ENTRY(EXC_DB, handle_exc_db);
    WRITE_IDT_ENTRY(EXC_NI, handle_exc_ni);
    WRITE_IDT_ENTRY(EXC_BP, handle_exc_bp);
    WRITE_IDT_ENTRY(EXC_OF, handle_exc_of);
    WRITE_IDT_ENTRY(EXC_BR, handle_exc_br);
    WRITE_IDT_ENTRY(EXC_UD, handle_exc_ud);
    WRITE_IDT_ENTRY(EXC_NM, handle_exc_nm);
    WRITE_IDT_ENTRY(EXC_DF, handle_exc_df);
    WRITE_IDT_ENTRY(EXC_CO, handle_exc_co);
    WRITE_IDT_ENTRY(EXC_TS, handle_exc_ts);
    WRITE_IDT_ENTRY(EXC_NP, handle_exc_np);
    WRITE_IDT_ENTRY(EXC_SS, handle_exc_ss);
    WRITE_IDT_ENTRY(EXC_GP, handle_exc_gp);
    WRITE_IDT_ENTRY(EXC_PF, handle_exc_pf);
    WRITE_IDT_ENTRY(EXC_RE, handle_exc_re);
    WRITE_IDT_ENTRY(EXC_MF, handle_exc_mf);
    WRITE_IDT_ENTRY(EXC_AC, handle_exc_ac);
    WRITE_IDT_ENTRY(EXC_MC, handle_exc_mc);
    WRITE_IDT_ENTRY(EXC_XF, handle_exc_xf);

    /* Initialize interrupt gates */
    desc.dpl = 0;
    desc.reserved3 = 0;
    for (; i < NUM_VEC; ++i) {
        idt[i] = desc;
        WRITE_IDT_ENTRY(i, handle_int_unknown);
    }

    /* Handle IRQ1 (keyboard) and IRQ8 (RTC) */
    WRITE_IDT_ENTRY(INT_IRQ1, handle_int_irq1);
    WRITE_IDT_ENTRY(INT_IRQ8, handle_int_irq8);

    /* Initialize syscall interrupt gate */
    idt[INT_SYSCALL].dpl = 3;
    WRITE_IDT_ENTRY(INT_SYSCALL, handle_int_syscall);
}
