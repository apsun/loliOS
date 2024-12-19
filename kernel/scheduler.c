#include "scheduler.h"
#include "types.h"
#include "debug.h"
#include "list.h"
#include "process.h"
#include "timer.h"

/*
 * Queue of processes waiting to be scheduled.
 * Note that the idle task is not in these queues, and is
 * only scheduled when there are no other processes to run.
 */
static list_define(scheduler_queue);

/*
 * Returns the next process to be scheduled and moves it to the
 * back of the queue.
 */
static pcb_t *
scheduler_next_pcb(void)
{
    /* If no processes to run, schedule the idle task */
    if (list_empty(&scheduler_queue)) {
        return get_idle_pcb();
    }

    /* Pop the first process from the queue and move it to the end */
    pcb_t *pcb = list_first_entry(&scheduler_queue, pcb_t, scheduler_list);
    list_del(&pcb->scheduler_list);
    list_add_tail(&pcb->scheduler_list, &scheduler_queue);

    return pcb;
}

/*
 * Yields the current process's timeslice and schedules
 * the next process to run. Pass null for curr if the process
 * is dying and will never be returned to.
 *
 * The noinline attribute is necessary since we need the registers
 * to be saved at exactly the start of the function and restored
 * at exactly the end.
 */
__noinline static void
scheduler_yield_impl(pcb_t *curr, pcb_t *next)
{
    asm volatile(
        "testl %0, %0;"
        "jz 1f;"
        "movl %%esp, %c2(%0);"
        "movl %%ebp, %c3(%0);"
        "1:"
        "pushl %1;"
        "pushl %0;"
        "call process_switch;"
        "movl %c2(%1), %%esp;"
        "movl %c3(%1), %%ebp;"
        : "+S"(curr),
          "+D"(next)
        : "i"(offsetof(pcb_t, scheduler_esp)),
          "i"(offsetof(pcb_t, scheduler_ebp))
        : "memory", "cc", "eax", "ebx", "ecx", "edx");
}

/*
 * Yields the current process's timeslice and schedules
 * the next process to run.
 */
void
scheduler_yield(void)
{
    pcb_t *curr = get_executing_pcb();
    pcb_t *next = scheduler_next_pcb();
    if (curr == next) {
        return;
    }

    scheduler_yield_impl(curr, next);
}

/*
 * Called when a process is about to die. Unlike scheduler_yield(),
 * this does not bother saving the current process state into the
 * PCB, avoiding a use-after-free. This function does not return.
 */
__noreturn void
scheduler_exit(void)
{
    pcb_t *next = scheduler_next_pcb();
    scheduler_yield_impl(NULL, next);
    panic("Should not return from scheduler_exit()\n");
}

/*
 * Adds a process to the scheduler queue.
 */
void
scheduler_add(pcb_t *pcb)
{
    assert(pcb->pid > 0);
    list_add_tail(&pcb->scheduler_list, &scheduler_queue);
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
 * Intrusive timer container for scheduler_sleep_with_timeout().
 */
typedef struct {
    timer_t timer;
    pcb_t *pcb;
} scheduler_sleep_timer_t;

/*
 * Callback for scheduler_sleep_with_timeout() that wakes the
 * sleeping process.
 */
static void
scheduler_sleep_with_timeout_callback(timer_t *timer)
{
    scheduler_sleep_timer_t *sleep_timer = timer_entry(timer, scheduler_sleep_timer_t, timer);
    scheduler_wake(sleep_timer->pcb);
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

    scheduler_sleep_timer_t sleep_timer;
    timer_init(&sleep_timer.timer);
    sleep_timer.pcb = get_executing_pcb();
    timer_setup_abs(&sleep_timer.timer, timeout, scheduler_sleep_with_timeout_callback);
    scheduler_sleep();
    timer_cancel(&sleep_timer.timer);
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
