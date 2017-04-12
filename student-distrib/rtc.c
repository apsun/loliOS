#include "rtc.h"
#include "irq.h"
#include "lib.h"
#include "debug.h"
#include "file.h"
#include "process.h"

/*
 * Number of RTC interrupts that have occurred.
 * Note: We use a 32-bit value since reads will be atomic,
 * so no locking is required. 48 days of uptime is
 * required to overflow the value, which is Good Enough (TM).
 */
static volatile uint32_t rtc_counter;

/*
 * Reads the value of a RTC register. The reg
 * param must be one of the RTC_REG_* constants.
 */
static uint8_t
read_reg(uint8_t reg)
{
    outb(reg, RTC_PORT_INDEX);
    return inb(RTC_PORT_DATA);
}

/*
 * Writes the value of a RTC register. The reg
 * param must be one of the RTC_REG_* constants.
 */
static void
write_reg(uint8_t reg, uint8_t value)
{
    outb(reg, RTC_PORT_INDEX);
    outb(value, RTC_PORT_DATA);
}

/* RTC IRQ handler callback */
static void
handle_rtc_irq(void)
{
    /* Read from register C, ignore value */
    read_reg(RTC_REG_C);

    /* Increment the global RTC interrupt counter */
    uint32_t new_counter = ++rtc_counter;

    /* Broadcast counter update for alarm signals */
    process_update_clock(new_counter);
}

/*
 * Maps an integer frequency value to one of the
 * RTC_A_RS_* constants. Returns RTC_A_RS_NONE if the
 * frequency is not a power of 2 between 2 and 1024.
 */
static uint8_t
rtc_freq_to_rs(int32_t freq)
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
static int32_t
rtc_set_frequency(int32_t freq)
{
    /* Convert the integer to an enum value */
    uint8_t rs = rtc_freq_to_rs(freq);
    if (rs == RTC_A_RS_NONE) {
        return -1;
    }

    /* Read register A */
    uint8_t reg_a = read_reg(RTC_REG_A);

    /* Set frequency control bits */
    reg_a &= ~RTC_A_RS;
    reg_a |= rs;

    /* Write register A */
    write_reg(RTC_REG_A, reg_a);

    return 0;
}

/*
 * Open syscall for RTC. Frequency is set to 2Hz by default.
 */
int32_t
rtc_open(const uint8_t *filename, file_obj_t *file)
{
    /*
     * File offset field holds the virtual interrupt frequency
     * for this file (because I'm lazy and it recycles an
     * otherwise unused field).
     */
    file->offset = 2;
    return 0;
}

/*
 * Read syscall for RTC. Waits for the next (virtual)
 * periodic interrupt to occur, then returns success.
 * If a signal is delivered during the read, the read
 * will be prematurely aborted and -1 will be returned.
 */
int32_t
rtc_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    /*
     * We need to wait until the RTC counter reaches this value
     */
    uint32_t target_counter = rtc_counter + MAX_RTC_FREQ / file->offset;

    /*
     * We should break out of the wait loop early if we receive
     * a signal that we need to handle.
     */
    bool have_signal = false;

    /* Enable interrupts */
    uint32_t flags;
    sti_and_save(flags);

    /* Wait for enough RTC interrupts or a signal, whichever comes first */
    while (rtc_counter < target_counter) {
        have_signal = process_has_pending_signal();
        if (have_signal) {
            break;
        }
    }

    /* Disable interrupts again */
    restore_flags(flags);

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
 * buf must point to a int32_t containing the desired frequency,
 * nbytes must equal sizeof(int32_t). The frequency must be a
 * power of 2 between 2 and 1024. file is ignored.
 */
int32_t
rtc_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    /* Check if we're reading the appropriate size */
    if (nbytes != sizeof(int32_t)) {
        return -1;
    }

    /* Read the frequency */
    int32_t freq;
    if (!copy_from_user(&freq, buf, sizeof(int32_t))) {
        return -1;
    }

    /* Validate frequency */
    if (rtc_freq_to_rs(freq) == RTC_A_RS_NONE) {
        return -1;
    }

    /* Save desired interrupt frequency in file offset */
    file->offset = freq;

    return 0;
}

/*
 * Close syscall for RTC. Does nothing.
 */
int32_t
rtc_close(file_obj_t *file)
{
    return 0;
}

/*
 * Returns the current value of the RTC counter.
 */
uint32_t
rtc_get_counter(void)
{
    return rtc_counter;
}

/* Initializes the RTC and enables interrupts */
void
rtc_init(void)
{
    /* Read RTC register B */
    uint8_t reg_b = read_reg(RTC_REG_B);

    /* Enable periodic interrupts */
    reg_b |= RTC_B_PIE;

    /* Write RTC register B */
    write_reg(RTC_REG_B, reg_b);

    /*
     * Initialize the interrupt frequency.
     * Since we virtualize the device, this just needs
     * to be at least as large as the largest virtual
     * frequency.
     */
    rtc_set_frequency(MAX_RTC_FREQ);

    /* Register RTC IRQ handler and enable interrupts */
    irq_register_handler(IRQ_RTC, handle_rtc_irq);
}
