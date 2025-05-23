#include "pit.h"
#include "types.h"
#include "debug.h"
#include "portio.h"
#include "irq.h"
#include "scheduler.h"
#include "timer.h"

/* Internal frequency of the PIT */
#define PIT_FREQ 1193182

/* Use a value such that freq/divisor is ~100Hz */
#define PIT_DIVISOR 11932

/* Number of milliseconds that elapse per interrupt */
#define PIT_MS_PER_IRQ (1000 * PIT_DIVISOR / PIT_FREQ)

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
 * Global counter used for monotonic time.
 */
static volatile uint32_t pit_counter = 0;

/*
 * Sets the interrupt frequency of the PIT. The argument
 * is the number of PIT cycles per interrupt.
 */
static void
pit_set_divisor(int divisor)
{
    assert(divisor >= 1 && divisor <= 65536);

    /* Set PIT operation mode */
    uint8_t cmd = 0;
    cmd |= PIT_CMD_CHANNEL_0;
    cmd |= PIT_CMD_ACCESS_HL;
    cmd |= PIT_CMD_OPMODE_2;
    cmd |= PIT_CMD_BINARY;
    outb(cmd, PIT_PORT_CMD);

    /* Write divisor value. 65536 is naturally masked to 0. */
    outb((divisor >> 0) & 0xff, PIT_PORT_DATA_0);
    outb((divisor >> 8) & 0xff, PIT_PORT_DATA_0);
}

/*
 * PIT IRQ handler. Updates timers and yields the current
 * process's timeslice.
 */
static void
pit_handle_irq(void)
{
    uint32_t now = ++pit_counter;
    timer_tick(PIT_MS_PER_IRQ * now);
    scheduler_yield();
}

/*
 * Returns the current monotonic clock time in milliseconds.
 * The result is only valid when compared with the result
 * of another call to monotime(), or as an input to sleep().
 */
__cdecl int
pit_monotime(void)
{
    return PIT_MS_PER_IRQ * pit_counter;
}

/*
 * Initializes the PIT. Sets the frequency and registers
 * the IRQ handler.
 */
void
pit_init(void)
{
    pit_set_divisor(PIT_DIVISOR);
    irq_register_handler(IRQ_PIT, pit_handle_irq);
}
