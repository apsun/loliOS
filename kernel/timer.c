#include "timer.h"
#include "lib.h"
#include "debug.h"
#include "rtc.h"

/* Global list of timers, in order of time until expiry */
static list_declare(timer_list);

/*
 * Inserts a timer into its correct position in the global
 * timer list.
 */
static void
timer_insert_list(timer_t *timer)
{
    list_t *pos;
    list_for_each_prev(pos, &timer_list) {
        timer_t *qtimer = list_entry(pos, timer_t, list);
        if (timer->when > qtimer->when) {
            break;
        }
    }
    list_add(&timer->list, pos);
}

/*
 * Calls and deactivates any expired timers. This is called
 * every RTC tick.
 */
void
timer_tick(int now)
{
    while (!list_empty(&timer_list)) {
        timer_t *pending = list_first_entry(&timer_list, timer_t, list);
        if (pending->when > now) {
            break;
        }
        list_del(&pending->list);
        void (*callback)(timer_t *) = pending->callback;
        pending->callback = NULL;
        callback(pending);
    }
}

/*
 * Initializes a new timer. Technically this isn't necessary,
 * but since we use the callback to check timer state, we
 * need to ensure that it is initialized to NULL.
 */
void
timer_init(timer_t *timer)
{
    timer->callback = NULL;
}

/*
 * Activates a timer to expire after the specified delay.
 * This cannot be called for an already active timer.
 * Note that the delay is relative to the current time,
 * and is in timer tick units (i.e. for a 3 second timeout,
 * delay should be 3 * TIMER_HZ).
 */
void
timer_setup(timer_t *timer, int delay, void (*callback)(timer_t *))
{
    assert(timer->callback == NULL);
    timer->when = rtc_get_counter() + delay;
    timer->callback = callback;
    timer_insert_list(timer);
}

/*
 * Reschedules a timer to expire after the specified delay.
 * This can only be called for an already active timer.
 */
void
timer_reschedule(timer_t *timer, int delay)
{
    assert(timer->callback != NULL);
    list_del(&timer->list);
    timer->when = rtc_get_counter() + delay;
    timer_insert_list(timer);
}

/*
 * Cancels an active timer. This can only be called for an
 * already active timer.
 */
void
timer_cancel(timer_t *timer)
{
    assert(timer->callback != NULL);
    list_del(&timer->list);
    timer->callback = NULL;
}
