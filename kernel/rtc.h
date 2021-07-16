#ifndef _RTC_H
#define _RTC_H

#include "time.h"

#ifndef ASM

/* Returns the current unix timestamp in seconds */
time_t rtc_now(void);

/* Initializes real-time clock interrupts */
void rtc_init(void);

#endif /* ASM */

#endif /* _RTC_H */
