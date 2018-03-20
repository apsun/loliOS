#include "rtc.h"
#include "lib.h"
#include "debug.h"
#include "irq.h"
#include "process.h"

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

/*
 * Number of RTC interrupts that have occurred.
 * Note: We use a 32-bit value since reads will be atomic,
 * so no locking is required. 24 days of uptime is
 * required to overflow the value, which is Good Enough (TM).
 */
static volatile int rtc_counter;

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
    int new_counter = ++rtc_counter;

    /* Broadcast counter update for alarm signals */
    process_update_clock(new_counter);
}

/*
 * Maps an integer frequency value to one of the
 * RTC_A_RS_* constants. Returns RTC_A_RS_NONE if the
 * frequency is not a power of 2 between 2 and 1024.
 */
static uint8_t
rtc_freq_to_rs(int freq)
{
    switch (freq) {
    case 8192:
    case 4096:
    case 2048:
        return RTC_A_RS_NONE;
    case 1024:
        return RTC_A_RS_1024;
    case 512:
        return RTC_A_RS_512;
    case 256:
        return RTC_A_RS_256;
    case 128:
        return RTC_A_RS_128;
    case 64:
        return RTC_A_RS_64;
    case 32:
        return RTC_A_RS_32;
    case 16:
        return RTC_A_RS_16;
    case 8:
        return RTC_A_RS_8;
    case 4:
        return RTC_A_RS_4;
    case 2:
        return RTC_A_RS_2;
    default:
        return RTC_A_RS_NONE;
    }
}

/*
 * Sets the real interrupt frequency of the RTC.
 * The input must be a power of 2 between 2
 * and 1024.
 *
 * Returns -1 on error, 0 on success.
 */
static int
rtc_set_frequency(int freq)
{
    /* Convert the integer to an enum value */
    uint8_t rs = rtc_freq_to_rs(freq);
    if (rs == RTC_A_RS_NONE) {
        return -1;
    }

    /* Read register A */
    uint8_t reg_a = rtc_read_reg(RTC_REG_A);

    /* Set frequency control bits */
    reg_a &= ~RTC_A_RS;
    reg_a |= rs;

    /* Write register A */
    rtc_write_reg(RTC_REG_A, reg_a);

    return 0;
}

/*
 * Open syscall for RTC. Frequency is set to 2Hz by default.
 */
int
rtc_open(const char *filename, file_obj_t *file)
{
    /*
     * File private field holds the virtual interrupt frequency
     * for this file.
     */
    file->private = 2;
    return 0;
}

/*
 * Read syscall for RTC. Waits for the next (virtual)
 * periodic interrupt to occur, then returns success.
 * If a signal is delivered during the read, the read
 * will be prematurely aborted and -1 will be returned.
 */
int
rtc_read(file_obj_t *file, void *buf, int nbytes)
{
    /* Max number of ticks we need to wait */
    int max_ticks = MAX_RTC_FREQ / file->private;

    /* Wait until we reach the next multiple of max ticks */
    int target_counter = (rtc_counter + max_ticks) & -max_ticks;

    /*
     * We should break out of the wait loop early if we receive
     * a signal that we need to handle.
     */
    bool have_signal = false;

    /* Wait for enough RTC interrupts or a signal, whichever comes first */
    while (true) {
        /* Check if we've received enough RTC interrupts */
        if (rtc_counter >= target_counter) {
            break;
        }

        /* Exit early if we have a pending signal */
        have_signal = signal_has_pending();
        if (have_signal) {
            break;
        }

        /* Sleep and wait for a new interrupt */
        sti();
        hlt();
        cli();
    }

    /* Return -1 if we aborted because of a signal */
    if (have_signal) {
        return -1;
    } else {
        return 0;
    }
}

/*
 * Write syscall for RTC. Sets the virtual periodic interrupt
 * frequency for this RTC file. Changes will only be visible when
 * calling read() on this file.
 *
 * buf must point to a int containing the desired frequency,
 * nbytes must equal sizeof(int). The frequency must be a
 * power of 2 between 2 and 1024. file is ignored.
 */
int
rtc_write(file_obj_t *file, const void *buf, int nbytes)
{
    /* Check if we're reading the appropriate size */
    if (nbytes != sizeof(int)) {
        return -1;
    }

    /* Read the frequency */
    int freq;
    if (!copy_from_user(&freq, buf, sizeof(int))) {
        return -1;
    }

    /* Validate frequency */
    if (rtc_freq_to_rs(freq) == RTC_A_RS_NONE) {
        return -1;
    }

    /* Save desired interrupt frequency in file */
    file->private = freq;

    return 0;
}

/*
 * Close syscall for RTC. Does nothing.
 */
int
rtc_close(file_obj_t *file)
{
    return 0;
}

/*
 * Ioctl syscall for RTC. Always fails.
 */
int
rtc_ioctl(file_obj_t *file, int req, int arg)
{
    return -1;
}

/*
 * Converts a separate-component time to a Unix timestamp.
 * Hopefully nobody is using our OS in 2038 ;-)
 */
static int
rtc_mktime(
    int year, int month, int day,
    int hour, int min, int sec)
{
    /* Below algorithm shamelessly stolen from Linux mktime() */
    month -= 2;
    if (month <= 0) {
        month += 12;
        year -= 1;
    }

    int leap_days = year / 4 - year / 100 + year / 400;
    int days = leap_days + 367 * month / 12 + day + year * 365 - 719499;
    int hours = days * 24 + hour;
    int mins = hours * 60 + min;
    int secs = mins * 60 + sec;
    return secs;
}

/*
 * Returns the number of seconds since the Unix Epoch
 * (1970-01-01 00:00:00 UTC). This value will overflow
 * in 2038.
 */
__cdecl int
rtc_time(void)
{
    /* Wait until update finishes */
    while (rtc_read_reg(RTC_REG_A) & RTC_A_UIP);

    /* Read all time components */
    int sec = rtc_read_reg(RTC_SECOND);
    int min = rtc_read_reg(RTC_MINUTE);
    int hour = rtc_read_reg(RTC_HOUR);
    int day = rtc_read_reg(RTC_DAY);
    int month = rtc_read_reg(RTC_MONTH);
    int year = rtc_read_reg(RTC_YEAR);
    int century = rtc_read_reg(RTC_CENTURY);
    year += 100 * century;

    /* Convert to Unix timestamp */
    return rtc_mktime(year, month, day, hour, min, sec);
}

/*
 * Returns the current value of the RTC counter.
 */
int
rtc_get_counter(void)
{
    return rtc_counter;
}

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
     * Initialize the interrupt frequency.
     * Since we virtualize the device, this just needs
     * to be at least as large as the largest virtual
     * frequency.
     */
    rtc_set_frequency(MAX_RTC_FREQ);

    /* Register RTC IRQ handler and enable interrupts */
    irq_register_handler(IRQ_RTC, rtc_handle_irq);
}
