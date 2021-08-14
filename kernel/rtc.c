#include "rtc.h"
#include "debug.h"
#include "portio.h"
#include "list.h"
#include "irq.h"
#include "file.h"
#include "paging.h"
#include "scheduler.h"
#include "signal.h"
#include "syscall.h"

/* RTC IO ports */
#define RTC_PORT_INDEX 0x70
#define RTC_PORT_DATA  0x71

/* RTC registers */
#define RTC_SECOND  0
#define RTC_MINUTE  2
#define RTC_HOUR    4
#define RTC_DAY     7
#define RTC_MONTH   8
#define RTC_YEAR    9
#define RTC_CENTURY 50
#define RTC_REG_A   10
#define RTC_REG_B   11
#define RTC_REG_C   12

/* RTC A register bits */
#define RTC_A_RS   0x0f /* Rate selector */
#define RTC_A_DV   0x70 /* Oscillator */
#define RTC_A_UIP  0x80 /* Update in progress */

/* RTC B register bits */
#define RTC_B_DSE  (1 << 0) /* Daylight saving enable */
#define RTC_B_24H  (1 << 1) /* 24/12 hour byte format */
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

/* Global RTC frequency, both as a register value and in Hz */
#define RTC_A_RS_GLOBAL RTC_A_RS_1024
#define RTC_HZ (32768 >> (RTC_A_RS_GLOBAL - 1))

/* Compact struct to hold fields read from the RTC. */
typedef struct {
    uint8_t century;
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_tm_t;

/*
 * Number of RTC interrupts that have occurred. Used to
 * implement virtual RTC reads. May wrap around to zero.
 */
static volatile uint32_t rtc_counter = 0;

/*
 * Scheduler queue for processes waiting for an RTC interrupt.
 */
static list_declare(rtc_sleep_queue);

/*
 * Reads the value of a RTC register. The reg
 * param must be one of the RTC_REG_* constants.
 */
static uint8_t
rtc_read_reg(uint8_t reg)
{
    outb(reg, RTC_PORT_INDEX);
    return inb(RTC_PORT_DATA);
}

/*
 * Writes the value of a RTC register. The reg
 * param must be one of the RTC_REG_* constants.
 */
static void
rtc_write_reg(uint8_t reg, uint8_t value)
{
    outb(reg, RTC_PORT_INDEX);
    outb(value, RTC_PORT_DATA);
}

/* RTC IRQ handler callback */
static void
rtc_handle_irq(void)
{
    /* Read from register C, ignore value */
    rtc_read_reg(RTC_REG_C);

    /* Increment the global RTC interrupt counter */
    rtc_counter++;

    /* Wake all processes waiting for an interrupt */
    scheduler_wake_all(&rtc_sleep_queue);
}

/*
 * Sets the real interrupt frequency of the RTC.
 * The input must be one of the RTC_A_RS_* constants.
 *
 * Returns -1 on error, 0 on success.
 */
static int
rtc_set_frequency(uint8_t rs)
{
    uint8_t reg_a = rtc_read_reg(RTC_REG_A);
    reg_a &= ~RTC_A_RS;
    reg_a |= rs;
    rtc_write_reg(RTC_REG_A, reg_a);
    return 0;
}

/*
 * open() syscall handler for RTC. Frequency is set to 2Hz by default.
 */
static int
rtc_open(file_obj_t *file)
{
    /*
     * File private field holds the virtual interrupt frequency
     * for this file.
     */
    file->private = 2;
    return 0;
}

/*
 * read() syscall handler for RTC. Waits for the next (virtual)
 * periodic interrupt to occur, then returns success.
 * If a signal is delivered during the read, the read
 * will be prematurely aborted and -EINTR will be returned.
 */
static int
rtc_read(file_obj_t *file, void *buf, int nbytes)
{
    /* Max number of ticks we need to wait */
    uint32_t max_ticks = RTC_HZ / (int)file->private;

    /* Wait until we reach the next multiple of max ticks */
    uint32_t target_counter = (rtc_counter + max_ticks) & -max_ticks;
    return BLOCKING_WAIT(
        (int32_t)(rtc_counter - target_counter) >= 0 ? 0 : -EAGAIN,
        rtc_sleep_queue,
        file->nonblocking);
}

/*
 * write() syscall handler for RTC. Sets the virtual periodic interrupt
 * frequency for this RTC file. Changes will only be visible when
 * calling read() on this file.
 *
 * buf must point to a int containing the desired frequency,
 * nbytes must equal sizeof(int). The frequency must be a
 * power of 2 between 2 and 1024.
 */
static int
rtc_write(file_obj_t *file, const void *buf, int nbytes)
{
    if (nbytes != sizeof(int)) {
        return -1;
    }

    int freq;
    if (!copy_from_user(&freq, buf, sizeof(int))) {
        return -1;
    }

    if (freq < 2 || freq > 1024 || ((freq & (freq - 1)) != 0)) {
        return -1;
    }

    file->private = freq;
    return nbytes;
}

/*
 * Converts a separate-component time to a Unix timestamp.
 */
static time_t
rtc_mktime(rtc_tm_t t)
{
    /* Below algorithm shamelessly stolen from Linux mktime() */
    int year = (int)t.year + (int)t.century * 100;
    int month = (int)t.month - 2;
    if (month <= 0) {
        month += 12;
        year -= 1;
    }

    time_t leap_days = year / 4 - year / 100 + year / 400;
    time_t day_in_year = 367 * month / 12 + t.day;
    time_t days = leap_days + day_in_year + year * 365 - 719499;
    time_t hours = days * 24 + t.hour;
    time_t mins = hours * 60 + t.minute;
    time_t secs = mins * 60 + t.second;
    return secs;
}

/*
 * Returns the number of seconds since the Unix epoch (UTC).
 */
time_t
rtc_now(void)
{
    /* Wait until update finishes */
    while (rtc_read_reg(RTC_REG_A) & RTC_A_UIP);

    /* Read all time components */
    rtc_tm_t t;
    t.century = rtc_read_reg(RTC_CENTURY);
    t.year = rtc_read_reg(RTC_YEAR);
    t.month = rtc_read_reg(RTC_MONTH);
    t.day = rtc_read_reg(RTC_DAY);
    t.hour = rtc_read_reg(RTC_HOUR);
    t.minute = rtc_read_reg(RTC_MINUTE);
    t.second = rtc_read_reg(RTC_SECOND);

    /* Convert to Unix timestamp */
    return rtc_mktime(t);
}

/* RTC file ops */
static const file_ops_t rtc_fops = {
    .open = rtc_open,
    .read = rtc_read,
    .write = rtc_write,
};

/* Initializes the RTC and enables interrupts */
void
rtc_init(void)
{
    /* Wait until update finishes */
    while (rtc_read_reg(RTC_REG_A) & RTC_A_UIP);

    /* Read RTC register B */
    uint8_t reg_b = rtc_read_reg(RTC_REG_B);

    /* Enable periodic interrupts */
    reg_b |= RTC_B_PIE;

    /* Read time in binary, 24 hour format */
    reg_b |= RTC_B_DM;
    reg_b |= RTC_B_24H;

    /* Write RTC register B */
    rtc_write_reg(RTC_REG_B, reg_b);

    /*
     * Set global RTC frequency (needs to be at least as large
     * as the largest virtual frequency we support)
     */
    rtc_set_frequency(RTC_A_RS_GLOBAL);

    /* Register RTC IRQ handler and enable interrupts */
    irq_register_handler(IRQ_RTC, rtc_handle_irq);

    /* Register file ops table */
    file_register_type(FILE_TYPE_RTC, &rtc_fops);
}
