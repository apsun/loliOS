#ifndef _RTC_H
#define _RTC_H

#include "types.h"
#include "syscall.h"
#include "file.h"

/*
 * Highest possible value for RTC virtual interrupt
 * frequency. Also used as the real interrupt frequency
 * for the RTC counter.
 */
#define MAX_RTC_FREQ 1024

#ifndef ASM

/* RTC syscall handlers */
int32_t rtc_open(const char *filename, file_obj_t *file);
int32_t rtc_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t rtc_write(file_obj_t *file, const void *buf, int32_t nbytes);
int32_t rtc_close(file_obj_t *file);
int32_t rtc_ioctl(file_obj_t *file, uint32_t req, uint32_t arg);

/* time() syscall handler */
__cdecl int32_t rtc_time(void);

/* Returns the current value of the RTC counter. */
uint32_t rtc_get_counter(void);

/* Initializes real-time clock interrupts */
void rtc_init(void);

#endif /* ASM */

#endif /* _RTC_H */
