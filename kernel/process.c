#include "process.h"
#include "lib.h"
#include "debug.h"
#include "filesys.h"
#include "paging.h"
#include "terminal.h"
#include "x86_desc.h"
#include "rtc.h"

/* Maximum length of string passed to execute() */
#define MAX_EXEC_LEN 128

/* Maximum number of processes */
#define MAX_PROCESSES 6

/* Executable magic bytes ('\x7fELF') */
#define EXE_MAGIC 0x464c457f

/* Process data block size, MUST BE A POWER OF 2! */
#define PROCESS_DATA_SIZE 8192

/* The virtual address that the process should be copied to */
#define PROCESS_VADDR (USER_PAGE_START + 0x48000)

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

/* Kernel stack struct */
typedef struct {
    pcb_t *pcb;
    uint8_t kernel_stack[PROCESS_DATA_SIZE - sizeof(pcb_t *)];
} process_data_t;

/* Process control blocks */
static pcb_t process_info[MAX_PROCESSES];

/* Kernel stack + pointer to PCB, one for each process */
__aligned(PROCESS_DATA_SIZE)
static process_data_t process_data[MAX_PROCESSES];

/*
 * Gets the PCB of the specified process.
 */
pcb_t *
get_pcb_by_pid(int pid)
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
 * Gets the PCB of the currently executing process
 * in the specified terminal.
 */
pcb_t *
get_pcb_by_terminal(int terminal)
{
    int i;
    for (i = 0; i < MAX_PROCESSES; ++i) {
        pcb_t *pcb = &process_info[i];
        if (pcb->pid >= 0 &&                /* Valid? */
            pcb->terminal == terminal &&    /* Same terminal? */
            pcb->status != PROCESS_SLEEP) { /* Running? */
            return pcb;
        }
    }
    return NULL;
}

/*
 * Finds the next process that is scheduled for execution. If
 * there are no other process that can be executed, returns the
 * current process.
 */
static pcb_t *
get_next_pcb(void)
{
    pcb_t *curr_pcb = get_executing_pcb();
    int i;
    for (i = 1; i < MAX_PROCESSES; ++i) {
        int pid = (curr_pcb->pid + i) % MAX_PROCESSES;
        pcb_t *pcb = &process_info[pid];
        if (pcb->pid >= 0 && pcb->status != PROCESS_SLEEP) {
            return pcb;
        }
    }

    /* If nothing else can be executed, just return the current one */
    return curr_pcb;
}

/*
 * Allocates a new PCB. Returns a pointer to the PCB, or
 * NULL if the maximum number of processes are already
 * running.
 */
static pcb_t *
process_new_pcb(void)
{
    /* Look for an empty process slot we can fill */
    int i;
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
static int
process_parse_cmd(const char *command, int *out_inode_idx, char *out_args)
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
     *                                  |___________ i = MAX_FILENAME_LEN + 1
     */

    /* Command index */
    int i = 0;

    /* Strip leading whitespace */
    while (command[i] == ' ') {
        i++;
    }

    /* Read the filename (up to 33 chars with NUL terminator) */
    char filename[MAX_FILENAME_LEN + 1];
    int fname_i;
    for (fname_i = 0; fname_i < MAX_FILENAME_LEN + 1; ++fname_i, ++i) {
        char c = command[i];
        if (c == ' ' || c == '\0') {
            filename[fname_i] = '\0';
            break;
        }
        filename[fname_i] = c;
    }

    /* If we didn't break out of the loop, the filename is too long */
    if (fname_i == MAX_FILENAME_LEN + 1) {
        debugf("Filename too long\n");
        return -1;
    }

    debugf("Trying to execute: %s\n", filename);

    /* Strip leading whitespace */
    while (command[i] == ' ') {
        i++;
    }

    /* Now copy the arguments to the arg buffer */
    int args_i;
    for (args_i = 0; args_i < MAX_ARGS_LEN; ++args_i, ++i) {
        if ((out_args[args_i] = command[i]) == '\0') {
            break;
        }
    }

    /* Args are too long */
    if (args_i == MAX_ARGS_LEN) {
        debugf("Args too long\n");
        return -1;
    }

    /* Read dentry for the file */
    dentry_t dentry;
    if (read_dentry_by_name(filename, &dentry) != 0) {
        debugf("Cannot find dentry\n");
        return -1;
    }

    /* Can only execute files, obviously */
    if (dentry.type != FTYPE_FILE) {
        debugf("Can only execute files\n");
        return -1;
    }

    /* Read the magic bytes from the file */
    uint32_t magic;
    if (read_data(dentry.inode_idx, 0, (uint8_t *)&magic, sizeof(magic)) != sizeof(magic)) {
        debugf("Could not read magic\n");
        return -1;
    }

    /* Ensure it's an executable file */
    if (magic != EXE_MAGIC) {
        debugf("Magic mismatch - not an executable (got 0x%#x)\n", magic);
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
process_load_exe(int inode_idx)
{
    /* Copy program into memory */
    int count;
    int offset = 0;
    do {
        uint8_t *vaddr = (uint8_t *)PROCESS_VADDR + offset;
        count = read_data(inode_idx, offset, vaddr, MB(4));
        offset += count;
    } while (count > 0);

    /* Clear user page contents for security */
    uint32_t head_size = PROCESS_VADDR - USER_PAGE_START;
    uint32_t tail_size = USER_PAGE_END - PROCESS_VADDR - offset;
    memset((uint8_t *)USER_PAGE_START, 0, head_size);
    memset((uint8_t *)PROCESS_VADDR + offset, 0, tail_size);

    /*
     * The entry point is located at bytes 24-27 of the executable.
     * If the "executable" is less than 28 bytes long, this will just
     * read garbage, which will cause the program to fault in userspace.
     * No need to handle it here.
     */
    uint32_t entry_point = *(uint32_t *)(PROCESS_VADDR + 24);
    return entry_point;
}

/*
 * Gets the address of the bottom of the kernel stack
 * for the specified process.
 */
static uint32_t
get_kernel_base_esp(pcb_t *pcb)
{
    /*
     * ESP0 points to bottom of the process kernel stack.
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
    uint32_t stack_start = (uint32_t)process_data[pcb->pid].kernel_stack;
    uint32_t stack_size = sizeof(process_data[pcb->pid].kernel_stack);
    return stack_start + stack_size;
}

/*
 * Sets the global execution context to the specified process.
 */
static void
process_set_context(pcb_t *to)
{
    /* Restore process page */
    paging_set_context(to->pid, &to->heap);

    /* Restore vidmap status */
    terminal_update_vidmap(to->terminal, to->vidmap);

    /* Restore TSS entry */
    tss.esp0 = get_kernel_base_esp(to);
}

/*
 * Jumps into userspace and executes the specified process.
 */
__noinline static int
process_run(pcb_t *pcb)
{
    int ret;

    ASSERT(pcb != NULL);
    ASSERT(pcb->pid >= 0);

    /* Mark process as initialized */
    pcb->status = PROCESS_RUN;

    /* Clear process's terminal input buffers */
    terminal_clear_input(pcb->terminal);

    /* Set the global execution context */
    process_set_context(pcb);

    /*
     * Save ESP and EBP of the current call frame so that we
     * can safely return from halt() inside the child, then
     * jump into userspace and execute.
     *
     * DO NOT MODIFY ANY CODE HERE UNLESS YOU ARE 100% SURE
     * ABOUT WHAT YOU ARE DOING!
     */
    asm volatile(
        /* Save ESP and EBP */
        "movl %%esp, (%1);"
        "movl %%ebp, (%2);"

        /* Segment registers */
        "movw %4, %%ax;"
        "movw %%ax, %%ds;"
        "movw %%ax, %%es;"
        "movw %%ax, %%fs;"
        "movw %%ax, %%gs;"

        /* SS register */
        "pushl %4;"

        /* ESP */
        "pushl %5;"

        /* EFLAGS */
        "pushfl;"
        "popl %%eax;"
        "orl $0x200, %%eax;" /* Set IF */
        "pushl %%eax;"

        /* CS register */
        "pushl %6;"

        /* EIP */
        "pushl %3;"

        /* Zero all general purpose registers for security */
        "xorl %%eax, %%eax;"
        "xorl %%ebx, %%ebx;"
        "xorl %%ecx, %%ecx;"
        "xorl %%edx, %%edx;"
        "xorl %%esi, %%esi;"
        "xorl %%edi, %%edi;"
        "xorl %%ebp, %%ebp;"

        /* GO! */
        "iret;"

        /* Get back here from halt */
        "process_run_ret:"

        : "=a"(ret)
        : "b"(&pcb->parent_esp),
          "c"(&pcb->parent_ebp),
          "d"(pcb->entry_point),
          "i"(USER_DS),
          "i"(USER_PAGE_END),
          "i"(USER_CS)
        : "esi", "edi", "cc", "memory");

    return ret;
}

/*
 * Creates and initializes a new PCB for the given process.
 *
 * Implementation note: This is deliberately decoupled from
 * actually executing the process so it's easier to implement
 * context switching.
 */
static pcb_t *
process_create_child(const char *command, pcb_t *parent_pcb, int terminal)
{
    int inode;
    char args[MAX_ARGS_LEN];

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
    child_pcb->status = PROCESS_SCHED;
    child_pcb->vidmap = false;
    child_pcb->last_alarm = rtc_get_counter();
    signal_init(child_pcb->signals);
    file_init(child_pcb->files);
    paging_heap_init(&child_pcb->heap);
    strncpy(child_pcb->args, args, MAX_ARGS_LEN);

    /* Update PCB pointer in the kernel data for this process */
    process_data[child_pcb->pid].pcb = child_pcb;

    /* Copy our program into physical memory */
    paging_set_context(child_pcb->pid, &child_pcb->heap);
    child_pcb->entry_point = process_load_exe(inode);

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
static int
process_execute_impl(const char *command, pcb_t *parent_pcb, int terminal)
{
    /* Create the child process */
    pcb_t *child_pcb = process_create_child(command, parent_pcb, terminal);
    if (child_pcb == NULL) {
        debugf("Could not create child process\n");
        return -1;
    }

    /* If there's a parent process, stop executing it */
    if (parent_pcb != NULL) {
        parent_pcb->status = PROCESS_SLEEP;
    }

    /* Jump into userspace and begin executing the program */
    return process_run(child_pcb);
}

/* execute() syscall handler */
__cdecl int
process_execute(const char *command)
{
    char tmp[MAX_EXEC_LEN];
    if (!strscpy_from_user(tmp, command, sizeof(tmp))) {
        debugf("Executed string too long or invalid\n");
        return -1;
    }

    /*
     * This should never be called directly from the kernel, so
     * there MUST be an executing process, so we can pass
     * -1 as the terminal since it will never be used anyways
     */
    return process_execute_impl(tmp, get_executing_pcb(), -1);
}

/*
 * Process halt implementation.
 *
 * Unlike process_halt(), the status is not truncated to 1 byte.
 */
int
process_halt_impl(int status)
{
    /* This is the PCB of the child (halting) process */
    pcb_t *child_pcb = get_executing_pcb();

    /* Find parent process */
    pcb_t *parent_pcb = get_pcb_by_pid(child_pcb->parent_pid);

    /* Close all open files */
    int i;
    for (i = 2; i < MAX_FILES; ++i) {
        if (child_pcb->files[i].valid) {
            file_close(i);
        }
    }

    /* Free all heap pages allocated by this process */
    paging_heap_destroy(&child_pcb->heap);

    /* Clear terminal input buffers */
    terminal_clear_input(child_pcb->terminal);

    /* Mark child PCB as free */
    child_pcb->pid = -1;

    /* If no parent process, just re-spawn a new shell in the same terminal */
    if (parent_pcb == NULL) {
        process_execute_impl("shell", NULL, child_pcb->terminal);

        /* Should never get back to this point */
        PANIC("Should not have returned from shell");
    }

    /* Mark parent as runnable again */
    parent_pcb->status = PROCESS_RUN;

    /* Set the global execution context */
    process_set_context(parent_pcb);

    /*
     * This returns back into the PARENT'S process_run call frame
     * by restoring its esp/ebp and doing a cross-function JMP.
     *
     * DO NOT MODIFY ANY CODE HERE UNLESS YOU ARE 100% SURE
     * ABOUT WHAT YOU ARE DOING!
     */
    asm volatile(
        "movl %1, %%esp;"
        "movl %2, %%ebp;"
        "movl %0, %%eax;"
        "jmp process_run_ret;"
        :
        : "r"(status),
          "r"(child_pcb->parent_esp),
          "r"(child_pcb->parent_ebp)
        : "eax");

    /* Should never get here! */
    PANIC("Should not have returned from halt");
    return -1;
}

/*
 * Switches execution to the next scheduled process.
 */
__used __cdecl static void
process_switch_impl(void)
{
    pcb_t *curr = get_executing_pcb();
    pcb_t *next = get_next_pcb();
    if (curr == next) {
        return;
    }

    /*
     * Save current stack pointer so we can "return" to this
     * stack frame.
     */
    asm volatile(
        "movl %%esp, %0;"
        "movl %%ebp, %1;"
        : "=g"(curr->kernel_esp),
          "=g"(curr->kernel_ebp));

    if (next->status == PROCESS_SCHED) {
        /*
         * If we're in this block, we're initializing
         * one of the three initial shells. We don't set
         * the stack pointer because:
         *
         * 1) It's never used
         * 2) It's not initialized anyways
         */
        process_run(next);
    } else if (next->status == PROCESS_RUN) {
        /*
         * If we're in this block, the next process must be
         * in a process_switch_impl call too. We just switch
         * into its stack and return.
         */

        /* Set global execution context */
        process_set_context(next);

        /* "Return" into the other process's process_switch_impl frame */
        asm volatile(
            "movl %0, %%esp;"
            "movl %1, %%ebp;"
            :
            : "r"(next->kernel_esp),
              "r"(next->kernel_ebp));
    }
}

/*
 * Wrapper for process_switch_impl that clobbers the
 * appropriate registers.
 */
void
process_switch(void)
{
    asm volatile(
        "call process_switch_impl;"
        :
        :
        : "eax", "ebx", "ecx", "edx", "esi", "edi", "cc", "memory");
}

/* halt() syscall handler */
__cdecl int
process_halt(int status)
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
__cdecl int
process_getargs(char *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    }

    pcb_t *pcb = get_executing_pcb();

    /*
     * Compute length of arguments. If they are empty, then we
     * should fail, as per the spec.
     */
    int length = strlen(pcb->args) + 1;
    if (length == 1) {
        return -1;
    }

    /*
     * Limit the number of characters read (include NUL). Note
     * that we fail if the buffer is too small, as per the spec.
     */
    if (nbytes > length) {
        nbytes = length;
    } else if (nbytes < length) {
        return -1;
    }

    /* Copy arguments to userspace */
    if (!copy_to_user(buf, pcb->args, nbytes)) {
        return -1;
    }

    return 0;
}

/* vidmap() syscall handler */
__cdecl int
process_vidmap(uint8_t **screen_start)
{
    pcb_t *pcb = get_executing_pcb();

    /* Check and copy before actually enabling vidmap */
    uint8_t *addr = (uint8_t *)VIDMAP_PAGE_START;
    if (!copy_to_user(screen_start, &addr, sizeof(addr))) {
        return -1;
    }

    /* Update vidmap status */
    terminal_update_vidmap(pcb->terminal, true);

    /* Save vidmap state in PCB */
    pcb->vidmap = true;

    return 0;
}

/* sbrk() syscall handler */
__cdecl int
process_sbrk(int delta)
{
    pcb_t *pcb = get_executing_pcb();
    return paging_heap_sbrk(&pcb->heap, delta);
}

/* Initializes all process control related data */
void
process_init(void)
{
    ASSERT(sizeof(process_data_t) == PROCESS_DATA_SIZE);

    int i;
    for (i = 0; i < MAX_PROCESSES; ++i) {
        process_info[i].pid = -1;
    }
}

/* She spawns C shells by the seashore */
void
process_start_shell(void)
{
    int i;
    for (i = 1; i < NUM_TERMINALS; ++i) {
        process_create_child("shell", NULL, i);
    }
    process_execute_impl("shell", NULL, 0);
}

/*
 * Handles RTC updates by sending an alarm signal
 * to each process every 10 seconds since its creation.
 */
void
process_update_clock(int rtc_counter)
{
    int i;
    for (i = 0; i < MAX_PROCESSES; ++i) {
        pcb_t *pcb = &process_info[i];
        if (pcb->pid >= 0) {
            int elapsed_time = rtc_counter - pcb->last_alarm;
            if (elapsed_time >= MAX_RTC_FREQ * SIG_ALARM_PERIOD) {
                pcb->last_alarm = rtc_counter;
                signal_raise(pcb->pid, SIG_ALARM);
            }
        }
    }
}
