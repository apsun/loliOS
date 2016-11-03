#include "process.h"
#include "lib.h"

/* Process control blocks */
static pcb_t process_info[MAX_PROCESSES];

/* PID of the currently executing process */
static int32_t current_pid = 0;

/* Gets the PCB of the currently executing process */
pcb_t *
get_executing_pcb()
{
    return &process_info[current_pid];
}

/*
 * Allocates a new PCB. Returns a pointer to the PCB, or
 * NULL if the maximum number of processes are already
 * running.
 */
static pcb_t *
process_new_pcb(void)
{
    int32_t i;

    /* Look for an empty process slot we can fill */
    for (i = 0; i < MAX_PROCESSES; ++i) {
        if (process_info[i].pid < 0) {
            return &process_info[i];
        }
    }

    /* Reached max number of processes */
    return NULL;
}

/* halt() syscall handler */
int32_t
process_halt(uint8_t status)
{
    pcb_t *pcb = get_executing_pcb();

    /* Mark PCB as free */
    pcb->pid = -1;

    /* TODO */
    return -1;
}

/* execute() syscall handler */
int32_t
process_execute(const uint8_t *command)
{
    /* Allocate a new PCB */
    pcb_t *child_pcb = process_new_pcb();
    if (child_pcb == NULL) {
        return -1;
    }

    /* This is the PCB of the current (parent) process */
    pcb_t *parent_pcb = get_executing_pcb();

    /* TODO */
    return -1;
}

/* getargs() syscall handler */
int32_t
process_getargs(uint8_t *buf, int32_t nbytes)
{
    /* Ensure buffer is valid */
    if (!is_user_writable(buf, nbytes)) {
        return -1;
    }

    /* Can only read at most MAX_ARGS_LEN characters */
    if (nbytes > MAX_ARGS_LEN) {
        nbytes = MAX_ARGS_LEN;
    }

    /* Copy the args into the buffer */
    pcb_t *pcb = get_executing_pcb();
    strncpy((int8_t *)buf, (int8_t *)pcb->args, nbytes);

    return 0;
}

/* Initializes all process control related data */
void
process_init(void)
{
    /* TODO */
}
