#ifndef _RTC_H
#define _RTC_H

#include "types.h"

/* RTC memory-mapped ports */
#define RTC_PORT_INDEX 0x70
#define RTC_PORT_DATA 0x71

/* RTC register addresses */
#define RTC_REG_A 0x8A
#define RTC_REG_B 0x8B
#define RTC_REG_C 0x8C

/* RTC A register bits */
#define RTC_A_RS   0x0f /* Rate selector */
#define RTC_A_DV   0x70 /* Oscillator */
#define RTC_A_UIP  0x80 /* Update in progress */

/* RTC B register bits */
#define RTC_B_DSE  (1 << 0) /* Daylight saving enable */
#define RTC_B_2412 (1 << 1) /* 24/12 hour byte format */
#define RTC_B_DM   (1 << 2) /* Binary or BCD format */
#define RTC_B_SQWE (1 << 3) /* Square wave enable */
#define RTC_B_UIE  (1 << 4) /* Interrupt on update */
#define RTC_B_AIE  (1 << 5) /* Interrupt on alarm */
#define RTC_B_PIE  (1 << 6) /* Interrupt periodically */
#define RTC_B_SET  (1 << 7) /* Disable updates */

/* RTC periodic interrupt rates */
#define RTC_A_RS_NONE 0x0
#define RTC_A_RS_8192 0x3
#define RTC_A_RS_4096 0x4
#define RTC_A_RS_2048 0x5
#define RTC_A_RS_1024 0x6
#define RTC_A_RS_512  0x7
#define RTC_A_RS_256  0x8
#define RTC_A_RS_128  0x9
#define RTC_A_RS_64   0xA
#define RTC_A_RS_32   0xB
#define RTC_A_RS_16   0xC
#define RTC_A_RS_8    0xD
#define RTC_A_RS_4    0xE
#define RTC_A_RS_2    0xF

#ifndef ASM

/* RTC syscall handlers */
int32_t rtc_open(const uint8_t *filename);
int32_t rtc_read(int32_t fd, void *buf, int32_t nbytes);
int32_t rtc_write(int32_t fd, const void *buf, int32_t nbytes);
int32_t rtc_close(int32_t fd);

/* Initializes real-time clock interrupts */
void rtc_init(void);

#endif /* ASM */

#endif /* _RTC_H */
