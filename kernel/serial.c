#include "serial.h"
#include "types.h"
#include "debug.h"
#include "portio.h"
#include "irq.h"

/*
 * Converts a COM# to the corresponding IO port address.
 * Currently only COM1 and COM2 are supported.
 */
static uint16_t
serial_which_to_port_base(int which)
{
    switch (which) {
    case 1:
        return SERIAL_PORT_COM1;
    case 2:
        return SERIAL_PORT_COM2;
    default:
        panic("Unknown serial COM#\n");
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
serial_in(int which, int port_offset)
{
    uint16_t port_base = serial_which_to_port_base(which);
    return inb(port_base + port_offset);
}

/*
 * Writes a byte to the specified serial port. The
 * arguments have the same meaning as serial_in.
 */
static void
serial_out(int which, int port_offset, uint8_t data)
{
    uint16_t port_base = serial_which_to_port_base(which);
    outb(data, port_base + port_offset);
}

/*
 * Returns whether there is data in the serial UART rx
 * queue available to be read immediately. If the rx queue
 * is empty, returns false.
 */
static bool
serial_can_read(int which)
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
static bool
serial_can_write(int which)
{
    serial_line_status_t status;
    status.raw = serial_in(which, SERIAL_PORT_LINE_STATUS);
    return status.empty_tx_holding;
}

/*
 * Reads a byte from the serial UART rx queue. Blocks
 * until a byte has been read.
 */
uint8_t
serial_read_blocking(int which)
{
    while (!serial_can_read(which));
    return serial_in(which, SERIAL_PORT_DATA);
}

/*
 * Reads as much data as is available from the serial
 * UART rx queue, up to len bytes. Returns the actual
 * number of bytes read.
 */
int
serial_read_upto(int which, uint8_t *buf, int len)
{
    int i = 0;
    while (i < len && serial_can_read(which)) {
        buf[i++] = serial_in(which, SERIAL_PORT_DATA);
    }
    return i;
}

/*
 * Writes a byte to the serial UART tx queue. Blocks
 * until the byte has been written.
 */
void
serial_write_blocking(int which, uint8_t data)
{
    while (!serial_can_write(which));
    serial_out(which, SERIAL_PORT_DATA, data);
}

/*
 * Writes as much data as will fit to the serial UART
 * tx queue, up to len bytes. Returns the actual number
 * of bytes written.
 */
int
serial_write_upto(int which, const uint8_t *buf, int len)
{
    int i = 0;
    while (i < len && serial_can_write(which)) {
        serial_out(which, SERIAL_PORT_DATA, buf[i++]);
    }
    return i;
}

/*
 * Writes a buffer of characters to the serial UART tx queue.
 * Blocks until the entire buffer has been written.
 */
void
serial_write_chars_blocking(int which, const char *buf, int len)
{
    while (len--) {
        char c = *buf++;
        if (c == '\n') {
            /*
             * QEMU VC doesn't treat \n as \r\n, so we need
             * to send the \r ourselves.
             */
            serial_write_blocking(which, '\r');
            serial_write_blocking(which, '\n');
        } else {
            serial_write_blocking(which, c);
        }
    }
}

/*
 * Configures the UART with the specified parameters
 * and registers an IRQ handler (if not null). This
 * must only be called once per COM port.
 */
void
serial_configure(
    int which,
    int baud_rate,
    int char_bits,
    int stop_bits,
    int parity,
    int trigger_level,
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
    int baud_divisor = SERIAL_CLOCK_HZ / baud_rate;
    assert(baud_divisor * baud_rate == SERIAL_CLOCK_HZ);
    assert((baud_divisor & ~0xffff) == 0);
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
    if (irq_handler != NULL) {
        if (which == 1) {
            irq_register_handler(IRQ_COM1, irq_handler);
        } else if (which == 2) {
            irq_register_handler(IRQ_COM2, irq_handler);
        }
    }
}
