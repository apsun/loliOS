#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "list.h"
#include "file.h"
#include "idt.h"
#include "signal.h"
#include "timer.h"
#include "heap.h"

/* Maximum argument length, including the NUL terminator */
#define MAX_ARGS_LEN 128

/* User-modifiable bits in EFLAGS */
#define EFLAGS_USER 0xDD5

/* Interrupt flag */
#define EFLAGS_IF (1 << 9)

/* Direction flag */
#define EFLAGS_DF (1 << 10)

#ifndef ASM

/* Execution state of the process. */
typedef enum {
    /*
     * The process's state has been created, but has not
     * yet been run yet (i.e. it does not have a scheduler
     * call frame yet).
     */
    PROCESS_STATE_NEW,

    /*
     * The process is on the scheduler queue, as normal.
     */
    PROCESS_STATE_RUNNING,

    /*
     * The process is on a sleep queue, waiting for someone
     * to wake it up.
     */
    PROCESS_STATE_SLEEPING,

    /*
     * The process is dead, waiting for someone to call
     * wait() on it. It is not in any queues.
     */
    PROCESS_STATE_ZOMBIE,
} process_state_t;

/*
 * Process control block structure
 */
typedef struct {
    /*
     * PID of the this process. If the PCB is not valid, this will contain
     * a negative number.
     */
    int pid;

    /*
     * Execution state of the process.
     */
    process_state_t state;

    /*
     * PID of the parent process that created this process. If
     * there is no parent, this will be negative.
     */
    int parent_pid;

    /*
     * Which terminal the process is executing on. Inherited from
     * the parent.
     */
    int terminal;

    /*
     * Allocated physical address for this process's 128MB page.
     */
    uintptr_t user_paddr;

    /*
     * Initial state used to run the process. For the initial
     * processes spawned by the kernel, this will be initialized
     * manually. For child processes spawned by fork(), this will
     * be a copy of the parent's registers.
     */
    int_regs_t regs;

    /*
     * List for use by the scheduler. Every process is either in a
     * scheduler queue or a sleep queue.
     */
    list_t scheduler_list;

    /*
     * Kernel ESP/EBP of the process inside the scheduler. Used to
     * context switch between processes. Only valid if state == RUNNING.
     */
    uint32_t scheduler_esp;
    uint32_t scheduler_ebp;

    /*
     * ID of the group that this process belongs to.
     */
    int group;

    /*
     * Exit status of the process.
     */
    int exit_code;

    /*
     * Whether the process has the virtual video memory page
     * mapped in memory, set after the process has called the
     * vidmap syscall.
     */
    bool vidmap : 1;

    /*
     * Whether the process has the VBE framebuffer mapped in memory.
     */
    bool fbmap : 1;

    /*
     * Whether this process is being executed in compatibility mode.
     * This currently has the following effects:
     *
     * - All files other than stdin/stdout will be closed at startup
     * - stdin/stdout files cannot be closed
     * - Will be loaded with memcpy rather than the proper ELF loader
     */
    bool compat : 1;

    /*
     * Array containing open file object pointers. The index in the
     * array corresponds to the file descriptor.
     */
    file_obj_t *files[MAX_FILES];

    /*
     * Signal handler and status array.
     */
    signal_info_t signals[NUM_SIGNALS];

    /*
     * Timer for the SIGALRM signal.
     */
    timer_t alarm_timer;

    /*
     * Timer for the monosleep() syscall.
     */
    timer_t sleep_timer;

    /*
     * Heap metadata for this process.
     */
    heap_t heap;

    /*
     * Arguments passed when creating this process. Will always be
     * NUL-terminated (holds up to MAX_ARGS_LEN - 1 characters).
     */
    char args[MAX_ARGS_LEN];
} pcb_t;

/* Iterator-like API for get_next_pcb() */
#define process_for_each(pcb) \
    for (pcb = NULL; (pcb = get_next_pcb(pcb)) != NULL;)

/* Gets a PCB by its process ID */
pcb_t *get_pcb(int pid);

/* Gets the idle process PCB */
pcb_t *get_idle_pcb(void);

/* Gets the next PCB after the specified one */
pcb_t *get_next_pcb(pcb_t *pcb);

/* Gets the PCB of the currently executing process */
pcb_t *get_executing_pcb(void);

/* Process syscall handlers */
__cdecl int process_getargs(char *buf, int nbytes);
__cdecl int process_vidmap(uint8_t **screen_start);
__cdecl int process_sbrk(int delta, void **orig_brk);
__cdecl int process_fork(
    intptr_t unused1,
    intptr_t unused2,
    intptr_t unused3,
    intptr_t unused4,
    intptr_t unused5,
    int_regs_t *regs);
__cdecl int process_exec(
    const char *command,
    intptr_t unused1,
    intptr_t unused2,
    intptr_t unused3,
    intptr_t unused4,
    int_regs_t *regs);
__cdecl int process_wait(int *pid);
__cdecl int process_getpid(void);
__cdecl int process_getpgrp(void);
__cdecl int process_setpgrp(int pid, int pgrp);
__cdecl int process_execute(
    const char *command,
    intptr_t unused1,
    intptr_t unused2,
    intptr_t unused3,
    intptr_t unused4,
    int_regs_t *regs);
__cdecl void process_halt(int status);
__cdecl int process_monosleep(int target);

/* Unsets the global execution context for the specified process */
void process_unset_context(pcb_t *pcb);

/* Sets the global execution context for the specified process */
void process_set_context(pcb_t *pcb);

/* Runs the specified process by jumping into userspace */
__noreturn void process_run(pcb_t *pcb);

/* Halts the executing process with the specified status code */
__noreturn void process_halt_impl(int status);

/* Initializes processes */
void process_init(void);

/* Starts the initial shells */
__noreturn void process_start_shell(void);

#endif /* ASM */

#endif /* _PROCESS_H */
