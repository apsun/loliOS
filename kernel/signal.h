#ifndef _SIGNAL_H
#define _SIGNAL_H

#include "idt.h"
#include "types.h"
#include "syscall.h"

/* Number of supported signals */
#define NUM_SIGNALS 5

/* Signal numbers */
#define SIG_DIV_ZERO  0
#define SIG_SEGFAULT  1
#define SIG_INTERRUPT 2
#define SIG_ALARM     3
#define SIG_USER1     4

/* sigmask() actions and return values */
#define SIGMASK_NONE    0
#define SIGMASK_BLOCK   1
#define SIGMASK_UNBLOCK 2

/* Period of the alarm signal, in seconds */
#define SIG_ALARM_PERIOD 10

#ifndef ASM

typedef struct {
    /*
     * The number of this signal.
     */
    int signum;

    /*
     * Userspace address of the signal handler. Equal
     * to NULL if no handler has been set.
     */
    void *handler_addr;

    /*
     * Whether this signal is currently masked.
     */
    bool masked;

    /*
     * Whether this signal is scheduled for delivery.
     */
    bool pending;
} signal_info_t;

/* Signal syscall handlers */
__cdecl int signal_sigaction(int signum, void *handler_address);
__cdecl int signal_sigreturn(
    int signum,
    int_regs_t *user_regs,
    int unused1,
    int unused2,
    int unused3,
    int_regs_t *kernel_regs);
__cdecl int signal_sigraise(int signum);
__cdecl int signal_sigmask(int signum, int action);

/* Initializes the signal info array */
void signal_init(signal_info_t *signals);

/* Handles any pending signals */
void signal_handle_all(int_regs_t *regs);

/* Returns whether the currently executing process has a pending signal */
bool signal_has_pending(void);

/* Raises a signal for the specified process */
void signal_raise(int pid, int signum);

#endif /* ASM */

#endif /* _SIGNAL_H */