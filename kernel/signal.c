#include "signal.h"
#include "lib.h"
#include "debug.h"
#include "process.h"
#include "x86_desc.h"

/* User-modifiable bits in EFLAGS */
#define EFLAGS_USER 0xDD5

/* Interrupt flag */
#define EFLAGS_IF (1 << 9)

/* Direction flag */
#define EFLAGS_DF (1 << 10)

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
    assert((sizeof(shellcode) & 0x3) == 0);

    /* Two-step copy: first copy to buffer, then copy buffer to userspace */
    uint8_t buf[
        sizeof(shellcode) +  /* shellcode */
        sizeof(int_regs_t) + /* regs */
        sizeof(int) +        /* signum */
        sizeof(uint32_t)];   /* return address */
    uint8_t *bufp = buf + sizeof(buf);
    uint8_t *esp = (uint8_t *)regs->esp;

    /* No way we can fit this onto the user stack, abort! */
    if ((uint32_t)esp < sizeof(buf)) {
        return false;
    }

    /* Push sigreturn linkage onto user stack */
    bufp -= sizeof(shellcode);
    esp -= sizeof(shellcode);
    memcpy(bufp, shellcode, sizeof(shellcode));
    uint8_t *shellcode_addr = esp;
    uint8_t *shellcode_bufp = bufp;

    /* Push interrupt context onto user stack */
    bufp -= sizeof(int_regs_t);
    esp -= sizeof(int_regs_t);
    memcpy(bufp, regs, sizeof(int_regs_t));
    uint8_t *intregs_addr = esp;

    /* Push signal number onto user stack */
    bufp -= sizeof(int);
    esp -= sizeof(int);
    memcpy(bufp, &sig->signum, sizeof(int));
    uint8_t *signum_addr = esp;

    /* Push return address (which is sigreturn linkage) onto user stack */
    bufp -= sizeof(uint32_t);
    esp -= sizeof(uint32_t);
    memcpy(bufp, &shellcode_addr, sizeof(uint32_t));

    /* Fill in shellcode values */
    int syscall_num = SYS_SIGRETURN;
    memcpy(shellcode_bufp + 1, &syscall_num, 4);
    memcpy(shellcode_bufp + 6, signum_addr, 4);
    memcpy(shellcode_bufp + 11, &intregs_addr, 4);

    /* Copy everything into userspace */
    assert(bufp == buf);
    if (!copy_to_user(esp, bufp, sizeof(buf))) {
        return false;
    }

    /* Change EIP of userspace program to the signal handler */
    regs->eip = (uint32_t)sig->handler_addr;

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

/* sigaction() syscall handler */
__cdecl int
signal_sigaction(int signum, void *handler_address)
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
__cdecl int
signal_sigreturn(
    int signum,
    int_regs_t *user_regs,
    int unused1,
    int unused2,
    int unused3,
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

/* sigraise() syscall handler */
__cdecl int
signal_sigraise(int signum)
{
    /* Check signal number range */
    if (signum < 0 || signum >= NUM_SIGNALS) {
        return -1;
    }

    pcb_t *pcb = get_executing_pcb();
    pcb->signals[signum].pending = true;
    return 0;
}

/* sigmask() syscall handler */
__cdecl int
signal_sigmask(int signum, int action)
{
    pcb_t *pcb = get_executing_pcb();
    signal_info_t *sig = &pcb->signals[signum];
    int orig_masked = sig->masked ? SIGMASK_BLOCK : SIGMASK_UNBLOCK;
    switch (action) {
    case SIGMASK_NONE:
        break;
    case SIGMASK_BLOCK:
        sig->masked = true;
        break;
    case SIGMASK_UNBLOCK:
        sig->masked = false;
        break;
    default:
        return -1;
    }
    return orig_masked;
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
    if (sig->handler_addr != NULL && !sig->masked) {
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
    int i;
    for (i = 0; i < NUM_SIGNALS; ++i) {
        signals[i].signum = i;
        signals[i].handler_addr = NULL;
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
    int i;
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
    int i;
    for (i = 0; i < NUM_SIGNALS; ++i) {
        signal_info_t *sig = &pcb->signals[i];
        if (sig->pending) {
            /*
             * If user manually registered a handler and the
             * signal is not masked, then we always execute it.
             */
            if (sig->handler_addr != NULL && !sig->masked) {
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
signal_raise(int pid, int signum)
{
    pcb_t *pcb = get_pcb_by_pid(pid);
    pcb->signals[signum].pending = true;
}