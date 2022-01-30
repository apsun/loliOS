#include "scheduler.h"
#include "types.h"
#include "debug.h"
#include "list.h"
#include "process.h"
#include "timer.h"

/*
 * Active and inactive scheduler queues. The reason we
 * have two queues is to simplify tracking of which
 * process needs to be executed next. As we dequeue tasks
 * from the active queue and enqueue them into the inactive
 * queue, we essentially mark them as "already executed".
 * Once every task has had an opportunity to execute, we
 * "clear" the mark by swapping the queues.
 *
 * Note that the idle task is not in these queues, and is
 * only scheduled when there are no other processes to run.
 */
static list_t scheduler_queues[2];
static int scheduler_active = 0;

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
        /*
         * If we _really_ have nothing to run, schedule
         * the idle process.
         */
        if (list_empty(scheduler_inactive_queue())) {
            return get_idle_pcb();
        }
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
 * Yields the current process's timeslice and schedules
 * the next process to run. Pass null for curr if the process
 * is dying and will never be returned to.
 */
__attribute__((no_caller_saved_registers))
__noinline static void
scheduler_yield_impl(pcb_t *curr)
{
    pcb_t *next = scheduler_next_pcb();
    if (curr == next) {
        return;
    }

    /*
     * Save current stack pointer so we can switch back to this
     * stack frame, and unset process execution context in
     * preparation for the next process.
     */
    if (curr != NULL) {
        asm volatile(
            "movl %%esp, %0;"
            "movl %%ebp, %1;"
            : "=g"(curr->scheduler_esp),
              "=g"(curr->scheduler_ebp));

        process_unset_context(curr);
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
 * Yields the current process's timeslice and schedules
 * the next process to run.
 */
void
scheduler_yield(void)
{
    scheduler_yield_impl(get_executing_pcb());
}

/*
 * Called when a process is about to die. Unlike scheduler_yield(),
 * this does not bother saving the current process state into the
 * PCB, avoiding a use-after-free. This function does not return.
 */
__noreturn void
scheduler_exit(void)
{
    scheduler_yield_impl(NULL);
    panic("Should not return from scheduler_exit()\n");
}

/*
 * Adds a process to the scheduler queue.
 */
void
scheduler_add(pcb_t *pcb)
{
    assert(pcb->pid > 0);
    list_add_tail(&pcb->scheduler_list, scheduler_inactive_queue());
}

/*
 * Removes a process from the scheduler queue.
 */
void
scheduler_remove(pcb_t *pcb)
{
    assert(pcb->pid > 0);
    list_del(&pcb->scheduler_list);
}

/*
 * Removes the currently executing process from the scheduler
 * queue, puts it into the SLEEPING state, and yields to the next
 * process.
 */
void
scheduler_sleep(void)
{
    pcb_t *pcb = get_executing_pcb();
    assert(pcb->state == PROCESS_STATE_RUNNING);
    pcb->state = PROCESS_STATE_SLEEPING;
    scheduler_remove(pcb);
    scheduler_yield();
}

/*
 * Callback for scheduler_sleep_with_timeout() that wakes the
 * sleeping process.
 */
static void
scheduler_sleep_with_timeout_callback(void *private)
{
    pcb_t *pcb = private;
    scheduler_wake(pcb);
}

/*
 * Same as scheduler_sleep(), but will automatically wake up at the
 * specified timeout (absolute monotonic time) if not woken up by
 * another source.
 */
void
scheduler_sleep_with_timeout(int timeout)
{
    assert(timeout >= 0);

    pcb_t *pcb = get_executing_pcb();
    timer_t timer;
    timer_init(&timer);
    timer_setup_abs(&timer, timeout, pcb, scheduler_sleep_with_timeout_callback);
    scheduler_sleep();
    timer_cancel(&timer);
}

/*
 * Wakes the specified process and adds it to the scheduler queue.
 * No-op if the process is not sleeping.
 */
void
scheduler_wake(pcb_t *pcb)
{
    if (pcb->state == PROCESS_STATE_SLEEPING) {
        scheduler_add(pcb);
        pcb->state = PROCESS_STATE_RUNNING;
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
