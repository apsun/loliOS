#ifndef _SERIAL_H
#define _SERIAL_H

#include "types.h"

#define SERIAL_PORT_COM1 0x3F8
#define SERIAL_PORT_COM2 0x2F8

#define SERIAL_PORT_DATA         0
#define SERIAL_PORT_BAUD_LO      0
#define SERIAL_PORT_INT_ENABLE   1
#define SERIAL_PORT_BAUD_HI      1
#define SERIAL_PORT_INT_ID       2
#define SERIAL_PORT_LINE_CTRL    3
#define SERIAL_PORT_MODEM_CTRL   4
#define SERIAL_PORT_LINE_STATUS  5
#define SERIAL_PORT_MODEM_STATUS 6
#define SERIAL_PORT_SCRATCH      7

#define SERIAL_CLOCK_HZ 115200

#define SERIAL_LC_CHAR_BITS_5  0x00
#define SERIAL_LC_CHAR_BITS_6  0x01
#define SERIAL_LC_CHAR_BITS_7  0x02
#define SERIAL_LC_CHAR_BITS_8  0x03
#define SERIAL_LC_STOP_BITS_1  0x00
#define SERIAL_LC_STOP_BITS_2  0x01
#define SERIAL_LC_PARITY_NONE  0x00
#define SERIAL_LC_PARITY_ODD   0x01
#define SERIAL_LC_PARITY_EVEN  0x03
#define SERIAL_LC_PARITY_MARK  0x05
#define SERIAL_LC_PARITY_SPACE 0x07

#define SERIAL_LS_DATA_READY   0
#define SERIAL_LS_THR_EMPTY    5

#ifndef ASM

/* Serial line control struct */
typedef union {
    struct {
        uint8_t char_bits : 2;
        uint8_t stop_bits : 1;
        uint8_t parity    : 3;
        uint8_t padding   : 1;
        uint8_t dlab      : 1;
    } __attribute__((packed));
    uint8_t raw;
} serial_lc_t;

/* Checks whether there is data available to read */
bool serial_can_read(int32_t which);

/* Checks whether there is space available to write */
bool serial_can_write(int32_t which);

/* Reads a single char from the UART rx queue (blocking) */
uint8_t serial_read(int32_t which);

/* Reads multiple chars from the UART rx queue (non-blocking) */
int32_t serial_read_all(int32_t which, uint8_t *buf, int32_t len);

/* Writes a single char to the UART tx queue (blocking) */
void serial_write(int32_t which, uint8_t data);

/* Writes multiple chars to the UART tx queue (non-blocking) */
int32_t serial_write_all(int32_t which, const uint8_t *buf, int32_t len);

/*
 * Initializes the serial driver for the specified COM
 * port with the specified parameters. This should be
 * called by device drivers.
 */
void
serial_init(
    int32_t which,
    uint32_t baud_rate,
    uint32_t char_bits,
    uint32_t stop_bits,
    uint32_t parity,
    void (*irq_handler)(void));

#endif /* ASM */

#endif /* _SERIAL_H */
