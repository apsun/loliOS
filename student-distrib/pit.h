#ifndef _PIT_H
#define _PIT_H

/* data port for channel 0 */
#define PIT_DATA_PORT0  0x40
/* command port */
#define PIT_CMD_PORT    0x43
/* mode selector for a command byte */
#define CHANNEL0        0x00
/* access both high and low byte of reload value */
#define ACCESS_HL       0x30
/* rate generator */
#define OPMODE2         0x04
/*
 * 16 bit Binary representation
 * instead of BCD representation
 */
#define BINARY          0x00

#ifndef _ASM

void pit_init(void);

#endif /* _ASM */

#endif /* _PIT_H */
