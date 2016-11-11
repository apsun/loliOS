#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "file.h"
#include "syscall.h"

/* Maximum argument length, including the NUL terminator */
#define MAX_ARGS_LEN 1024

/* Maximum number of processes */
#define MAX_PROCESSES 8

/* Executable magic bytes ('\x7fELF') */
#define EXE_MAGIC 0x464c457f

/* Kernel stack size, MUST BE A POWER OF 2! */
#define KERNEL_STACK_SIZE 8192

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
    int32_t pid;

    /*
     * PID of the parent process that created this process. If
     * there is no parent, this will be negative.
     */
    int32_t parent_pid;

    /*
     * Kernel ESP of the parent. Used to jump back to the parent
     * kernel stack frame after the child calls halt(). Invalid if
     * the process has no parent (i.e. parent_pid < 0).
     */
    uint32_t parent_esp;

    /*
     * Kernel EBP of the parent. See parent_esp.
     */
    uint32_t parent_ebp;

    /*
     * User EIP of this process.
     *
     * TODO: When supporting preemptive multitasking, this might
     * need to be changed to a full register state
     */
    uint32_t user_eip;

    /*
     * Which terminal the process is executing on. Inherited from
     * the parent.
     */
    int32_t terminal;

    /*
     * Whether the process has the virtual video memory page
     * mapped in memory, set after the process has called the
     * vidmap syscall.
     */
    bool vidmap;

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

    uint8_t kernel_stack[KERNEL_STACK_SIZE - sizeof(pcb_t *)];
} process_data_t;

/* Gets the PCB of the currently executing process */
pcb_t *get_executing_pcb(void);

/* Syscall delegate functions */
int32_t process_halt_impl(uint32_t status);
int32_t process_execute_impl(const uint8_t *command, pcb_t *parent_pcb, int32_t terminal);

/* Process syscall handlers */
__cdecl int32_t process_halt(uint32_t status);
__cdecl int32_t process_execute(const uint8_t *command);
__cdecl int32_t process_getargs(uint8_t *buf, int32_t nbytes);
__cdecl int32_t process_vidmap(uint8_t **screen_start);

/* Initializes processes. */
void process_init(void);

/* Starts the shell. This must only be called after all kernel init has completed. */
void process_start_shell(void);

#endif /* ASM */

#endif /* _PROCESS_H */
