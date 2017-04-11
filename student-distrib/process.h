#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "file.h"
#include "syscall.h"
#include "idt.h"

/* Maximum argument length, including the NUL terminator */
#define MAX_ARGS_LEN 1024

/* Maximum number of processes */
#define MAX_PROCESSES 6

/* Executable magic bytes ('\x7fELF') */
#define EXE_MAGIC 0x464c457f

/* Process data block size, MUST BE A POWER OF 2! */
#define PROCESS_DATA_SIZE 8192

/*
 * The process has been initialized and is running or
 * is scheduled to run
 */
#define PROCESS_RUN 0

/*
 * The process is waiting for a child process to exit
 * and should not be scheduled for execution
 */
#define PROCESS_SLEEP 1

/*
 * The process has been created, but not initialized
 * (i.e. process_run has not yet been called)
 */
#define PROCESS_SCHED 2

/* Number of supported signals */
#define NUM_SIGNALS 5

/* Signal numbers */
#define SIG_DIV_ZERO  0
#define SIG_SEGFAULT  1
#define SIG_INTERRUPT 2
#define SIG_ALARM     3
#define SIG_USER1     4

#ifndef ASM

typedef struct {
    /*
     * Userspace address of the signal handler. Equal
     * to 0 if no handler has been set.
     */
    uint32_t handler_addr;

    /*
     * Whether this signal is currently masked.
     */
    bool masked;

    /*
     * Whether this signal is scheduled for delivery.
     */
    bool pending;
} signal_info_t;

/*
 * Process control block structure
 */
typedef struct {
    /*
     * PID of the this process. If the PCB does not represent
     * a valid process, this will be negative. Non-negative PIDs
     * represent valid processes.
     */
    int32_t pid;

    /*
     * PID of the parent process that created this process. If
     * there is no parent, this will be negative.
     */
    int32_t parent_pid;

    /*
     * Kernel ESP/EBP of the parent process. Used to return to the parent
     * process's stack frame from halt inside the child process. This is
     * only valid if parent_pid >= 0.
     */
    uint32_t parent_esp;
    uint32_t parent_ebp;

    /*
     * Kernel ESP/EBP of the process, set inside process_switch so that
     * we can return into different processes. This is only valid if
     * status == PROCESS_RUN.
     */
    uint32_t kernel_esp;
    uint32_t kernel_ebp;

    /*
     * User EIP of this process. Used for the initial jump into userspace.
     */
    uint32_t user_eip;

    /*
     * Which terminal the process is executing on. Inherited from
     * the parent.
     */
    int32_t terminal;

    /*
     * Execution status of the process.
     */
    int32_t status;

    /*
     * Whether the process has the virtual video memory page
     * mapped in memory, set after the process has called the
     * vidmap syscall.
     */
    bool vidmap;

    /*
     * Signal handler and status array.
     */
    signal_info_t signals[NUM_SIGNALS];

    /*
     * Array of file objects for this process.
     */
    file_obj_t files[MAX_FILES];

    /*
     * Arguments passed when creating this process. Will always be
     * NUL-terminated (holds up to MAX_ARGS_LEN - 1 characters).
     */
    uint8_t args[MAX_ARGS_LEN];
} pcb_t;

/* Kernel stack struct */
typedef struct {
    pcb_t *pcb;
    uint8_t kernel_stack[PROCESS_DATA_SIZE - sizeof(pcb_t *)];
} process_data_t;

/* Gets the PCB of the currently executing process */
pcb_t *get_executing_pcb(void);

/* Gets the PCB of the process currently running in the specified terminal */
pcb_t *get_pcb_by_terminal(int32_t terminal);

/* Process syscall handlers */
__cdecl int32_t process_halt(uint32_t status);
__cdecl int32_t process_execute(const uint8_t *command);
__cdecl int32_t process_getargs(uint8_t *buf, int32_t nbytes);
__cdecl int32_t process_vidmap(uint8_t **screen_start);
__cdecl int32_t process_set_handler(int32_t signum, uint32_t handler_address);
__cdecl int32_t process_sigreturn(int32_t signum, int_regs_t *user_regs, uint32_t, int_regs_t *kernel_regs);

/* Initializes processes. */
void process_init(void);

/* Switches to the next scheduled process */
void process_switch(void);

/* Handles any pending signals */
void process_handle_signals(int_regs_t *regs);

/* Handles a userspace exception */
void process_user_exception(uint32_t int_num);

/* Handles CTRL-C input */
void process_interrupt(int32_t terminal);

/* Starts the shell. This must only be called after all kernel init has completed. */
void process_start_shell(void);

#endif /* ASM */

#endif /* _PROCESS_H */
