#ifndef _TIMER_H
#define _TIMER_H

#include "types.h"
#include "lib.h"
#include "list.h"
#include "time.h"

#ifndef ASM

/*
 * Timer structure - works similarly to the list API.
 * Contains an expiry time and a callback to run upon
 * expiry. The timer itself is passed to the callback.
 */
typedef struct timer {
    list_t list;
    nanotime_t when;
    void (*callback)(struct timer *);
} timer_t;

/*
 * Returns a pointer to the structure containing
 * this timer.
 */
#define timer_entry(ptr, type, member) \
    container_of(ptr, type, member)

/* Updates all active timers and runs callbacks upon expiry */
void timer_tick(nanotime_t now);

/* Initializes a timer object */
void timer_init(timer_t *timer);

/* Clones an existing timer object */
void timer_clone(timer_t *dest, timer_t *src);

/* Starts a new timer with the specified delay and callback */
void timer_setup(timer_t *timer, nanotime_t delay, void (*callback)(timer_t *));

/* Starts a new timer with the specified target time and callback */
void timer_setup_abs(timer_t *timer, nanotime_t when, void (*callback)(timer_t *));

/* Cancels a running timer */
void timer_cancel(timer_t *timer);

#endif /* ASM */

#endif /* _TIMER_H */
