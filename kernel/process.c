#include "process.h"
#include "types.h"
#include "debug.h"
#include "string.h"
#include "filesys.h"
#include "paging.h"
#include "elf.h"
#include "terminal.h"
#include "pit.h"
#include "timer.h"
#include "x86_desc.h"
#include "scheduler.h"
#include "syscall.h"

/* Maximum length of string passed to execute()/exec() */
#define MAX_EXEC_LEN 128

/* Maximum number of processes, including idle process */
#define MAX_PROCESSES 16

/* Process data block size, MUST BE A POWER OF 2! */
#define PROCESS_DATA_SIZE 8192

/* Name of the userspace program to execute on boot */
#define INIT_PROCESS "shell"

/* Period of the alarm signal in milliseconds */
#define SIGALRM_PERIOD_MS 10000

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

/* Sleep queue for processes wait()ing on another process */
static list_declare(wait_queue);

/* Sleep queue for processes that called sleep() */
static list_declare(sleep_queue);

/*
 * Gets the PCB of the process with the given PID.
 * This does NOT include the idle process. Note that
 * the process may already be dead; make sure to check
 * the process state to ensure it is not a zombie.
 */
pcb_t *
get_pcb(int pid)
{
    if (pid <= 0 || pid > MAX_PROCESSES) {
        return NULL;
    }

    pcb_t *pcb = &process_info[pid];
    if (pcb->pid < 0) {
        return NULL;
    }

    return pcb;
}

/*
 * Returns the PCB of the idle process.
 */
pcb_t *
get_idle_pcb(void)
{
    return &process_info[0];
}

/*
 * Iterator API for PCB objects. Call this with NULL
 * as a parameter to get the first process; call this
 * with the first process to get the second process;
 * and so on. When all processes have been exhausted,
 * this returns NULL. Note that this is NOT stateful;
 * so all iterations must be executed in one go. This
 * does NOT include the idle process.
 */
pcb_t *
get_next_pcb(pcb_t *pcb)
{
    /* Task 0 always refers to the idle task */
    if (pcb == NULL) {
        pcb = &process_info[0];
    }

    pcb_t *next;
    for (next = pcb + 1; next < &process_info[MAX_PROCESSES]; ++next) {
        if (next->pid > 0) {
            return next;
        }
    }
    return NULL;
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
    asm volatile("movl %%esp, %0" : "=g"(esp));
    process_data_t *data = (process_data_t *)(esp & ~(PROCESS_DATA_SIZE - 1));
    return data->pcb;
}

/*
 * Allocates a new PCB. Returns a pointer to the PCB, or
 * NULL if the maximum number of processes are already
 * running.
 */
static pcb_t *
process_alloc_pcb(void)
{
    /* Look for an empty process slot we can fill */
    int i;
    for (i = 0; i < MAX_PROCESSES; ++i) {
        pcb_t *pcb = &process_info[i];
        if (pcb->pid < 0) {
            pcb->pid = i;
            process_data[i].pcb = pcb;
            return pcb;
        }
    }

    /* Reached max number of processes */
    return NULL;
}

/*
 * Frees an allocated PCB. This does NOT release any
 * resource used by the PCB.
 */
static void
process_free_pcb(pcb_t *pcb)
{
    pcb->pid = -1;
}

/*
 * Parses a command in the format "(space*)<cmd>[(space+)<args>]".
 * The input command must be a string in kernel memory, and may be
 * modified by this function.
 *
 * On success, writes the inode index of the file to out_inode_idx,
 * the arguments to out_args, whether the program should be loaded
 * in compatibility mode to out_compat, and returns 0. Otherwise,
 * returns < 0.
 */
static int
process_parse_cmd(
    char *command,
    int *out_inode_idx,
    char *out_args,
    bool *out_compat)
{
    /* Strip leading whitespace */
    command += strspn(command, " ");

    /* Filename is everything up to the first space */
    char *filename = strsep(&command, " ");

    /* Read dentry for the file */
    dentry_t *dentry;
    if (fs_dentry_by_name(filename, &dentry) != 0) {
        debugf("Cannot find dentry\n");
        return -1;
    }

    /* Can only execute files, obviously */
    if (dentry->type != FILE_TYPE_FILE) {
        debugf("Can only execute files\n");
        return -1;
    }

    /* Check that it's a valid ELF file */
    if (!elf_is_valid(dentry->inode_idx, out_compat)) {
        return -1;
    }

    /* Remainder of the command becomes the arguments */
    if (command != NULL) {
        command += strspn(command, " ");

        /* Don't clobber out_args unless successful */
        if (strlen(command) >= MAX_ARGS_LEN) {
            debugf("Arguments too long\n");
            return -1;
        }

        strcpy(out_args, command);
    } else {
        *out_args = '\0';
    }

    *out_inode_idx = (int)dentry->inode_idx;
    return 0;
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
 * Unsets the global execution context for the specified process.
 */
void
process_unset_context(pcb_t *pcb)
{
    heap_unmap(&pcb->heap);
}

/*
 * Sets the global execution context for the specified process.
 */
void
process_set_context(pcb_t *pcb)
{
    paging_map_user_page(pcb->user_paddr);
    heap_map(&pcb->heap);
    terminal_update_vidmap(pcb->terminal, pcb->vidmap);

    /* Restore TSS entry */
    tss.esp0 = get_kernel_base_esp(pcb);
}

/*
 * Copies the given interrupt context onto the specified
 * kernel stack, then performs the IRET on behalf of that
 * process. This function does not return.
 */
static void
process_iret(int_regs_t *regs, void *kernel_stack)
{
    /*
     * Copy the interrupt context to the bottom of the stack.
     * Note that if IRET'ing to kernel mode, the bottom 8 bytes
     * of the stack are wasted (they hold the unused ESP/SS).
     */
    int_regs_t *dest = kernel_stack;
    memcpy(--dest, regs, sizeof(int_regs_t));

    /* Unwind the stack starting from that point */
    asm volatile(
        "movl %0, %%esp;"
        "jmp idt_unwind_stack;"
        :
        : "g"(dest));

    panic("Should not return from process_iret()");
}

/*
 * SIGALRM timer callback, raises the signal and
 * restarts the timer.
 */
static void
process_alarm_callback(timer_t *timer)
{
    pcb_t *pcb = timer_entry(timer, pcb_t, alarm_timer);
    signal_kill(pcb->pid, SIGALRM);
    timer_setup(timer, SIGALRM_PERIOD_MS, process_alarm_callback);
}

/*
 * Executes the specified process for the first time. This
 * function does not return. The process must be in the NEW
 * state.
 */
void
process_run(pcb_t *pcb)
{
    assert(pcb != NULL);
    assert(pcb->pid >= 0);
    assert(pcb->state == PROCESS_STATE_NEW);

    /* Mark process as initialized */
    pcb->state = PROCESS_STATE_RUNNING;

    /* Set the global execution context */
    process_set_context(pcb);

    /* Perform a fake IRET on behalf of the process */
    process_iret(&pcb->regs, (void *)get_kernel_base_esp(pcb));
}

/*
 * Idle loop "process". Basically just handles interrupts
 * endlessly. This is the only place in the kernel where
 * interrupts are enabled.
 */
static void
process_idle(void)
{
    while (1) {
        /*
         * Note that there is no race condition between sti and
         * hlt here - sti only takes effect after the next instruction
         * has executed. If an interrupt occurred between sti and hlt,
         * it would be handled after hlt executes and hlt would
         * return immediately. Also use a single asm block, or else
         * the compiler might insert extra instructions between sti
         * and hlt.
         */
        asm volatile("sti; hlt; cli" ::: "memory");

        /*
         * Immediately yield back to the scheduler, in case the
         * interrupt woke up a normal process. If it turns out that
         * there is nothing to do, we will just come right back.
         */
        scheduler_yield();
    }
}

/*
 * Initializes the given registers as appropriate for
 * executing a userspace process.
 */
static void
process_fill_user_regs(int_regs_t *regs, uintptr_t entry_point)
{
    uint32_t eflags;
    asm volatile("pushfl; popl %0" : "=r"(eflags));

    regs->ds = USER_DS;
    regs->es = USER_DS;
    regs->fs = USER_DS;
    regs->gs = USER_DS;
    regs->eax = 0;
    regs->ebx = 0;
    regs->ecx = 0;
    regs->edx = 0;
    regs->esi = 0;
    regs->edi = 0;
    regs->ebp = 0;
    regs->eip = (uint32_t)entry_point;
    regs->cs = USER_CS;
    regs->eflags = (eflags & ~EFLAGS_USER) | EFLAGS_IF;
    regs->esp = USER_PAGE_END;
    regs->ss = USER_DS;
}

/*
 * Initializes the registers used to schedule the idle task.
 */
static void
process_fill_idle_regs(int_regs_t *regs)
{
    uint32_t eflags;
    asm volatile("pushfl; popl %0" : "=r"(eflags));

    regs->ds = KERNEL_DS;
    regs->es = KERNEL_DS;
    regs->fs = KERNEL_DS;
    regs->gs = KERNEL_DS;
    regs->eax = 0;
    regs->ebx = 0;
    regs->ecx = 0;
    regs->edx = 0;
    regs->esi = 0;
    regs->edi = 0;
    regs->ebp = 0;
    regs->eip = (uint32_t)process_idle;
    regs->cs = KERNEL_CS;
    regs->eflags = eflags & ~EFLAGS_USER;
}

/*
 * Releases all resources used by the given PCB, without
 * freeing it. This will also remove it from the scheduler.
 */
static void
process_close(pcb_t *pcb)
{
    file_deinit(pcb->files);
    timer_cancel(&pcb->alarm_timer);
    heap_clear(&pcb->heap);
    paging_page_free(pcb->user_paddr);
    scheduler_remove(pcb);
}

/*
 * Creates the idle process state. This must be called
 * before creating any other processes.
 */
static pcb_t *
process_create_idle(void)
{
    pcb_t *pcb = process_alloc_pcb();
    assert(pcb != NULL && pcb->pid == 0);

    pcb->state = PROCESS_STATE_NEW;
    pcb->parent_pid = -1;
    pcb->terminal = 0;
    pcb->user_paddr = 0;
    pcb->compat = false;
    pcb->group = pcb->pid;
    pcb->vidmap = false;
    file_init(pcb->files);
    signal_init(pcb->signals);
    timer_init(&pcb->alarm_timer);
    timer_init(&pcb->sleep_timer);
    heap_init_kernel(&pcb->heap, 0, 0, NULL);

    process_fill_idle_regs(&pcb->regs);
    return pcb;
}

/*
 * Creates a process from scratch. This is used to spawn
 * the initial shell processes. Warning: this will clobber
 * the current paging context!
 */
static pcb_t *
process_create_user(char *command, int terminal)
{
    /* Try to allocate a new PCB */
    pcb_t *pcb = process_alloc_pcb();
    if (pcb == NULL) {
        debugf("Reached max number of processes\n");
        return NULL;
    }

    /* Parse command and find the executable inode */
    int inode_idx;
    bool compat;
    if (process_parse_cmd(command, &inode_idx, pcb->args, &compat) < 0) {
        debugf("Invalid command/executable file\n");
        process_free_pcb(pcb);
        return NULL;
    }

    /* Allocate physical memory to hold process */
    uintptr_t paddr = paging_page_alloc();
    if (paddr == 0) {
        debugf("Cannot allocate page for process\n");
        process_free_pcb(pcb);
        return NULL;
    }

    /* A bunch of initialization follows... */
    pcb->state = PROCESS_STATE_NEW;
    pcb->parent_pid = -1;
    pcb->terminal = terminal;
    pcb->user_paddr = paddr;
    pcb->group = pcb->pid;
    pcb->vidmap = false;
    pcb->compat = compat;
    file_init(pcb->files);
    terminal_open_streams(pcb->files);
    signal_init(pcb->signals);
    timer_init(&pcb->alarm_timer);
    timer_setup(&pcb->alarm_timer, SIGALRM_PERIOD_MS, process_alarm_callback);
    timer_init(&pcb->sleep_timer);
    heap_init_user(&pcb->heap, USER_HEAP_START, USER_HEAP_END);

    /* Set terminal foreground group since this is the only process */
    terminal_tcsetpgrp_impl(terminal, pcb->group);

    /* Copy our program into physical memory */
    uintptr_t entry_point = elf_load(inode_idx, paddr, pcb->compat);
    process_fill_user_regs(&pcb->regs, entry_point);

    /* Finally, schedule this process for execution */
    scheduler_add(pcb);
    return pcb;
}

/*
 * Clones the specified process. regs points to the original
 * process's interrupt context on the stack. If clone_pages is
 * false, the user and heap pages will NOT be cloned, which is
 * useful if this is immediately followed by exec().
 */
static pcb_t *
process_clone(pcb_t *parent_pcb, int_regs_t *regs, bool clone_pages)
{
    /* Try to allocate a new PCB */
    pcb_t *child_pcb = process_alloc_pcb();
    if (child_pcb == NULL) {
        debugf("Reached max number of processes\n");
        return NULL;
    }

    /* Allocate physical memory to hold process */
    uintptr_t paddr = paging_page_alloc();
    if (paddr == 0) {
        debugf("Cannot allocate page for child process\n");
        process_free_pcb(child_pcb);
        return NULL;
    }

    /* First try to clone the heap, since that can fail */
    if (clone_pages) {
        if (heap_clone(&child_pcb->heap, &parent_pcb->heap) < 0) {
            debugf("Cannot allocate heap for child process\n");
            paging_page_free(paddr);
            process_free_pcb(child_pcb);
            return NULL;
        }
    } else {
        heap_init_user(&child_pcb->heap, USER_HEAP_START, USER_HEAP_END);
    }

    /* Some state isn't cloned - set those here */
    child_pcb->state = PROCESS_STATE_NEW;
    child_pcb->parent_pid = parent_pcb->pid;
    child_pcb->user_paddr = paddr;

    /* Set "return" value to zero in child */
    child_pcb->regs = *regs;
    child_pcb->regs.eax = 0;

    /* Clone the remaining state from the parent */
    child_pcb->compat = parent_pcb->compat;
    child_pcb->terminal = parent_pcb->terminal;
    child_pcb->vidmap = parent_pcb->vidmap;
    child_pcb->group = parent_pcb->group;
    file_clone(child_pcb->files, parent_pcb->files);
    signal_clone(child_pcb->signals, parent_pcb->signals);
    timer_clone(&child_pcb->alarm_timer, &parent_pcb->alarm_timer);
    strcpy(child_pcb->args, parent_pcb->args);

    /* Clone user page into child */
    if (clone_pages) {
        paging_clone_user_page(paddr);
    }

    /* Schedule child for execution */
    scheduler_add(child_pcb);

    return child_pcb;
}

/*
 * Performs an exec() on behalf of the specified process.
 * regs must point to the saved interrupt context on the
 * stack if the process has already been into userspace
 * (i.e. is calling exec()), or pcb->regs otherwise.
 */
static int
process_exec_impl(pcb_t *pcb, int_regs_t *regs, const char *command)
{
    /* Copy command into kernel memory */
    char cmd[MAX_EXEC_LEN];
    if (strscpy_from_user(cmd, command, sizeof(cmd)) < 0) {
        debugf("Executed string too long or invalid\n");
        return -1;
    }

    /* Parse command and find the executable inode */
    int inode_idx;
    bool compat;
    if (process_parse_cmd(cmd, &inode_idx, pcb->args, &compat) < 0) {
        debugf("Invalid command/executable file\n");
        return -1;
    }

    /* Set compatibility mode */
    pcb->compat = compat;

    /* Reset all signal state */
    signal_init(pcb->signals);

    /* Reset child process heap */
    heap_clear(&pcb->heap);

    /* Restart SIGALRM timer */
    timer_setup(&pcb->alarm_timer, SIGALRM_PERIOD_MS, process_alarm_callback);

    /* Copy our program into physical memory */
    uintptr_t entry_point = elf_load(inode_idx, pcb->user_paddr, pcb->compat);

    /* Replace interrupt context used to return into userspace */
    process_fill_user_regs(regs, entry_point);

    return 0;
}

/*
 * wait() implementation. Note that this is non-blocking,
 * and will return -EAGAIN if no processes are ready to be
 * reaped. To implement a blocking wait(), simply call this
 * in a loop.
 */
static int
process_wait_impl(int parent_pid, int *pid)
{
    int kpid = *pid;
    bool exists = false;

    pcb_t *pcb;
    process_for_each(pcb) {
        /* Can't reap other people's children */
        if (pcb->parent_pid != parent_pid) {
            continue;
        }

        /* Check if PID matches our query */
        if (pcb->pid != kpid && pcb->group != -kpid) {
            continue;
        }

        /* Okay, so at least one process matching pid exists */
        exists = true;

        /* If it's dead, reap it and we're done! */
        if (pcb->state == PROCESS_STATE_ZOMBIE) {
            int exit_code = pcb->exit_code;
            *pid = pcb->pid;
            process_free_pcb(pcb);
            return exit_code;
        }
    }

    /* If the process doesn't exist, fail instead of retrying */
    if (!exists) {
        return -1;
    } else {
        return -EAGAIN;
    }
}

/*
 * Calls process_wait_impl(), but also copies the output
 * PID to upid (userspace pointer).
 */
static int
process_wait_impl_user(int parent_pid, int *kpid, int *upid)
{
    int ret = process_wait_impl(parent_pid, kpid);
    if (ret < 0) {
        return ret;
    }
    if (!copy_to_user(upid, kpid, sizeof(int))) {
        return -1;
    }
    return ret;
}

/*
 * getargs() syscall handler. Copies the command-line arguments
 * that were used to execute the current process into buf.
 */
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

/*
 * vidmap() syscall handler. Enables the vidmap page and
 * copies its address to screen_start.
 */
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

/*
 * sbrk() syscall handler. Expands or shrinks the current
 * process's heap by the specified number of bytes. If orig_brk
 * is not NULL, the original brk value is written to it.
 * Returns 0 on success, < 0 on failure.
 */
__cdecl int
process_sbrk(int delta, void **orig_brk)
{
    pcb_t *pcb = get_executing_pcb();

    /* Try to copy the address first to avoid having to revert the change */
    void *brk = heap_sbrk(&pcb->heap, 0);
    if (orig_brk != NULL && !copy_to_user(orig_brk, &brk, sizeof(void *))) {
        return -1;
    }

    /* Resize the heap */
    void *ret = heap_sbrk(&pcb->heap, delta);
    if (ret == NULL) {
        return -1;
    }

    return 0;
}

/*
 * fork() syscall handler. Creates a clone of the current
 * process. All state is preserved except for pending signals.
 */
__cdecl int
process_fork(
    intptr_t unused1,
    intptr_t unused2,
    intptr_t unused3,
    intptr_t unused4,
    intptr_t unused5,
    int_regs_t *regs)
{
    /*
     * All code below executes in the parent! The child
     * begins execution in idt_unwind_stack (i.e. skips
     * all normal C stack unwinding).
     */
    pcb_t *child_pcb = process_clone(get_executing_pcb(), regs, true);
    if (child_pcb == NULL) {
        return -1;
    }
    return child_pcb->pid;
}

/*
 * exec() syscall handler. Replaces the calling process
 * by executing the specified command.
 */
__cdecl int
process_exec(
    const char *command,
    intptr_t unused1,
    intptr_t unused2,
    intptr_t unused3,
    intptr_t unused4,
    int_regs_t *regs)
{
    return process_exec_impl(get_executing_pcb(), regs, command);
}

/*
 * wait() syscall handler. Note that the API is a bit different
 * from Linux: pid is an in-out pointer to a PID/PGID. If the
 * wait completes successfully (i.e. is not interrupted by a
 * signal), pid will point to the actual PID of the process that
 * was reaped. The exit code of the process will be returned
 * by this function.
 *
 * On input, if *pid > 0, waits for the process with the specified
 * PID. If *pid < 0, waits for any process in the process group
 * with pgid == -pid. If *pid == 0, waits for any process in the
 * caller's process group.
 */
__cdecl int
process_wait(int *pid)
{
    pcb_t *pcb = get_executing_pcb();

    /* Read the actual pid from userspace */
    int kpid;
    if (!copy_from_user(&kpid, pid, sizeof(int))) {
        return -1;
    }

    /* kpid == 0 means wait on our own group */
    if (kpid == 0) {
        kpid = -pcb->group;
    }

    /* Wait for a process to die and copy its PID */
    return BLOCKING_WAIT(
        process_wait_impl_user(pcb->pid, &kpid, pid),
        wait_queue,
        false);
}

/*
 * getpid() syscall handler. Returns the PID of the calling
 * process.
 */
__cdecl int
process_getpid(void)
{
    pcb_t *pcb = get_executing_pcb();
    return pcb->pid;
}

/*
 * getpgrp() syscall handler. Returns the current process
 * group of the calling process.
 */
__cdecl int
process_getpgrp(void)
{
    pcb_t *pcb = get_executing_pcb();
    return pcb->group;
}

/*
 * setpgrp() syscall handler. Sets the process group
 * of the specified process. If pid == 0, this sets the
 * process group of the calling process. If pgrp == 0,
 * the PID is used as the group ID.
 */
__cdecl int
process_setpgrp(int pid, int pgrp)
{
    if (pid < 0 || pgrp < 0) {
        return -1;
    }

    /* If pid is zero, this refers to the calling process */
    pcb_t *pcb;
    if (pid == 0) {
        pcb = get_executing_pcb();
        pid = pcb->pid;
    } else {
        pcb = get_pcb(pid);
        if (pcb == NULL || pcb->state == PROCESS_STATE_ZOMBIE) {
            debugf("Invalid/nonexistent/dead PID: %d\n", pid);
            return -1;
        }
    }

    /* If pgrp is zero, use the PID as the group ID */
    if (pgrp == 0) {
        pgrp = pid;
    }

    /* No checks here, just #YOLO it. Not POSIX compliant. */
    pcb->group = pgrp;
    return 0;
}

/*
 * execute() syscall handler. This is provided for ABI
 * compatibility with the original fs programs. It is identical
 * to executing fork + exec + wait in userspace (with
 * process groups set accordingly). Note that any signals
 * received during execution are delayed until the child
 * process halts (i.e. -EINTR is impossible).
 */
__cdecl int
process_execute(
    const char *command,
    intptr_t unused1,
    intptr_t unused2,
    intptr_t unused3,
    intptr_t unused4,
    int_regs_t *regs)
{
    /* Start by cloning ourselves */
    pcb_t *parent_pcb = get_executing_pcb();
    pcb_t *child_pcb = process_clone(parent_pcb, regs, false);
    if (child_pcb == NULL) {
        return -1;
    }

    /* Next, perform exec() on behalf of the child process */
    if (process_exec_impl(child_pcb, &child_pcb->regs, command) < 0) {
        process_close(child_pcb);
        process_free_pcb(child_pcb);
        return -1;
    }

    /* Close everything except stdin and stdout if in compat mode */
    if (child_pcb->compat) {
        int fd;
        for (fd = 2; fd < MAX_FILES; ++fd) {
            file_desc_unbind(child_pcb->files, fd);
        }
    }

    /* Next, change the child's group and set it as the foreground */
    child_pcb->group = child_pcb->pid;
    terminal_tcsetpgrp(child_pcb->pid);

    /*
     * Wait for the child process to exit. We can't directly
     * call process_wait() here, since that will abort early
     * on signals, which we don't want.
     */
    int ret;
    do {
        scheduler_sleep(&wait_queue);
        ret = process_wait_impl(parent_pcb->pid, &child_pcb->pid);
    } while (ret == -EAGAIN);

    /* Finally, restore the original foreground group */
    terminal_tcsetpgrp(parent_pcb->pid);

    return ret;
}

/*
 * Process halt implementation. Unlike process_halt(),
 * the status is not truncated to 1 byte.
 */
void
process_halt_impl(int status)
{
    /* This is the PCB of the child (halting) process */
    pcb_t *child_pcb = get_executing_pcb();

    /* Release process resources */
    process_close(child_pcb);

    /*
     * Orphan any processes created by this process,
     * and reap the ones that have already exited.
     *
     * Warning: This is only safe because "freeing"
     * a PCB just sets its PID to an invalid value.
     * If one day we use malloc and free, this will
     * cause a use-after-free when moving to the next PCB.
     */
    pcb_t *other_pcb;
    process_for_each(other_pcb) {
        if (other_pcb->parent_pid == child_pcb->pid) {
            other_pcb->parent_pid = -1;
            if (other_pcb->state == PROCESS_STATE_ZOMBIE) {
                process_free_pcb(other_pcb);
            }
        }
    }

    /*
     * If our parent is dead, auto-reap this process,
     * otherwise notify parent that child is dead.
     */
    if (child_pcb->parent_pid < 0) {
        /* Save terminal across process destruction */
        int terminal = child_pcb->terminal;

        /* Destroy the child process */
        process_free_pcb(child_pcb);

        /*
         * If that was the last process in its terminal, spawn
         * another one in its place. This is to account for
         * the fact that we do not have an init process.
         */
        bool restart = true;
        process_for_each(other_pcb) {
            if (other_pcb->terminal == terminal) {
                restart = false;
                break;
            }
        }

        /*
         * No processes left in this terminal, create a new one to
         * be scheduled once we finish tearing down this stack.
         */
        if (restart) {
            char cmd[] = INIT_PROCESS;
            pcb_t *pcb = process_create_user(cmd, terminal);
            assert(pcb != NULL);
        }
    } else {
        /* Put child into zombie state */
        child_pcb->exit_code = status;
        child_pcb->state = PROCESS_STATE_ZOMBIE;

        /* Wake parent to notify them that child is dead */
        pcb_t *parent_pcb = get_pcb(child_pcb->parent_pid);
        scheduler_wake(parent_pcb);
    }

    /* Switch away from this process for the last time */
    scheduler_exit();
}

/*
 * halt() syscall handler. Releases most process state and
 * places it into a zombie state to be reaped by the parent.
 * Unlike Linux, if the parent dies, the process will be
 * reaped by the kernel. This never returns.
 */
__cdecl void
process_halt(int status)
{
    /*
     * Only the lowest byte is used, rest are reserved
     * This only applies when this is called via syscall;
     * the kernel must still be able to halt a process
     * with a status > 255.
     */
    process_halt_impl(status & 0xff);
}

/*
 * Callback for process_monosleep(). Wakes the corresponding process.
 */
static void
process_monosleep_callback(timer_t *timer)
{
    pcb_t *pcb = timer_entry(timer, pcb_t, sleep_timer);
    scheduler_wake(pcb);
}

/*
 * Sleeps until the specified monotonic clock time (in milliseconds).
 * If target is earlier than the current time, the call will immediately
 * return 0. The sleep may be interrupted, in which case -EINTR will
 * be returned and this can be called again with the same argument.
 * Otherwise, 0 will be returned to indicate a successful sleep.
 */
__cdecl int
process_monosleep(int target)
{
    if (target < 0) {
        return -1;
    }

    /* Check if we're already past the target time */
    int now = pit_monotime();
    if (now >= target) {
        return 0;
    }

    /* Put ourselves to sleep */
    pcb_t *pcb = get_executing_pcb();
    timer_setup_abs(&pcb->sleep_timer, target, process_monosleep_callback);
    scheduler_sleep(&sleep_queue);

    /* We woke up, cancel timer in case we got woken early */
    timer_cancel(&pcb->sleep_timer);

    /* Check if we slept long enough */
    now = pit_monotime();
    if (now >= target) {
        return 0;
    }

    return -EINTR;
}

/* Initializes all process control related data */
void
process_init(void)
{
    assert(sizeof(process_data_t) == PROCESS_DATA_SIZE);

    int i;
    for (i = 0; i < MAX_PROCESSES; ++i) {
        process_info[i].pid = -1;
    }
}

/* She spawns C shells by the seashore */
void
process_start_shell(void)
{
    pcb_t *idle = process_create_idle();
    int i;
    for (i = 0; i < NUM_TERMINALS; ++i) {
        char cmd[] = INIT_PROCESS;
        pcb_t *pcb = process_create_user(cmd, i);
        assert(pcb != NULL);
    }
    process_run(idle);
}
