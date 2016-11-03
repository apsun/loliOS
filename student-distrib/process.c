#include "process.h"
#include "lib.h"
#include "filesys.h"
#include "paging.h"
#include "debug.h"

/* The virtual address that the process should be copied to */
static uint8_t * const process_vaddr = (uint8_t *)0x8048000;

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

/*
 * Ensures that the given file is a valid executable file.
 * On success, writes the inode index of the file to out_inode_idx,
 * the starting address of the arguments to out_args, and returns 0.
 * Otherwise, returns -1.
 */
static int32_t
process_parse_exe(const uint8_t *command, uint32_t *out_inode_idx, const uint8_t **out_args)
{
    /* TODO: What if the pointer is invalid? */
    if (command == NULL) {
        return -1;
    }

    /*
     * Scan for the end of the exe filename
     * (@ == \0 in this diagram)
     *
     * Valid case:
     * cat    myfile.txt@
     *    |___|__________ i = 3 after loop terminates
     *        |__________ out_args
     *
     * Valid case:
     * ls@
     *   |___ i = 2
     *   |___ out_args
     *
     * Invalid case:
     * ccccccccccccaaaaaaaaaaaattttttttt myfile.txt@
     *                                  |___________ i = FNAME_LEN + 1
     */
    int32_t i;
    uint8_t filename[FNAME_LEN + 1];
    for (i = 0; i <= FNAME_LEN; ++i) {
        if (command[i] == ' ' || command[i] == '\0') {
            break;
        }
        filename[i] = command[i];
    }

    /* Filename too long! */
    if (i == FNAME_LEN + 1) {
        return -1;
    }

    /* NUL-terminate the filename */
    filename[i] = '\0';

    /* Read dentry for the file */
    dentry_t dentry;
    if (read_dentry_by_name(filename, &dentry) != 0) {
        return -1;
    }

    /* Can't load directories... */
    if (dentry.ftype != FTYPE_FILE) {
        return -1;
    }

    /* Read the magic bytes from the file */
    uint32_t magic;
    if (read_data(dentry.inode_idx, 0, (uint8_t *)&magic, 4) != 4) {
        return -1;
    }

    /* Ensure it's an executable file */
    if (magic != EXE_MAGIC) {
        return -1;
    }

    /* Write inode index */
    *out_inode_idx = dentry.inode_idx;

    /* Write starting address of args, skipping leading spaces */
    while (command[i] == ' ') {
        i++;
    }
    *out_args = &command[i];

    return 0;
}

static int32_t
process_init_pcb(void)
{
    
}

/*
 * Copies the program into memory. Returns the address of
 * the entry point of the program.
 *
 * You must point the process page to the correct physical
 * page before calling this!
 */
static uint32_t
process_load_exe(uint32_t inode_idx)
{
    uint32_t count;
    uint32_t offset = 0;
    do {
        /*
         * Copy the program to the process page.
         * The chunk size (4096B) is arbitrary.
         */
        count = read_data(inode_idx, offset, process_vaddr + offset, 4096);
        offset += count;
    } while (count > 0);

    /* The entry point is located at bytes 24-27 of the executable */
    uint32_t entry_point = *(uint32_t *)(process_vaddr + 24);
    return entry_point;
}

/*
 * Jumps into userspace at the given entry point and begins
 * executing the program
 */
static void
process_run(uint32_t entry_point)
{
    /* TODO */
}

/* execute() syscall handler */
int32_t
process_execute(const uint8_t *command)
{
    uint32_t inode;
    const uint8_t *args;

    /* First make sure we have a valid executable... */
    if (process_parse_exe(command, &inode, &args) != 0) {
        return -1;
    }

    /* Try to allocate a new PCB */
    pcb_t *child_pcb = process_new_pcb();
    if (child_pcb == NULL) {
        return -1;
    }

    /* This is the PCB of the current (parent) process */
    pcb_t *parent_pcb = get_executing_pcb();

    /* TODO: Initialize PCB (THIS CODE IS INCOMPLETE!!!) */
    child_pcb->parent_pid = parent_pcb->pid;
    read_register("ebp", child_pcb->parent_ebp);
    read_register("esp", child_pcb->parent_esp);
    strncpy((int8_t *)child_pcb->args, (const int8_t *)args, MAX_ARGS_LEN);

    /* TODO: Update TSS */

    /* Update the paging structures */
    paging_update_process_page(child_pcb->pid);

    /* Copy our program into physical memory */
    uint32_t entry_point = process_load_exe(inode);

    /* Jump to userspace and begin execution */
    process_run(entry_point);

    /*
     * We should never reach this point.
     * When the child process calls halt(), the ebp and esp of the
     * parent (the current stack frame) are restored. The "halt"
     * call then returns for us.
     */
    ASSERT(0);
    return -1;
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
