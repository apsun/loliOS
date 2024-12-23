#include "ps2.h"
#include "types.h"
#include "debug.h"
#include "portio.h"
#include "irq.h"
#include "keyboard.h"
#include "mouse.h"

/*
 * Reads the status of the PS/2 controller.
 */
static uint8_t
ps2_read_status(void)
{
    return inb(PS2_PORT_STATUS);
}

/*
 * Waits for the PS/2 input buffer to be empty (meaning
 * we can write to it).
 */
static void
ps2_wait_write(void)
{
    while ((ps2_read_status() & PS2_STATUS_HAS_IN) != 0);
}

/*
 * Waits for the PS/2 output buffer to be non-empty (meaning
 * we can read from it).
 */
static void
ps2_wait_read(void)
{
    while ((ps2_read_status() & PS2_STATUS_HAS_OUT) == 0);
}

/*
 * Checks if the PS/2 output buffer has data to read.
 */
static bool
ps2_can_read(void)
{
    return (ps2_read_status() & PS2_STATUS_HAS_OUT) != 0;
}

/*
 * Sends a command to the PS/2 controller. This blocks
 * until the write completes.
 */
void
ps2_write_command(uint8_t cmd)
{
    ps2_wait_write();
    outb(cmd, PS2_PORT_CMD);
}

/*
 * Writes a byte to the PS/2 data port. This blocks until
 * the write completes.
 */
void
ps2_write_data(uint8_t data)
{
    ps2_wait_write();
    outb(data, PS2_PORT_DATA);
}

/*
 * Reads a byte from the PS/2 data port. This blocks until
 * the read completes.
 */
uint8_t
ps2_read_data_blocking(void)
{
    ps2_wait_read();
    return inb(PS2_PORT_DATA);
}

/*
 * Reads a byte from the PS/2 data port. This does not block;
 * if there is no data available, returns -EAGAIN immediately.
 */
int
ps2_read_data_nonblocking(void)
{
    if (!ps2_can_read()) {
        return -EAGAIN;
    }
    return inb(PS2_PORT_DATA);
}

/*
 * Waits for a PS/2 ACK packet.
 */
void
ps2_wait_ack(void)
{
    uint8_t ack = ps2_read_data_blocking();
    if (ack != PS2_DATA_ACK) {
        debugf("Received non-ACK PS/2 response\n");
    }
}

/*
 * Handler for keyboard and mouse IRQs.
 */
static void
ps2_handle_irq(void)
{
    /* Drain ALL the input! */
    while (true) {
        uint8_t status = ps2_read_status();
        if ((status & PS2_STATUS_HAS_OUT) == 0) {
            break;
        }

        /* Dispatch to correct handler */
        if ((status & PS2_STATUS_IS_MOUSE) == 0) {
            keyboard_handle_irq();
        } else {
            mouse_handle_irq();
        }
    }
}

/*
 * Initializes the PS/2 devices.
 */
void
ps2_init(void)
{
    /* Drain any leftover data in the output buffer */
    int data;
    while ((data = ps2_read_data_nonblocking()) != -EAGAIN) {
        debugf("Discarding unknown data 0x%02x\n", data);
    }

    /* Initialize devices */
    keyboard_init();
    mouse_init();

    /* Register IRQ handlers */
    irq_register_handler(IRQ_KEYBOARD, ps2_handle_irq);
    irq_register_handler(IRQ_MOUSE, ps2_handle_irq);
}
