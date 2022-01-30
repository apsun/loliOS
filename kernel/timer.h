#ifndef _TIMER_H
#define _TIMER_H

#include "types.h"
#include "list.h"

#ifndef ASM

/*
 * Timer structure - contains an expiry time and a callback to run upon
 * expiry. The private field is passed to the callback as an input.
 */
typedef struct timer {
    list_t list;
    int when;
    void *private;
    void (*callback)(void *);
} timer_t;

/* Updates all active timers and runs callbacks upon expiry */
void timer_tick(int now);

/* Initializes a timer object */
void timer_init(timer_t *timer);

/* Clones an existing timer object */
void timer_clone(timer_t *dest, timer_t *src);

/* Starts a new timer with the specified delay in milliseconds and callback */
void timer_setup(timer_t *timer, int delay, void *private, void (*callback)(void *));

/* Starts a new timer with the specified target monotonic time and callback */
void timer_setup_abs(timer_t *timer, int when, void *private, void (*callback)(void *));

/* Cancels an active timer */
void timer_cancel(timer_t *timer);

/* Checks whether a timer is active */
bool timer_is_active(timer_t *timer);

#endif /* ASM */

#endif /* _TIMER_H */
