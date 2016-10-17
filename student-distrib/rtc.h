#ifndef _RTC_H
#define _RTC_H

#include "types.h"
#include "lib.h"
#include "i8259.h"
extern void rtc_init();
extern void rtc_interrupt_handler();

#endif
