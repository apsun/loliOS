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
 * is empty, returns false.
 */
bool
serial_can_read(int32_t which)
{
    serial_line_status_t status;
    status.raw = serial_in(which, SERIAL_PORT_LINE_STATUS);
    return status.data_ready;
}

/*
 * Returns whether there is space remaining in the serial
 * UART tx queue. If the tx queue is full, returns
 * false.
 */
bool
serial_can_write(int32_t which)
{
    serial_line_status_t status;
    status.raw = serial_in(which, SERIAL_PORT_LINE_STATUS);
    return status.empty_tx_holding;
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
    uint32_t trigger_level,
    void (*irq_handler)(void))
{
    /* Disable all interrupts */
    serial_int_enable_t ie;
    ie.data_available = 0;
    ie.empty_tx_holding = 0;
    ie.line_status = 0;
    ie.modem_status = 0;
    ie.reserved = 0;
    serial_out(which, SERIAL_PORT_INT_ENABLE, ie.raw);

    /* Put serial into DLAB mode, also set some parameters */
    serial_line_ctrl_t lc;
    lc.char_bits = char_bits;
    lc.stop_bits = stop_bits;
    lc.parity = parity;
    lc.reserved = 0;
    lc.dlab = 1;
    serial_out(which, SERIAL_PORT_LINE_CTRL, lc.raw);

    /* Write baud rate */
    uint32_t baud_divisor = SERIAL_CLOCK_HZ / baud_rate;
    ASSERT(baud_divisor * baud_rate == SERIAL_CLOCK_HZ);
    ASSERT((baud_divisor & ~0xffff) == 0);
    serial_out(which, SERIAL_PORT_BAUD_LO, (baud_divisor >> 0) & 0xff);
    serial_out(which, SERIAL_PORT_BAUD_HI, (baud_divisor >> 8) & 0xff);

    /* Disable DLAB mode */
    lc.dlab = 0;
    serial_out(which, SERIAL_PORT_LINE_CTRL, lc.raw);

    /* Enable FIFO, set trigger level */
    serial_fifo_ctrl_t fc;
    fc.enable_fifo = 1;
    fc.clear_rx = 1;
    fc.clear_tx = 1;
    fc.reserved = 0;
    fc.trigger_level = trigger_level;
    serial_out(which, SERIAL_PORT_FIFO_CTRL, fc.raw);

    /* Apparently aux output 2 needs to be 1 to receive interrupts */
    serial_modem_ctrl_t mc;
    mc.data_terminal_ready = 1;
    mc.request_to_send = 1;
    mc.aux_output_1 = 0;
    mc.aux_output_2 = 1;
    mc.loopback = 0;
    mc.autoflow_control = 0;
    mc.reserved = 0;
    serial_out(which, SERIAL_PORT_MODEM_CTRL, mc.raw);

    /* Re-enable interrupts */
    ie.data_available = 1;
    serial_out(which, SERIAL_PORT_INT_ENABLE, ie.raw);

    /* Register IRQ handler */
    if (which == 1) {
        irq_register_handler(IRQ_COM1, irq_handler);
    } else if (which == 2) {
        irq_register_handler(IRQ_COM2, irq_handler);
    } else {
        ASSERT(0);
    }
}
