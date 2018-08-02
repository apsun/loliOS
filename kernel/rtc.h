#ifndef _RTC_H
#define _RTC_H

#include "types.h"

/*
 * Highest possible value for RTC virtual interrupt
 * frequency. Also used as the real interrupt frequency
 * for the RTC counter.
 */
#define RTC_HZ 1024

#ifndef ASM

/* time() syscall handler */
__cdecl int rtc_time(void);

/* Returns the current value of the RTC counter. */
int rtc_get_counter(void);

/* Initializes real-time clock interrupts */
void rtc_init(void);

#endif /* ASM */

#endif /* _RTC_H */
