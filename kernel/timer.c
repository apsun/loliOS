#include "timer.h"
#include "types.h"
#include "debug.h"
#include "list.h"
#include "pit.h"

/* Global list of timers, in order of time until expiry */
static list_declare(timer_list);

/*
 * Inserts a timer into its correct position in the global
 * timer list.
 */
static void
timer_insert_list(timer_t *timer)
{
    assert(timer != NULL);
    assert(timer->callback != NULL);

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
timer_tick(int now)
{
    assert(now >= 0);

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
    assert(timer != NULL);
    timer->callback = NULL;
}

/*
 * Clones an existing timer. The destination timer must be
 * originally inactive.
 */
void
timer_clone(timer_t *dest, timer_t *src)
{
    assert(src != NULL);
    assert(dest != NULL);
    assert(dest->callback == NULL);

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
 * Activates a timer to expire after the specified delay in
 * milliseconds. If the timer is already active, the original
 * callback will be cancelled and the timer rescheduled.
 */
void
timer_setup(timer_t *timer, int delay, void (*callback)(timer_t *))
{
    assert(timer != NULL);
    assert(delay >= 0);
    assert(callback != NULL);

    timer_setup_abs(timer, pit_monotime() + delay, callback);
}

/*
 * Activates a timer to expire at the specified monotonic time.
 * if the timer is already active, the original callback will
 * be cancelled and the timer rescheduled.
 */
void
timer_setup_abs(timer_t *timer, int when, void (*callback)(timer_t *))
{
    assert(timer != NULL);
    assert(when >= 0);
    assert(callback != NULL);

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
    assert(timer != NULL);

    if (timer->callback != NULL) {
        list_del(&timer->list);
        timer->callback = NULL;
    }
}

/*
 * Returns whether a timer is currently active.
 */
bool
timer_is_active(timer_t *timer)
{
    assert(timer != NULL);
    return timer->callback != NULL;
}
