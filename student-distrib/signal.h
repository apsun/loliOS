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
    int32_t signum;

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
__cdecl int32_t signal_sigaction(int32_t signum, void *handler_address);
__cdecl int32_t signal_sigreturn(
    int32_t signum,
    int_regs_t *user_regs,
    uint32_t unused,
    int_regs_t *kernel_regs);
__cdecl int32_t signal_sigraise(int32_t signum);
__cdecl int32_t signal_sigmask(int32_t signum, int32_t action);

/* Initializes the signal info array */
void signal_init(signal_info_t *signals);

/* Handles any pending signals */
void signal_handle_all(int_regs_t *regs);

/* Returns whether the currently executing process has a pending signal */
bool signal_has_pending(void);

/* Raises a signal for the specified process */
void signal_raise(int32_t pid, int32_t signum);

#endif /* ASM */

#endif /* _SIGNAL_H */
