#ifndef _TAUX_H
#define _TAUX_H

/* Accepted ioctl() request values */
#define TAUX_SET_LED     0x10
#define TAUX_READ_LED    0x11
#define TAUX_BUTTONS     0x12
#define TAUX_INIT        0x13
#define TAUX_LED_REQUEST 0x14
#define TAUX_LED_ACK     0x15
#define TAUX_SET_LED_STR 0x16

#ifndef ASM

/* Initializes the taux driver */
void taux_init(void);

#endif /* ASM */

#endif /* _TAUX_H */
