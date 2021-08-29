#ifndef _SIGNAL_H
#define _SIGNAL_H

#include "idt.h"
#include "types.h"

/* Signal numbers */
#define SIGFPE 0
#define SIGSEGV 1
#define SIGINT 2
#define SIGALRM 3
#define SIGUSR1 4
#define SIGKILL 5
#define SIGPIPE 6
#define SIGABRT 7
#define NUM_SIGNALS 8

/* sigmask() actions and return values */
#define SIGMASK_NONE    0
#define SIGMASK_BLOCK   1
#define SIGMASK_UNBLOCK 2

/* sigaction() special handlers */
#define SIG_IGN ((void (*)(int))1)
#define SIG_DFL ((void (*)(int))0)

#ifndef ASM

typedef struct {
    /*
     * The number of this signal.
     */
    int signum;

    /*
     * Userspace address of the signal handler.
     * Can also take on two special values:
     *
     * SIG_DFL (the default) - no handler set
     * SIG_IGN - as if an empty handler has been set
     */
    void (*handler_addr)(int);

    /*
     * Whether this signal is currently masked.
     */
    bool masked : 1;

    /*
     * Whether this signal is scheduled for delivery.
     */
    bool pending : 1;
} signal_info_t;

/* Signal syscall handlers */
__cdecl int signal_sigaction(int signum, void (*handler_address)(int));
__cdecl int signal_sigreturn(
    int signum,
    int_regs_t *user_regs,
    intptr_t unused1,
    intptr_t unused2,
    intptr_t unused3,
    int_regs_t *kernel_regs);
__cdecl int signal_sigmask(int signum, int action);
__cdecl int signal_kill(int pid, int signum);

/* Initializes the signal info array */
void signal_init(signal_info_t *signals);

/* Clones an existing signal info array */
void signal_clone(signal_info_t *dest, signal_info_t *src);

/* Handles any pending signals */
void signal_handle_all(signal_info_t *signals, int_regs_t *regs);

/* Checks whether a process has a pending signal */
bool signal_has_pending(signal_info_t *signals);

/* Raises a signal for the executing process */
void signal_raise_executing(int signum);

#endif /* ASM */

#endif /* _SIGNAL_H */
