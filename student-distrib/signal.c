#include "signal.h"
#include "lib.h"
#include "debug.h"
#include "process.h"
#include "x86_desc.h"

/*
 * Pushes the signal handler context onto the user stack
 * and modifies the register context to start execution
 * at the signal handler.
 */
static bool
signal_deliver(signal_info_t *sig, int_regs_t *regs)
{
    /* "Shellcode" that calls the sigreturn() syscall */
    uint8_t shellcode[] = {
        /* movl $SYS_SIGRETURN, %eax */
        0xB8, 0xAA, 0xAA, 0xAA, 0xAA,

        /* movl signum, %ebx */
        0xBB, 0xBB, 0xBB, 0xBB, 0xBB,

        /* movl regs, %ecx */
        0xB9, 0xCC, 0xCC, 0xCC, 0xCC,

        /* int 0x80 */
        0xCD, 0x80,

        /* nop (to align stack to 4 bytes) */
        0x90, 0x90, 0x90,
    };

    /* Make sure shellcode is 4-byte aligned for stack */
    ASSERT((sizeof(shellcode) & 0x3) == 0);

    /* Holds the userspace stack pointer */
    uint8_t *esp = (uint8_t *)regs->esp;

    /* Push sigreturn linkage onto user stack */
    esp -= sizeof(shellcode);
    uint8_t *linkage_addr = esp;
    if (!copy_to_user(esp, shellcode, sizeof(shellcode))) {
        return false;
    }

    /* Push interrupt context onto user stack */
    esp -= sizeof(int_regs_t);
    uint8_t *intregs_addr = esp;
    if (!copy_to_user(esp, regs, sizeof(int_regs_t))) {
        return false;
    }

    /* Push signal number onto user stack */
    esp -= sizeof(int32_t);
    uint8_t *signum_addr = esp;
    if (!copy_to_user(esp, &sig->signum, sizeof(int32_t))) {
        return false;
    }

    /* Push return address (which is sigreturn linkage) onto user stack */
    esp -= sizeof(uint32_t);
    if (!copy_to_user(esp, &linkage_addr, sizeof(uint32_t))) {
        return false;
    }

    /* Fill in shellcode values */
    int32_t syscall_num = SYS_SIGRETURN;
    copy_to_user(linkage_addr + 1, &syscall_num, 4);
    copy_to_user(linkage_addr + 6, signum_addr, 4);
    copy_to_user(linkage_addr + 11, &intregs_addr, 4);

    /* Change EIP of userspace program to the signal handler */
    regs->eip = sig->handler_addr;

    /* Change ESP to point to new stack bottom */
    regs->esp = (uint32_t)esp;

    /* Fix segment registers in case that was the cause of an exception */
    regs->cs = USER_CS;
    regs->ds = USER_DS;
    regs->es = USER_DS;
    regs->fs = USER_DS;
    regs->gs = USER_DS;
    regs->ss = USER_DS;

    /* Clear direction flag */
    regs->eflags &= ~EFLAGS_DF;

    /* Mask signal so we don't get re-entrant calls */
    sig->masked = true;
    sig->pending = false;
    return true;
}

/* set_handler() syscall handler */
__cdecl int32_t
signal_set_handler(int32_t signum, uint32_t handler_address)
{
    /* Check signal number range */
    if (signum < 0 || signum >= NUM_SIGNALS) {
        return -1;
    }

    pcb_t *pcb = get_executing_pcb();
    pcb->signals[signum].handler_addr = handler_address;
    return 0;
}

/* sigreturn() syscall handler */
__cdecl int32_t
signal_sigreturn(
    int32_t signum,
    int_regs_t *user_regs,
    __unused uint32_t unused,
    int_regs_t *kernel_regs)
{
    /* Check signal number range */
    if (signum < 0 || signum >= NUM_SIGNALS) {
        debugf("Invalid signal number\n");
        return -1;
    }

    /* First copy to a temporary context... */
    int_regs_t tmp_regs;
    if (!copy_from_user(&tmp_regs, user_regs, sizeof(int_regs_t))) {
        debugf("Cannot read user regs\n");
        return -1;
    }

    /* Unmask signal again */
    pcb_t *pcb = get_executing_pcb();
    pcb->signals[signum].masked = false;

    /* Ignore privileged EFLAGS bits (emulate POPFL behavior) */
    /* http://stackoverflow.com/a/39195843 */
    uint32_t kernel_eflags = kernel_regs->eflags & ~EFLAGS_USER;
    uint32_t user_eflags = tmp_regs.eflags & EFLAGS_USER;
    tmp_regs.eflags = kernel_eflags | user_eflags;

    /* Reset segment registers (no privilege exploits for you!) */
    tmp_regs.cs = USER_CS;
    tmp_regs.ds = USER_DS;
    tmp_regs.es = USER_DS;
    tmp_regs.fs = USER_DS;
    tmp_regs.gs = USER_DS;
    tmp_regs.ss = USER_DS;

    /* Copy temporary context to kernel context */
    *kernel_regs = tmp_regs;

    /*
     * Interrupt handler overwrites EAX with the return value,
     * so we just return EAX so it will get set to itself.
     */
    return kernel_regs->eax;
}

/*
 * Attempts to deliver a signal to the currently
 * executing process. Returns true if the signal
 * was actually delivered or the process was
 * killed, and false if the signal was ignored.
 */
static bool
signal_handle(signal_info_t *sig, int_regs_t *regs)
{
    /* If handler is set and signal isn't masked, run it */
    if (sig->handler_addr != 0 && !sig->masked) {
        /* If no more space on stack to push signal context, kill process */
        if (!signal_deliver(sig, regs)) {
            debugf("Failed to push signal context, killing process\n");
            process_halt_impl(256);
        }
        return true;
    }

    /* Run default handler if no handler or masked */
    if (sig->signum == SIG_DIV_ZERO || sig->signum == SIG_SEGFAULT) {
        debugf("Killing process due to exception\n");
        process_halt_impl(256);
        return true;
    }

    /* CTRL-C halts with exit code 130 (SIGINT) */
    if (sig->signum == SIG_INTERRUPT) {
        debugf("Killing process due to CTRL-C\n");
        process_halt_impl(130);
        return true;
    }

    /* Default action is to ignore the signal */
    sig->pending = false;
    return false;
}

/*
 * Initializes the signal array for a proces.
 */
void
signal_init(signal_info_t *signals)
{
    int32_t i;
    for (i = 0; i < NUM_SIGNALS; ++i) {
        signals[i].signum = i;
        signals[i].handler_addr = 0;
        signals[i].masked = false;
        signals[i].pending = false;
    }
}

/*
 * If the currently executing process has any pending
 * signals, modifies the IRET context and user stack to run the
 * signal handler.
 */
void
signal_handle_all(int_regs_t *regs)
{
    pcb_t *pcb = get_executing_pcb();
    int32_t i;
    for (i = 0; i < NUM_SIGNALS; ++i) {
        signal_info_t *sig = &pcb->signals[i];
        if (sig->pending && signal_handle(sig, regs)) {
            break;
        }
    }
}

/*
 * Returns whether the currently executing process
 * has a pending signal for which there exists a
 * handler (or default action) that does something.
 */
bool
signal_has_pending(void)
{
    pcb_t *pcb = get_executing_pcb();
    int32_t i;
    for (i = 0; i < NUM_SIGNALS; ++i) {
        signal_info_t *sig = &pcb->signals[i];
        if (sig->pending) {
            /*
             * If user manually registered a handler and the
             * signal is not masked, then we always execute it.
             */
            if (sig->handler_addr != 0 && !sig->masked) {
                return true;
            }

            /*
             * If there's no manually registered handler, check
             * if default action actually does something. Here we
             * ignore whether the signal is masked since all our
             * default actions kill the process.
             */
            switch (sig->signum) {
            case SIG_DIV_ZERO:
            case SIG_SEGFAULT:
            case SIG_INTERRUPT:
                return true;
            }
        }
    }
    return false;
}

/*
 * Raises (marks as pending) a signal for the specified process.
 */
void
signal_raise(int32_t pid, int32_t signum)
{
    pcb_t *pcb = get_pcb_by_pid(pid);
    signal_info_t *sig = &pcb->signals[signum];
    sig->pending = true;
}
