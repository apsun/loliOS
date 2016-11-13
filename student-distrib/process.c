#include "process.h"
#include "lib.h"
#include "filesys.h"
#include "paging.h"
#include "debug.h"
#include "x86_desc.h"

/* The virtual address that the process should be copied to */
static uint8_t * const process_vaddr = (uint8_t *)(USER_PAGE_START + 0x48000);

/* Process control blocks */
static pcb_t process_info[MAX_PROCESSES];

/* Kernel stack + pointer to PCB, one for each process */
__attribute__((aligned(PROCESS_DATA_SIZE)))
static process_data_t process_data[MAX_PROCESSES];

/*
 * Gets the PCB of the specified process.
 */
static pcb_t *
get_pcb(int32_t pid)
{
    /* When getting the parent of a "root" process */
    if (pid < 0) {
        return NULL;
    }

    ASSERT(pid < MAX_PROCESSES);
    ASSERT(process_info[pid].pid >= 0);
    return &process_info[pid];
}

/*
 * Gets the PCB of the currently executing process.
 *
 * This may only be called from a *process's* kernel stack
 * (that is, it must not be called during kernel init)!
 */
pcb_t *
get_executing_pcb(void)
{
    /*
     * Since the process data entries are 8KB-aligned, we can
     * extract the PCB pointer by masking the current kernel
     * ESP, which gives us the address of the executing process's
     * process_data_t struct.
     *
     * (8KB-aligned ESP)                        ESP
     *       |                                   |
     *       v                                   v
     *      [PCB|_____________KERNEL STACK_______________]
     *      <- lower addresses         higher addresses ->
     */
    uint32_t esp;
    read_register("esp", esp);
    process_data_t *data = (process_data_t *)(esp & ~(PROCESS_DATA_SIZE - 1));
    return data->pcb;
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
            process_info[i].pid = i;
            return &process_info[i];
        }
    }

    /* Reached max number of processes */
    return NULL;
}

/*
 * Ensures that the given file is a valid executable file.
 * On success, writes the inode index of the file to out_inode_idx,
 * the arguments to out_args, and returns 0. Otherwise, returns -1.
 */
static int32_t
process_parse_cmd(const uint8_t *command, uint32_t *out_inode_idx, uint8_t *out_args)
{
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

    /* Command index */
    int32_t i = 0;

    /* Strip leading whitespace */
    while (command[i] == ' ') {
        i++;
    }

    /* Read the filename (up to 33 chars with NUL terminator) */
    uint8_t filename[FNAME_LEN + 1];
    int32_t fname_i;
    for (fname_i = 0; fname_i < FNAME_LEN + 1; ++fname_i, ++i) {
        uint8_t c = command[i];
        if (c == ' ' || c == '\0') {
            filename[fname_i] = '\0';
            break;
        }
        filename[fname_i] = c;
    }

    /* If we didn't break out of the loop, the filename is too long */
    if (fname_i == FNAME_LEN + 1) {
        return -1;
    }

    /* Strip leading whitespace */
    while (command[i] == ' ') {
        i++;
    }

    /* Now copy the arguments to the arg buffer */
    int32_t args_i;
    for (args_i = 0; args_i < MAX_ARGS_LEN; ++args_i, ++i) {
        if ((out_args[args_i] = command[i]) == '\0') {
            break;
        }
    }

    /* Args are too long */
    if (args_i == MAX_ARGS_LEN) {
        return -1;
    }

    /* Read dentry for the file */
    dentry_t dentry;
    if (read_dentry_by_name(filename, &dentry) != 0) {
        return -1;
    }

    /* Can only execute files, obviously */
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

    return 0;
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
 * Jumps into userspace and executes the specified process.
 */
static int32_t
process_run(pcb_t *pcb)
{
    ASSERT(pcb != NULL);
    ASSERT(pcb->pid >= 0);

    /* Update paging structures */
    paging_update_process_page(pcb->pid);
    paging_update_vidmap_page(pcb->vidmap);

    /*
     * ESP0 points to bottom of the process kernel stack,
     * which is the same as the start of the process data of
     * the next process.
     *
     * (lower addresses)
     * |---------|
     * |  PID 0  |
     * |---------|
     * |  PID 1  |
     * |---------|<- ESP0 when new PID == 1
     * |   ...   |
     * (higher addresses)
     */
    tss.esp0 = (uint32_t)&process_data[pcb->pid + 1];

    /*
     * Save ESP and EBP of the current call frame so that we
     * can safely return from halt() inside the child.
     *
     * DO NOT MODIFY ANY CODE HERE UNLESS YOU ARE 100% SURE
     * ABOUT WHAT YOU ARE DOING!
     */
    asm volatile("movl %%esp, %0;"
                 "movl %%ebp, %1;"
                 : "=g"(pcb->parent_esp),
                   "=g"(pcb->parent_ebp)
                 :
                 : "memory");

    /* Jump into userspace and execute */
    asm volatile(
                 /* Segment registers */
                 "movw %1, %%ax;"
                 "movw %%ax, %%es;"
                 "movw %%ax, %%fs;"
                 "movw %%ax, %%gs;"

                 /* DS register */
                 "pushl %1;"

                 /* ESP */
                 "pushl %2;"

                 /* EFLAGS */
                 "pushfl;"
                 "popl %%eax;"
                 "orl $0x200, %%eax;" /* Set IF */
                 "pushl %%eax;"

                 /* CS register */
                 "pushl %3;"

                 /* EIP */
                 "pushl %0;"

                 /* GO! */
                 "iret;"

                 : /* No outputs */
                 : "r"(pcb->user_eip),
                   "g"(USER_DS),
                   "g"(USER_PAGE_END),
                   "g"(USER_CS)
                 : "eax", "cc");

    /* Can't touch this */
    ASSERT(0);
    return -1;
}

/*
 * Creates and initializes a new PCB for the given process.
 *
 * Implementation note: This is deliberately decoupled from
 * actually executing the process so it's easier to implement
 * context switching.
 */
static pcb_t *
process_create_child(const uint8_t *command, pcb_t *parent_pcb, int32_t terminal)
{
    uint32_t inode;
    uint8_t args[MAX_ARGS_LEN];

    /* First make sure we have a valid executable... */
    if (process_parse_cmd(command, &inode, args) != 0) {
        debugf("Invalid command/executable file\n");
        return NULL;
    }

    /* Try to allocate a new PCB */
    pcb_t *child_pcb = process_new_pcb();
    if (child_pcb == NULL) {
        debugf("Reached max number of processes\n");
        return NULL;
    }

    /* Initialize child PCB */
    if (parent_pcb == NULL) {
        /* This is the first process! */
        ASSERT(terminal >= 0);
        child_pcb->parent_pid = -1;
        child_pcb->terminal = terminal;
    } else {
        /* Inherit values from parent process */
        child_pcb->parent_pid = parent_pcb->pid;
        child_pcb->terminal = parent_pcb->terminal;
    }

    /* Common initialization */
    child_pcb->vidmap = false;
    file_init(child_pcb->files);
    strncpy((int8_t *)child_pcb->args, (const int8_t *)args, MAX_ARGS_LEN);

    /* Update PCB pointer in the kernel stack for this process */
    process_data[child_pcb->pid].pcb = child_pcb;

    /* Copy our program into physical memory
     *
     * TODO: This modifies the process page table entry.
     * We should probably find a better way to do this.
     */
    paging_update_process_page(child_pcb->pid);
    child_pcb->user_eip = process_load_exe(inode);

    return child_pcb;
}

/*
 * Process execute implementation.
 *
 * The command buffer will not be checked for validity, so
 * this can be called from either userspace or kernelspace.
 *
 * The terminal argument specifies which terminal to spawn
 * the process on if there is no parent.
 */
int32_t
process_execute_impl(const uint8_t *command, pcb_t *parent_pcb, int32_t terminal)
{
    /* Create the child process */
    pcb_t *child_pcb = process_create_child(command, parent_pcb, terminal);
    if (child_pcb == NULL) {
        debugf("Could not create child process\n");
        return -1;
    }

    /* Jump to userspace and begin execution */
    return process_run(child_pcb);
}

/* execute() syscall handler */
__cdecl int32_t
process_execute(const uint8_t *command)
{
    /* Validate command */
    if (!is_user_readable_string(command)) {
        debugf("Invalid string passed to process_execute\n");
        return -1;
    }

    /*
     * This should never be called directly from the kernel, so
     * there MUST be an executing process, so we can pass
     * -1 as the terminal since it will never be used anyways
     */
    return process_execute_impl(command, get_executing_pcb(), -1);
}

/*
 * Process halt implementation.
 *
 * Unlike process_halt(), the status is not truncated to 1 byte.
 */
int32_t
process_halt_impl(uint32_t status)
{
    /* This is the PCB of the child (halting) process */
    pcb_t *child_pcb = get_executing_pcb();

    /* Find parent process */
    pcb_t *parent_pcb = get_pcb(child_pcb->parent_pid);

    /* Close all open files */
    int32_t i;
    for (i = 2; i < MAX_FILES; ++i) {
        if (child_pcb->files[i].valid) {
            file_close(i);
        }
    }

    /* Free child PCB */
    child_pcb->pid = -1;

    if (parent_pcb != NULL) {
        /* Restore parent kernel stack TSS entry */
        tss.esp0 = (uint32_t)&process_data[child_pcb->parent_pid + 1];

        /* Restore the parent's paging structures */
        paging_update_process_page(parent_pcb->pid);
        paging_update_vidmap_page(parent_pcb->vidmap);
    } else {
        /* No parent process, just re-spawn a new shell in the same terminal */
        process_execute_impl((uint8_t *)"shell", NULL, child_pcb->terminal);

        /* Should never get back to this point */
        ASSERT(0);
    }

    /*
     * This "returns" from the PARENT'S execute() call
     * by restoring its esp/ebp and executing leave + ret,
     * which bypasses the execute() call frame and jumps directly
     * to its caller.
     *
     * DO NOT MODIFY ANY CODE HERE UNLESS YOU ARE 100% SURE
     * ABOUT WHAT YOU ARE DOING!
     */
    asm volatile("movl %1, %%esp;"
                 "movl %2, %%ebp;"
                 "movl %0, %%eax;"
                 "leave;"
                 "ret;"
                 :
                 : "r"(status),
                   "r"(child_pcb->parent_esp),
                   "r"(child_pcb->parent_ebp)
                 : "esp", "ebp", "eax");

    /* Should never get here! */
    ASSERT(0);
    return -1;
}

/* halt() syscall handler */
__cdecl int32_t
process_halt(uint32_t status)
{
    /*
     * Only the lowest byte is used, rest are reserved
     * This only applies when this is called via syscall;
     * the kernel must still be able to halt a process
     * with a status > 255.
     */
    return process_halt_impl(status & 0xff);
}

/* getargs() syscall handler */
__cdecl int32_t
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

/* vidmap() syscall handler */
__cdecl int32_t
process_vidmap(uint8_t **screen_start)
{
    /* Ensure buffer is valid */
    if (!is_user_writable(screen_start, sizeof(uint8_t *))) {
        return -1;
    }

    /* Set the vidmap page to present */
    *screen_start = paging_update_vidmap_page(true);

    /* Save vidmap state in PCB */
    pcb_t *pcb = get_executing_pcb();
    pcb->vidmap = true;

    return 0;
}

/* set_handler() syscall handler */
__cdecl int32_t
process_set_handler(int32_t signum, void *handler_address)
{
    /* Not yet implemented */
    return -1;
}

/* sigreturn() syscall handler */
__cdecl int32_t
process_sigreturn(void)
{
    /* Not yet implemented */
    return -1;
}

/* Initializes all process control related data */
void
process_init(void)
{
    int32_t i;
    for (i = 0; i < MAX_PROCESSES; ++i) {
        process_info[i].pid = -1;
    }
}

/* She spawns C shells by the seashore */
void
process_start_shell(void)
{
    /* Make sure nobody stops us... */
    cli();

    /* Execute a shell in the first terminal */
    process_execute_impl((uint8_t *)"shell", NULL, 0);

    /* TODO: I can haz more shells? */
}
