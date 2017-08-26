#ifndef _TAUX_H
#define _TAUX_H

#include "types.h"
#include "file.h"
#include "serial.h"

/*
 * Taux controller serial configuration.
 */
#define TAUX_COM_PORT      1
#define TAUX_BAUD_RATE     9600
#define TAUX_CHAR_BITS     SERIAL_LC_CHAR_BITS_8 /* 8-bit data */
#define TAUX_PARITY        SERIAL_LC_PARITY_NONE /* no parity bit */
#define TAUX_STOP_BITS     SERIAL_LC_STOP_BITS_1 /* 1 stop bit */
#define TAUX_TRIGGER_LEVEL SERIAL_FC_TRIGGER_LEVEL_14 /* prevents freezing */

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

/* Taux controller syscalls */
int32_t taux_open(const char *filename, file_obj_t *file);
int32_t taux_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t taux_write(file_obj_t *file, const void *buf, int32_t nbytes);
int32_t taux_close(file_obj_t *file);
int32_t taux_ioctl(file_obj_t *file, uint32_t req, uint32_t arg);

/* Initializes the taux driver */
void taux_init(void);

#endif /* ASM */

#endif /* _TAUX_H */
