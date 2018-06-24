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
 * Returns the current timer counter.
 */
int
timer_now(void)
{
    return rtc_get_counter();
}

/*
 * Initializes a new timer. This is necessary since we use
 * the callback to determine whether the timer is currently
 * active or not.
 */
void
timer_init(timer_t *timer)
{
    timer->callback = NULL;
}

/*
 * Returns whether the timer is currently active.
 */
bool
timer_is_active(timer_t *timer)
{
    return timer->callback != NULL;
}

/*
 * Activates a timer to expire after the specified delay.
 * Note that the delay is relative to the current time,
 * and is in timer tick units (i.e. for a 3 second timeout,
 * delay should be 3 * TIMER_HZ). If the timer is already
 * active, the original callback will be cancelled and the
 * timer rescheduled.
 */
void
timer_setup(timer_t *timer, int delay, void (*callback)(timer_t *))
{
    if (timer->callback != NULL) {
        list_del(&timer->list);
    }
    timer->when = timer_now() + delay;
    timer->callback = callback;
    timer_insert_list(timer);
}

/*
 * Cancels an active timer. This has no effect if the timer
 * is not active.
 */
void
timer_cancel(timer_t *timer)
{
    if (timer->callback != NULL) {
        list_del(&timer->list);
        timer->callback = NULL;
    }
}
