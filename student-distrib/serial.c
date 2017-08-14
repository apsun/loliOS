#include "serial.h"
#include "lib.h"
#include "debug.h"
#include "irq.h"

/*
 * Converts a COM# to the corresponding IO port address.
 * Currently only COM1 and COM2 are supported.
 */
static uint32_t
serial_which_to_port_base(int32_t which)
{
    switch (which) {
    case 1:
        return SERIAL_PORT_COM1;
    case 2:
        return SERIAL_PORT_COM2;
    default:
        ASSERT(0);
        return 0;
    }
}

/*
 * Reads a byte from the specified serial port. The
 * which parameter refers to the COM port number,
 * i.e. which=1 means COM1. The port_offset parameter
 * must be one of the SERIAL_PORT_* constants.
 */
static uint8_t
serial_in(int32_t which, uint32_t port_offset)
{
    uint32_t port_base = serial_which_to_port_base(which);
    return inb(port_base + port_offset);
}

/*
 * Writes a byte to the specified serial port. The
 * arguments have the same meaning as serial_in.
 */
static void
serial_out(int32_t which, uint32_t port_offset, uint8_t data)
{
    uint32_t port_base = serial_which_to_port_base(which);
    outb(data, port_base + port_offset);
}

/*
 * Returns whether there is data in the serial UART rx
 * queue available to be read immediately. If the rx queue
 * is empty, return false.
 */
bool
serial_can_read(int32_t which)
{
    uint8_t ls = serial_in(which, SERIAL_PORT_LINE_STATUS);
    return (ls & SERIAL_LS_DATA_READY) != 0;
}

/*
 * Returns whether there is space remaining in the serial
 * UART tx queue. If the tx queue is full, returns
 * false.
 */
bool
serial_can_write(int32_t which)
{
    uint8_t ls = serial_in(which, SERIAL_PORT_LINE_STATUS);
    return (ls & SERIAL_LS_THR_EMPTY) != 0;
}

/*
 * Reads a char from the serial UART rx queue. Blocks
 * until a char has been read.
 */
uint8_t
serial_read(int32_t which)
{
    while (!serial_can_read(which));
    return serial_in(which, SERIAL_PORT_DATA);
}

/*
 * Reads as much data as is available from the serial
 * UART rx queue, up to len chars. Returns the actual
 * number of chars read.
 */
int32_t
serial_read_all(int32_t which, uint8_t *buf, int32_t len)
{
    int32_t i = 0;
    while (i < len && serial_can_read(which)) {
        buf[i++] = serial_in(which, SERIAL_PORT_DATA);
    }
    return i;
}

/*
 * Writes a char to the serial UART tx queue. Blocks
 * until the char has been written.
 */
void
serial_write(int32_t which, uint8_t data)
{
    while (!serial_can_write(which));
    serial_out(which, SERIAL_PORT_DATA, data);
}

/*
 * Writes as much data as will fit to the serial UART
 * tx queue, up to len chars. Returns the actual number
 * of chars written.
 */
int32_t
serial_write_all(int32_t which, const uint8_t *buf, int32_t len)
{
    int32_t i = 0;
    while (i < len && serial_can_write(which)) {
        serial_out(which, SERIAL_PORT_DATA, buf[i++]);
    }
    return i;
}

/*
 * Initializes the serial driver for the specified COM
 * port with the specified parameters. This should be
 * called by the device drivers.
 */
void
serial_init(
    int32_t which,
    uint32_t baud_rate,
    uint32_t char_bits,
    uint32_t stop_bits,
    uint32_t parity,
    void (*irq_handler)(void))
{
    /* Reset interrupt state */
    serial_out(which, SERIAL_PORT_INT_ENABLE, 0);

    /* Get original line control state */
    serial_lc_t lc;
    lc.raw = serial_in(which, SERIAL_PORT_LINE_CTRL);

    /* Put serial into DLAB mode to set baud rate */
    lc.dlab = 1;
    serial_out(which, SERIAL_PORT_LINE_CTRL, lc.raw);

    /* Write baud rate to registers */
    uint32_t baud_divisor = SERIAL_CLOCK_HZ / baud_rate;
    ASSERT(baud_divisor * baud_rate == SERIAL_CLOCK_HZ);
    ASSERT((baud_divisor & ~0xffff) == 0);
    serial_out(which, SERIAL_PORT_BAUD_LO, (baud_divisor >> 0) & 0xff);
    serial_out(which, SERIAL_PORT_BAUD_HI, (baud_divisor >> 8) & 0xff);

    /* Disable DLAB mode and set other parameters */
    lc.dlab = 0;
    lc.char_bits = char_bits;
    lc.stop_bits = stop_bits;
    lc.parity = parity;
    serial_out(which, SERIAL_PORT_LINE_CTRL, lc.raw);

    /* Register IRQ handler */
    if (which == 1) {
        irq_register_handler(IRQ_COM1, irq_handler);
    } else if (which == 2) {
        irq_register_handler(IRQ_COM2, irq_handler);
    } else {
        ASSERT(0);
    }
}
