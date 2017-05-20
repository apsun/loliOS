#include "pit.h"
#include "lib.h"
#include "irq.h"
#include "process.h"

/*
 * Sets the interrupt frequency of the PIT.
 */
static void
pit_set_frequency(uint32_t freq)
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

/* PIT IRQ handler callback */
static void
pit_handle_irq(void)
{
    process_switch();
}

/* Initializes the PIT and enables interrupts */
void
pit_init(void)
{
    pit_set_frequency(PIT_FREQ_SCHEDULER);
    irq_register_handler(IRQ_PIT, pit_handle_irq);
}
