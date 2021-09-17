#include "idt.h"
#include "types.h"
#include "printf.h"
#include "debug.h"
#include "x86_desc.h"
#include "irq.h"
#include "paging.h"
#include "syscall.h"
#include "process.h"
#include "signal.h"
#include "terminal.h"
#include "loopback.h"
#include "tcp.h"

/* Whether to display a BSOD on a userspace exception (for debugging) */
#ifndef USER_BSOD
#define USER_BSOD 0
#endif

/* Convenience wrapper around SET_IDT_ENTRY */
#define WRITE_IDT_ENTRY(i, name) do {       \
    extern void name(void);                 \
    SET_IDT_ENTRY(idt[i], name);            \
} while (0)

/* Exception number to name table */
static const char *exception_names[NUM_EXC] = {
    "Divide error exception",
    "Debug exception",
    "Nonmaskable interrupt",
    "Breakpoint exception",
    "Overflow exception",
    "Bound range exceeded exception",
    "Invalid opcode exception",
    "Device not available exception",
    "Double fault exception",
    "Coprocessor segment overrun",
    "Invalid TSS exception",
    "Segment not present",
    "Stack fault exception",
    "General protection exception",
    "Page-fault exception",
    "Reserved exception",
    "Floating-point error",
    "Alignment check exception",
    "Machine-check exception",
    "SIMD floating-point exception",
};

/* Prints all interrupt registers */
static void
dump_registers(int_regs_t *regs)
{
    printf("eax: 0x%08x     ", regs->eax);
    printf("ebx: 0x%08x     ", regs->ebx);
    printf("ecx: 0x%08x     ", regs->ecx);
    printf("edx: 0x%08x\n",    regs->edx);

    printf("esi: 0x%08x     ", regs->esi);
    printf("edi: 0x%08x     ", regs->edi);
    printf("ebp: 0x%08x     ", regs->ebp);
    printf("esp: 0x%08x\n",    regs->esp);

    uint32_t cr0, cr2, cr3, cr4;
    asm volatile(
        "movl %%cr0, %0;"
        "movl %%cr2, %1;"
        "movl %%cr3, %2;"
        "movl %%cr4, %3;"
        : "=r"(cr0), "=r"(cr2), "=r"(cr3), "=r"(cr4));

    printf("cr0: 0x%08x     ", cr0);
    printf("cr2: 0x%08x     ", cr2);
    printf("cr3: 0x%08x     ", cr3);
    printf("cr4: 0x%08x\n",    cr4);

    printf("eip: 0x%08x  ",     regs->eip);
    printf("eflags: 0x%08x   ", regs->eflags);
    printf("error: 0x%08x\n",   regs->error_code);

    printf("cs: 0x%04x   ", regs->cs);
    printf("ds: 0x%04x   ", regs->ds);
    printf("es: 0x%04x   ", regs->es);
    printf("fs: 0x%04x   ", regs->fs);
    printf("gs: 0x%04x   ", regs->gs);
    printf("ss: 0x%04x\n",  regs->ss);
}

/*
 * Reads a value off of the kernel stack and converts it into
 * a string.
 */
static char *
dump_callstack_utox(char buf[16], uint32_t *p)
{
    if (!is_memory_accessible(p, sizeof(uint32_t), false, false)) {
        return "<overflow>";
    }
    snprintf(buf, 16, "0x%08x", *p);
    return buf;
}

/*
 * Dumps the call stack leading up to the specified location.
 * This is a best-effort attempt, and can be unreliable.
 * Notably, this does not work well in -O2 (due to inlining),
 * or on static functions (since the compiler is free to change
 * their calling conventions).
 */
static void
dump_callstack(uint32_t eip, uint32_t ebp, int limit)
{
    int n = 0;
    while (n++ < limit && is_memory_accessible((void *)ebp, 8, false, false)) {
        uint32_t *args = (uint32_t *)(ebp + 8);
        char buf[5][16];
#define ARG(n) (dump_callstack_utox(buf[n], &args[n]))
        printf(" at 0x%08x (%s, %s, %s, %s, %s)\n",
            eip, ARG(0), ARG(1), ARG(2), ARG(3), ARG(4));
#undef ARG
        eip = *(uint32_t *)(ebp + 4);
        ebp = *(uint32_t *)ebp;
    }
}

/*
 * Handles an exception that occurred in userspace.
 * If a signal handler is available, will cause that to
 * be executed. Otherwise, kills the process.
 */
__used static void
handle_user_exception(int_regs_t *regs)
{
    debugf("%s in userspace at 0x%08x\n", exception_names[regs->int_num], regs->eip);
    if (regs->int_num == EXC_DE) {
        signal_raise_executing(SIGFPE);
    } else {
        signal_raise_executing(SIGSEGV);
    }
}

/* Exception handler */
static void
handle_exception(int_regs_t *regs)
{
#if !USER_BSOD
    /* If we were in userspace, run signal handler or kill the process */
    if (regs->cs == USER_CS) {
        handle_user_exception(regs);
        return;
    }
#endif

    terminal_clear_bsod();
    printf("Exception: %s (%d)\n", exception_names[regs->int_num], regs->int_num);
    printf("\nRegisters:\n");
    dump_registers(regs);
    printf("\nBacktrace:\n");
    dump_callstack(regs->eip, regs->ebp, 8);
    loop();
}

/* IRQ handler */
static void
handle_irq(int_regs_t *regs)
{
    int irq_num = regs->int_num - INT_IRQ0;
    irq_handle_interrupt(irq_num);
}

/* Syscall handler */
static void
handle_syscall(int_regs_t *regs)
{
    regs->eax = syscall_handle(
        regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi,
        regs, regs->eax);
}

/*
 * Called when an interrupt occurs (from idtthunk.S).
 * The registers in regs should not be modified unless
 * the interrupt is a syscall.
 */
__cdecl void
idt_handle_interrupt(int_regs_t *regs)
{
    if (regs->int_num >= 0 && regs->int_num < NUM_EXC) {
        handle_exception(regs);
    } else if (regs->int_num >= INT_IRQ0 && regs->int_num <= INT_IRQ15) {
        handle_irq(regs);
    } else if (regs->int_num == INT_SYSCALL) {
        handle_syscall(regs);
    } else {
        debugf("Unknown interrupt: %d\n", regs->int_num);
    }

    /* Deliver queued up loopback packets */
    loopback_deliver();

    /* Deliver queued up ACKs */
    tcp_deliver_ack();

    /*
     * If process has any pending signals, run their handlers.
     * Note that since we have security checks inside sigreturn,
     * we only do this if we came from userspace, since that's
     * the only place we can safely return to after sigreturn.
     */
    if (regs->cs == USER_CS) {
        signal_handle_all(get_executing_pcb()->signals, regs);
    }
}

/*
 * Initializes the interrupt descriptor table.
 */
void
idt_init(void)
{
    /* Initialize template interrupt descriptor */
    idt_desc_t desc;
    desc.present      = 1;
    desc.dpl          = 0;
    desc.storage_seg  = 0;
    desc.type         = GATE_INTERRUPT;
    desc.reserved     = 0;
    desc.seg_selector = KERNEL_CS;

    /* Default initialization for most gates */
    int i;
    for (i = 0; i < NUM_VEC; ++i) {
        idt[i] = desc;
        WRITE_IDT_ENTRY(i, idt_handle_int_unknown);
    }

    /* Exception handlers */
    WRITE_IDT_ENTRY(EXC_DE, idt_handle_exc_de);
    WRITE_IDT_ENTRY(EXC_DB, idt_handle_exc_db);
    WRITE_IDT_ENTRY(EXC_NI, idt_handle_exc_ni);
    WRITE_IDT_ENTRY(EXC_BP, idt_handle_exc_bp);
    WRITE_IDT_ENTRY(EXC_OF, idt_handle_exc_of);
    WRITE_IDT_ENTRY(EXC_BR, idt_handle_exc_br);
    WRITE_IDT_ENTRY(EXC_UD, idt_handle_exc_ud);
    WRITE_IDT_ENTRY(EXC_NM, idt_handle_exc_nm);
    WRITE_IDT_ENTRY(EXC_DF, idt_handle_exc_df);
    WRITE_IDT_ENTRY(EXC_CO, idt_handle_exc_co);
    WRITE_IDT_ENTRY(EXC_TS, idt_handle_exc_ts);
    WRITE_IDT_ENTRY(EXC_NP, idt_handle_exc_np);
    WRITE_IDT_ENTRY(EXC_SS, idt_handle_exc_ss);
    WRITE_IDT_ENTRY(EXC_GP, idt_handle_exc_gp);
    WRITE_IDT_ENTRY(EXC_PF, idt_handle_exc_pf);
    WRITE_IDT_ENTRY(EXC_RE, idt_handle_exc_re);
    WRITE_IDT_ENTRY(EXC_MF, idt_handle_exc_mf);
    WRITE_IDT_ENTRY(EXC_AC, idt_handle_exc_ac);
    WRITE_IDT_ENTRY(EXC_MC, idt_handle_exc_mc);
    WRITE_IDT_ENTRY(EXC_XF, idt_handle_exc_xf);

    /* IRQ handlers */
    WRITE_IDT_ENTRY(INT_IRQ0, idt_handle_int_irq0);
    WRITE_IDT_ENTRY(INT_IRQ1, idt_handle_int_irq1);
    WRITE_IDT_ENTRY(INT_IRQ2, idt_handle_int_irq2);
    WRITE_IDT_ENTRY(INT_IRQ3, idt_handle_int_irq3);
    WRITE_IDT_ENTRY(INT_IRQ4, idt_handle_int_irq4);
    WRITE_IDT_ENTRY(INT_IRQ5, idt_handle_int_irq5);
    WRITE_IDT_ENTRY(INT_IRQ6, idt_handle_int_irq6);
    WRITE_IDT_ENTRY(INT_IRQ7, idt_handle_int_irq7);
    WRITE_IDT_ENTRY(INT_IRQ8, idt_handle_int_irq8);
    WRITE_IDT_ENTRY(INT_IRQ9, idt_handle_int_irq9);
    WRITE_IDT_ENTRY(INT_IRQ10, idt_handle_int_irq10);
    WRITE_IDT_ENTRY(INT_IRQ11, idt_handle_int_irq11);
    WRITE_IDT_ENTRY(INT_IRQ12, idt_handle_int_irq12);
    WRITE_IDT_ENTRY(INT_IRQ13, idt_handle_int_irq13);
    WRITE_IDT_ENTRY(INT_IRQ14, idt_handle_int_irq14);
    WRITE_IDT_ENTRY(INT_IRQ15, idt_handle_int_irq15);

    /* Syscall handler (DPL = 3 to allow userspace access) */
    idt[INT_SYSCALL].dpl = 3;
    WRITE_IDT_ENTRY(INT_SYSCALL, idt_handle_int_syscall);

    /* Load the IDT */
    lidt(idt_desc_ptr);
}
