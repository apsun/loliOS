#ifndef _PIT_H
#define _PIT_H

/* Min and max PIT frequencies in Hz */
#define PIT_FREQ_MAX 1193182
#define PIT_FREQ_MIN 19

/*
 * The frequency we use for context switching
 * According to the specs it should be 10-50,
 * but 100 looks nicer
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

#ifndef ASM

void pit_init(void);

#endif /* ASM */

#endif /* _PIT_H */
