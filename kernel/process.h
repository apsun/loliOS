#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "file.h"
#include "syscall.h"
#include "idt.h"
#include "signal.h"
#include "paging.h"
#include "timer.h"

/* Maximum argument length, including the NUL terminator */
#define MAX_ARGS_LEN 128

#ifndef ASM

/*
 * Process control block structure
 */
typedef struct {
    /*
     * PID of the this process. If the PCB does not represent
     * a valid process, this will be negative. Non-negative PIDs
     * represent valid processes.
     */
    int pid;

    /*
     * Allocated physical page frame number for this process's 128MB page.
     */
    int pfn;

    /*
     * PID of the parent process that created this process. If
     * there is no parent, this will be negative.
     */
    int parent_pid;

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
     * Entry point of this process. Used for the initial jump into userspace.
     */
    uint32_t entry_point;

    /*
     * Which terminal the process is executing on. Inherited from
     * the parent.
     */
    int terminal;

    /*
     * Execution status of the process.
     */
    int status;

    /*
     * Whether the process has the virtual video memory page
     * mapped in memory, set after the process has called the
     * vidmap syscall.
     */
    bool vidmap;

    /*
     * Timer for the SIGALARM signal.
     */
    timer_t alarm_timer;

    /*
     * Signal handler and status array.
     */
    signal_info_t signals[NUM_SIGNALS];

    /*
     * Array of file objects for this process.
     */
    file_obj_t files[MAX_FILES];

    /*
     * Heap metadata for this process.
     */
    paging_heap_t heap;

    /*
     * Arguments passed when creating this process. Will always be
     * NUL-terminated (holds up to MAX_ARGS_LEN - 1 characters).
     */
    char args[MAX_ARGS_LEN];
} pcb_t;

/* Gets a PCB by its process ID */
pcb_t *get_pcb_by_pid(int pid);

/* Gets the PCB of the currently executing process */
pcb_t *get_executing_pcb(void);

/* Gets the PCB of the process currently running in the specified terminal */
pcb_t *get_pcb_by_terminal(int terminal);

/* Process syscall handlers */
__cdecl int process_halt(int status);
__cdecl int process_execute(const char *command);
__cdecl int process_getargs(char *buf, int nbytes);
__cdecl int process_vidmap(uint8_t **screen_start);
__cdecl int process_sbrk(int delta);

/* Initializes processes. */
void process_init(void);

/* Switches to the next scheduled process */
void process_switch(void);

/* Halts the executing process with the specified status code */
int process_halt_impl(int status);

/* Starts the shell. This must only be called after kernel initialization. */
void process_start_shell(void);

/* Handles RTC updates and delivers SIG_ALARM when necessary */
void process_update_clock(int rtc_counter);

#endif /* ASM */

#endif /* _PROCESS_H */
