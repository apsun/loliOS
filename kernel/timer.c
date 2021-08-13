#include "timer.h"
#include "debug.h"
#include "list.h"
#include "time.h"

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
 * Calls and deactivates any expired timers.
 */
void
timer_tick(nanotime_t now)
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
 * Clones an existing timer. The destination timer must be
 * originally inactive.
 */
void
timer_clone(timer_t *dest, timer_t *src)
{
    dest->callback = src->callback;
    if (dest->callback != NULL) {
        dest->when = src->when;

        /*
         * Since we know that the expiration times are the same,
         * we can just directly add the new timer immediately
         * adjacent to the original one.
         */
        list_add(&dest->list, &src->list);
    }
}

/*
 * Activates a timer to expire after the specified delay.
 * If the timer is already active, the original callback will
 * be cancelled and the timer rescheduled.
 */
void
timer_setup(timer_t *timer, nanotime_t delay, void (*callback)(timer_t *))
{
    timer_setup_abs(timer, monotime_now() + delay, callback);
}

/*
 * Activates a timer to expire at the specified (monotonic) time.
 * if the timer is already active, the original callback will
 * be cancelled and the timer rescheduled.
 */
void
timer_setup_abs(timer_t *timer, nanotime_t when, void (*callback)(timer_t *))
{
    if (timer->callback != NULL) {
        list_del(&timer->list);
    }
    timer->when = when;
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
