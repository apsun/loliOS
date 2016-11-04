#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "file.h"

#define MAX_ARGS_LEN 1024
#define MAX_PROCESSES 8
#define EXE_MAGIC 0x464c457f

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

/* Gets the PCB of the currently executing process */
pcb_t *get_executing_pcb(void);

/* Process syscall handlers */
int32_t process_halt(uint32_t status);
int32_t process_execute(const uint8_t *command);
int32_t process_getargs(uint8_t *buf, int32_t nbytes);
int32_t process_vidmap(uint8_t **screen_start);

/* Initializes processes. This must be called first in the kernel bootup sequence */
void process_init(void);

#endif /* ASM */

#endif /* _PROCESS_H */
