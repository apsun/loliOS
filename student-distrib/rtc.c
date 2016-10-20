#include "rtc.h"
#include "irq.h"
#include "lib.h"
#include "debug.h"
#include "syscall.h"

/* Whether we are waiting for an interrupt to occur */
volatile int32_t waiting_interrupt = 0;

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

    /* Notify any read calls that we got an interrupt */
    waiting_interrupt = 0;

    /* For RTC testing */
    // test_interrupts();
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
 * Sets the interrupt frequency of the RTC.
 * The input must be a power of 2 between 2
 * and 1024.
 *
 * Returns -1 on error, 0 on success.
 */
static int32_t
rtc_set_frequency(int32_t freq)
{
    uint8_t reg_a;

    /* Convert the integer to an enum value */
    uint8_t rs = rtc_freq_to_rs(freq);
    if (rs == RTC_A_RS_NONE) {
        return -1;
    }

    /* Read register A */
    reg_a = read_reg(RTC_REG_A);

    /* Set frequency control bits */
    reg_a &= ~RTC_A_RS;
    reg_a |= rs;

    /* Write register A */
    write_reg(RTC_REG_A, reg_a);
    return 0;
}

/*
 * Open syscall for RTC. Does nothing.
 */
int32_t
rtc_open(const uint8_t *filename)
{
    return 0;
}

/*
 * Read syscall for RTC. Waits for the next periodic interrupt
 * to occur, then returns success.
 *
 * buf and nbytes are ignored.
 */
int32_t
rtc_read(int32_t fd, void *buf, int32_t nbytes)
{
    /* Wait for the interrupt handler to set this flag to 0 */
    waiting_interrupt = 1;
    while (waiting_interrupt);
    return 0;
}

/*
 * Write syscall for RTC. Sets the global periodic interrupt
 * frequency for the RTC. This change will be visible to other
 * programs.
 *
 * buf must point to a int32_t containing the desired frequency,
 * nbytes must equal sizeof(int32_t). The frequency must be a
 * power of 2 between 2 and 1024.
 */
int32_t
rtc_write(int32_t fd, const void *buf, int32_t nbytes)
{
    int32_t freq;

    /* Check if we're writing the appropriate size */
    if (nbytes != sizeof(int32_t)) {
        return -1;
    }

    /* Ensure buffer is valid */
    if (buf == NULL) {
        return -1;
    }

    /* Set frequency */
    freq = *(int32_t *)buf;
    return rtc_set_frequency(freq);
}

/*
 * Close syscall for RTC. Does nothing.
 */
int32_t
rtc_close(int32_t fd)
{
    return 0;
}

/* Initializes the RTC and enables interrupts */
void
rtc_init(void)
{
    /* Read RTC register B */
    uint8_t reg_b = read_reg(RTC_REG_B);

    /* Enable periodic interrupts */
    reg_b |= RTC_B_PIE;

    /* Use binary format */
    reg_b |= RTC_B_DM;

    /* Write RTC register B */
    write_reg(RTC_REG_B, reg_b);

    /* Set the interrupt frequency to 2Hz by default */
    rtc_set_frequency(2);

    /* Register RTC IRQ handler and enable interrupts */
    irq_register_handler(IRQ_RTC, handle_rtc_irq);
}
