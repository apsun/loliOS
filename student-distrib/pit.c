#include "pit.h"
#include "lib.h"
#include "irq.h"
#include "sched.h"

#define LOWEST_FREQ  18
#define HIGHEST_FREQ 1193181
#define HIGHEST_RELOAD_VAL 65536
#define SET0_FREQ_CMD (CHANNEL0 | ACCESS_HL | OPMODE2 | BINARY)
/* The frequency for scheduler to perform process switch */
#define SCHED_FREQ 100

/* Handle the interrupt by calling the scheduler */
static void
handle_pit_irq(int_regs_t *regs)
{
	sched_switch(regs);
}

/*
 * set the frequency for the Programmable Interval Timer
 *
 * Note: must initialize PIT before enable IF flag
 */
static void
pit_set_frequency(uint32_t freq)
{
	uint16_t divisor;

	/* ensure if freqency dose not exceed boundry */
	if (freq < LOWEST_FREQ) {
		freq = LOWEST_FREQ;
	} else if (freq > HIGHEST_FREQ)	{
		freq = HIGHEST_FREQ;
	}

	/* if frequency is lowest frequency, set reload value to 0
	 * zero can be used to specify a divisor of 2^16 = 65536
	 */
	divisor = (freq == LOWEST_FREQ) ? 0 : (HIGHEST_FREQ / freq);

	/* select channel 0,
	 * 		  mode 2, 
	 *		  access high and low byte of divisor,
	 *		  binary number representation
	 */
	outb(SET0_FREQ_CMD, PIT_CMD_PORT);

	/* Set low byte of PIT reload value */
	outb(divisor & 0xff, PIT_DATA_PORT0);

	/* Set high byte of PIT reload value */
	outb((divisor >> 8) & 0xff, PIT_DATA_PORT0);
}

/*
 * initialize the Programmable Interval Timer
 *
 * set channel 0 to mode 0,
 *     frequency to about 100Hz (10ms)
 * register interrupt handler
 *
 * Note: must initialize PIT before enable IF flag
 */
void
pit_init(void)
{
	pit_set_frequency(SCHED_FREQ);
	irq_register_handler(IRQ_PIT, handle_pit_irq);
}
