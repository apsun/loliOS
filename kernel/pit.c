#include "pit.h"
#include "lib.h"
#include "irq.h"
#include "scheduler.h"

/* Min and max PIT frequencies in Hz */
#define PIT_FREQ_MAX 1193182
#define PIT_FREQ_MIN 19

/*
 * The frequency we use for context switching.
 * According to the specs it should be 10-50,
 * but 100 looks nicer.
 */
#define PIT_FREQ_SCHEDULER 100

/* PIT IO ports */
#define PIT_PORT_DATA_0 0x40
#define PIT_PORT_DATA_1 0x41
#define PIT_PORT_DATA_2 0x42
#define PIT_PORT_CMD    0x43

/* PIT command bits */
#define PIT_CMD_CHANNEL_0 0x00 /* Select channel 0 */
#define PIT_CMD_ACCESS_HL 0x30 /* Access high and low bytes */
#define PIT_CMD_OPMODE_2  0x04 /* Rate generator mode */
#define PIT_CMD_BINARY    0x00 /* Use binary mode */

/*
 * Sets the interrupt frequency of the PIT.
 */
static void
pit_set_frequency(int freq)
{
    /* Set PIT operation mode */
    uint8_t cmd = 0;
    cmd |= PIT_CMD_CHANNEL_0;
    cmd |= PIT_CMD_ACCESS_HL;
    cmd |= PIT_CMD_OPMODE_2;
    cmd |= PIT_CMD_BINARY;
    outb(cmd, PIT_PORT_CMD);

    /*
     * Convert frequency to reload value.
     * A divisor of 0 actually represents 65536,
     * since the value is truncated to 16 bits.
     */
    uint16_t divisor;
    if (freq < PIT_FREQ_MIN) {
        divisor = 0;
    } else if (freq > PIT_FREQ_MAX) {
        divisor = 1;
    } else {
        divisor = PIT_FREQ_MAX / freq;
    }

    /* Write reload value */
    outb((divisor >> 0) & 0xff, PIT_PORT_DATA_0);
    outb((divisor >> 8) & 0xff, PIT_PORT_DATA_0);
}

/*
 * PIT IRQ handler. Yields the current process's timeslice
 * and performs a context switch.
 */
static void
pit_handle_irq(void)
{
    scheduler_yield();
}

/*
 * Initializes the PIT. Sets the frequency and registers
 * the IRQ handler.
 */
void
pit_init(void)
{
    pit_set_frequency(PIT_FREQ_SCHEDULER);
    irq_register_handler(IRQ_PIT, pit_handle_irq);
}
