#include "scheduler.h"
#include "lib.h"
#include "debug.h"
#include "process.h"

/*
 * Active and inactive scheduler queues. The reason we
 * have two queues is to simplify tracking of which
 * process needs to be executed next. As we dequeue tasks
 * from the active queue and enqueue them into the inactive
 * queue, we essentially mark them as "already executed".
 * Once every task has had an opportunity to execute, we
 * "clear" the mark by swapping the queues. Note that
 * the invariant is that the idle task is always schedulable,
 * so there is always _something_ to run.
 */
static list_t scheduler_queues[2];
static int scheduler_active;

/*
 * Returns the currently active scheduler queue.
 */
static list_t *
scheduler_active_queue(void)
{
    return &scheduler_queues[scheduler_active];
}

/*
 * Returns the currently inactive scheduler queue.
 */
static list_t *
scheduler_inactive_queue(void)
{
    return &scheduler_queues[!scheduler_active];
}

/*
 * Pops the next process scheduled to be executed
 * off the active queue. If the active queue becomes
 * empty as a result, this will also swap the active
 * and inactive queues.
 */
static pcb_t *
scheduler_next_pcb(void)
{
    /*
     * Swap the queues if we've finished processing
     * everything in the active queue (inactive queue
     * becomes the active queue and vise versa). We
     * can't do this at the end since it's possible
     * to pull processes out of the active queue when
     * putting them to sleep.
     */
    if (list_empty(scheduler_active_queue())) {
        scheduler_active = !scheduler_active;
    }

    /* Pop the first process from the active queue */
    list_t *active_queue = scheduler_active_queue();
    assert(!list_empty(active_queue));
    pcb_t *pcb = list_first_entry(active_queue, pcb_t, scheduler_list);
    list_del(&pcb->scheduler_list);

    /* Move it to the inactive queue */
    list_t *inactive_queue = scheduler_inactive_queue();
    list_add_tail(&pcb->scheduler_list, inactive_queue);

    return pcb;
}


/*
 * Yields the current process's execution and schedules
 * the next process to run. This should never be called
 * directly as it does not obey the normal cdecl ABI;
 * use scheduler_yield() instead.
 */
__cdecl __noinline __used static void
scheduler_yield_impl(pcb_t *curr)
{
    pcb_t *next = scheduler_next_pcb();

    /* This can happen if idle is the only process running */
    if (curr == next) {
        return;
    }

    /*
     * Save current stack pointer so we can switch back to this
     * stack frame.
     */
    if (curr != NULL) {
        asm volatile(
            "movl %%esp, %0;"
            "movl %%ebp, %1;"
            : "=g"(curr->scheduler_esp),
              "=g"(curr->scheduler_ebp));
    }

    if (next->state == PROCESS_STATE_NEW) {
        /*
         * Since the process has not been run yet, its saved
         * scheduler ESP/EBP are invalid. Just execute it directly
         * on top of the current stack; the extra garbage
         * will be ignored the next time the current process
         * is scheduled.
         */
        process_run(next);
    } else if (next->state == PROCESS_STATE_RUNNING) {
        /* Set global execution context */
        process_set_context(next);

        /* Switch to the other process's stack frame */
        asm volatile(
            "movl %0, %%esp;"
            "movl %1, %%ebp;"
            :
            : "r"(next->scheduler_esp),
              "r"(next->scheduler_ebp));
    }
}

/*
 * yield() syscall handler. Yields the current process's
 * execution and schedules the next process to run. Can
 * also be called directly by the kernel.
 */
__cdecl void
scheduler_yield(void)
{
    asm volatile(
        "push %0;"
        "call scheduler_yield_impl;"
        "addl $4, %%esp;"
        :
        : "a"(get_executing_pcb())
        : "ebx", "ecx", "edx", "esi", "edi", "memory");
}

/*
 * Called when a process is about to die. Unlike yield(),
 * this does not bother saving the current process state
 * into the PCB, avoiding a use-after-free. This function
 * does not return.
 */
void
scheduler_exit(void)
{
    scheduler_yield_impl(NULL);
    panic("Should not return from scheduler_exit()");
}

/*
 * Adds a process to the scheduler queue.
 */
void
scheduler_add(pcb_t *pcb)
{
    list_add_tail(&pcb->scheduler_list, scheduler_inactive_queue());
}

/*
 * Removes a process from the scheduler queue.
 */
void
scheduler_remove(pcb_t *pcb)
{
    list_del(&pcb->scheduler_list);
}

/*
 * Removes the currently executing process from the scheduler
 * queue and places it into a sleep queue. The process must
 * be woken by either raising a signal, or by calling one of
 * the scheduler_wake functions. The process must be in the
 * RUNNING state.
 */
void
scheduler_sleep(list_t *queue)
{
    pcb_t *pcb = get_executing_pcb();
    assert(pcb->state == PROCESS_STATE_RUNNING);
    list_del(&pcb->scheduler_list);
    list_add_tail(&pcb->scheduler_list, queue);
    pcb->state = PROCESS_STATE_SLEEPING;
    scheduler_yield();
}

/*
 * Removes the specified process from whatever sleep queue it's
 * currently in and adds it to the scheduler queue again. If the
 * process is not sleeping, this is a no-op.
 */
void
scheduler_wake(pcb_t *pcb)
{
    if (pcb->state == PROCESS_STATE_SLEEPING) {
        list_del(&pcb->scheduler_list);
        list_add_tail(&pcb->scheduler_list, scheduler_inactive_queue());
        pcb->state = PROCESS_STATE_RUNNING;
    }
}

/*
 * Wakes all processes in the specified sleep queue.
 */
void
scheduler_wake_all(list_t *queue)
{
    list_t *pos, *next;
    list_for_each_safe(pos, next, queue) {
        pcb_t *pcb = list_entry(pos, pcb_t, scheduler_list);
        scheduler_wake(pcb);
    }
}

/*
 * Initializes the scheduler.
 */
void
scheduler_init(void)
{
    list_init(&scheduler_queues[0]);
    list_init(&scheduler_queues[1]);
}
