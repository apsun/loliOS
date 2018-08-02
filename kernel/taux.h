#ifndef _TAUX_H
#define _TAUX_H

#include "types.h"

/*
 * Accepted ioctl() request values.
 */
#define TUX_SET_LED     0x10
#define TUX_READ_LED    0x11
#define TUX_BUTTONS     0x12
#define TUX_INIT        0x13
#define TUX_LED_REQUEST 0x14
#define TUX_LED_ACK     0x15
#define TUX_SET_LED_STR 0x16

/*
 * Commands have the top 2 bits set to 11.
 */
#define MTCP_CMD(c)    (0xC0 | (c))
#define MTCP_OFF       MTCP_CMD(0x0)
#define MTCP_RESET_DEV MTCP_CMD(0x1)
#define MTCP_POLL      MTCP_CMD(0x2)
#define MTCP_BIOC_ON   MTCP_CMD(0x3)
#define MTCP_BIOC_OFF  MTCP_CMD(0x4)
#define MTCP_DBG_OFF   MTCP_CMD(0x5)
#define MTCP_LED_SET   MTCP_CMD(0x6)
#define MTCP_LED_CLK   MTCP_CMD(0x7)
#define MTCP_LED_USR   MTCP_CMD(0x8)
#define MTCP_CLK_RESET MTCP_CMD(0x9)
#define MTCP_CLK_SET   MTCP_CMD(0xa)
#define MTCP_CLK_POLL  MTCP_CMD(0xb)
#define MTCP_CLK_RUN   MTCP_CMD(0xc)
#define MTCP_CLK_STOP  MTCP_CMD(0xd)
#define MTCP_CLK_UP    MTCP_CMD(0xe)
#define MTCP_CLK_DOWN  MTCP_CMD(0xf)
#define MTCP_CLK_MAX   MTCP_CMD(0x10)
#define MTCP_MOUSE_OFF MTCP_CMD(0x11)
#define MTCP_MOUSE_ON  MTCP_CMD(0x12)
#define MTCP_POLL_LEDS MTCP_CMD(0x13)

/*
 * Responses have the top 2 bits set to 01.
 * The MTCP_RESP macro converts the parameter
 * from 000ABCDE format to 01AB0CDE format
 * (6th bit = 1, 3rd bit = 0).
 */
#define MTCP_RESP(n)      (((n) & 7) | (((n) & 0x18) << 1) | 0x40)
#define MTCP_ACK          MTCP_RESP(0x0)
#define MTCP_BIOC_EVENT   MTCP_RESP(0x1)
#define MTCP_CLK_EVENT    MTCP_RESP(0x2)
#define MTCP_OFF_EVENT    MTCP_RESP(0x3)
#define MTCP_POLL_OK      MTCP_RESP(0x4)
#define MCTP_CLK_POLL     MTCP_RESP(0x5)
#define MTCP_RESET        MTCP_RESP(0x6)
#define MTCP_LEDS_POLL0   MTCP_RESP(0x8)
#define MTCP_LEDS_POLL01  MTCP_RESP(0x9)
#define MTCP_LEDS_POLL02  MTCP_RESP(0xa)
#define MTCP_LEDS_POLL012 MTCP_RESP(0xb)
#define MTCP_LEDS_POLL1   MTCP_RESP(0xc)
#define MTCP_LEDS_POLL11  MTCP_RESP(0xd)
#define MTCP_LEDS_POLL12  MTCP_RESP(0xe)
#define MTCP_LEDS_POLL112 MTCP_RESP(0xf)
#define MTCP_ERROR        MTCP_RESP(0x1F)

#ifndef ASM

/* Initializes the taux driver */
void taux_init(void);

#endif /* ASM */

#endif /* _TAUX_H */
