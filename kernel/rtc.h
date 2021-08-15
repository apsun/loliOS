#ifndef _RTC_H
#define _RTC_H

#include "types.h"

#ifndef ASM

/* Returns the current unix timestamp in seconds */
__cdecl int rtc_realtime(void);

/* Initializes real-time clock interrupts */
void rtc_init(void);

#endif /* ASM */

#endif /* _RTC_H */
