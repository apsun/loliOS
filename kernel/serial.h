#ifndef _SERIAL_H
#define _SERIAL_H

#include "types.h"

#define SERIAL_PORT_COM1 0x3F8
#define SERIAL_PORT_COM2 0x2F8
#define SERIAL_CLOCK_HZ 115200

#define SERIAL_PORT_DATA         0 /* DLAB = 0 */
#define SERIAL_PORT_BAUD_LO      0 /* DLAB = 1 */
#define SERIAL_PORT_INT_ENABLE   1 /* DLAB = 0 */
#define SERIAL_PORT_BAUD_HI      1 /* DLAB = 1 */
#define SERIAL_PORT_INT_ID       2 /* Read */
#define SERIAL_PORT_FIFO_CTRL    2 /* Write */
#define SERIAL_PORT_LINE_CTRL    3
#define SERIAL_PORT_MODEM_CTRL   4
#define SERIAL_PORT_LINE_STATUS  5
#define SERIAL_PORT_MODEM_STATUS 6
#define SERIAL_PORT_SCRATCH      7

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

#define SERIAL_FC_TRIGGER_LEVEL_1  0x00
#define SERIAL_FC_TRIGGER_LEVEL_4  0x01
#define SERIAL_FC_TRIGGER_LEVEL_8  0x02
#define SERIAL_FC_TRIGGER_LEVEL_14 0x03

#ifndef ASM

/* Serial interrupt enable struct */
typedef union {
    struct {
        uint8_t data_available   : 1;
        uint8_t empty_tx_holding : 1;
        uint8_t line_status      : 1;
        uint8_t modem_status     : 1;
        uint8_t reserved         : 4;
    };
    uint8_t raw;
} __packed serial_int_enable_t;

/* Serial FIFO control struct */
typedef union {
    struct {
        uint8_t enable_fifo   : 1;
        uint8_t clear_rx      : 1;
        uint8_t clear_tx      : 1;
        uint8_t dma_mode      : 1;
        uint8_t reserved      : 2;
        uint8_t trigger_level : 2;
    };
    uint8_t raw;
} __packed serial_fifo_ctrl_t;

/* Serial line control struct */
typedef union {
    struct {
        uint8_t char_bits : 2;
        uint8_t stop_bits : 1;
        uint8_t parity    : 3;
        uint8_t reserved  : 1;
        uint8_t dlab      : 1;
    };
    uint8_t raw;
} __packed serial_line_ctrl_t;

/* Serial line status struct */
typedef union {
    struct {
        uint8_t data_ready         : 1;
        uint8_t overrun_error      : 1;
        uint8_t parity_error       : 1;
        uint8_t framing_error      : 1;
        uint8_t break_interrupt    : 1;
        uint8_t empty_tx_holding   : 1;
        uint8_t empty_data_holding : 1;
        uint8_t rx_error           : 1;
    };
    uint8_t raw;
} __packed serial_line_status_t;

/* Serial modem control struct */
typedef union {
    struct {
        uint8_t data_terminal_ready : 1;
        uint8_t request_to_send     : 1;
        uint8_t aux_output_1        : 1;
        uint8_t aux_output_2        : 1;
        uint8_t loopback            : 1;
        uint8_t autoflow_control    : 1;
        uint8_t reserved            : 2;
    };
    uint8_t raw;
} __packed serial_modem_ctrl_t;

/* Reads a single byte from the UART rx queue (blocking) */
uint8_t serial_read_blocking(int which);

/* Reads multiple bytes from the UART rx queue (non-blocking) */
int serial_read_upto(int which, uint8_t *buf, int len);

/* Writes a single byte to the UART tx queue (blocking) */
void serial_write_blocking(int which, uint8_t data);

/* Writes multiple bytes to the UART tx queue (non-blocking) */
int serial_write_upto(int which, const uint8_t *buf, int len);

/* Writes a char buffer to the UART tx queue (blocking) */
void serial_write_chars_blocking(int which, const char *buf, int len);

/* Configures the serial UART and registers an IRQ handler */
void
serial_configure(
    int which,
    int baud_rate,
    int char_bits,
    int stop_bits,
    int parity,
    int trigger_level,
    void (*irq_handler)(void));

#endif /* ASM */

#endif /* _SERIAL_H */
