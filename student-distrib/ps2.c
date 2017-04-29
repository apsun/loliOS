#include "ps2.h"
#include "lib.h"
#include "irq.h"
#include "debug.h"
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
 * Waits for the PS/2 input buffer to be empty.
 */
static void
ps2_wait_input(void)
{
    while ((ps2_read_status() & PS2_STATUS_HAS_IN) != 0);
}

/*
 * Waits for the PS/2 output buffer to be full.
 */
static void
ps2_wait_output(void)
{
    while ((ps2_read_status() & PS2_STATUS_HAS_OUT) == 0);
}

/*
 * Sends a command to the PS/2 controller.
 */
void
ps2_write_command(uint8_t cmd)
{
    ps2_wait_input();
    outb(cmd, PS2_PORT_CMD);
}

/*
 * Writes a byte to the PS/2 data port.
 */
void
ps2_write_data(uint8_t data)
{
    ps2_wait_input();
    outb(data, PS2_PORT_DATA);
}

/*
 * Reads a byte from the PS/2 data port.
 */
uint8_t
ps2_read_data(void)
{
    ps2_wait_output();
    return inb(PS2_PORT_DATA);
}

/*
 * Waits for a PS/2 ACK packet.
 */
void
ps2_wait_ack(void)
{
    uint8_t ack = ps2_read_data();
    if (ack != PS2_DATA_ACK) {
        debugf("Received non-ACK PS/2 response\n");
    }
}

/*
 * Sends a byte to the PS/2 keyboard.
 */
void
ps2_write_keyboard(uint8_t data)
{
    ps2_write_data(data);
    ps2_wait_ack();
}

/*
 * Sends a byte to the PS/2 mouse.
 */
void
ps2_write_mouse(uint8_t data)
{
    ps2_write_command(PS2_CMD_NEXT_MOUSE);
    ps2_write_data(data);
    ps2_wait_ack();
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
    /* Initialize keyboard */
    keyboard_init();

    /* Initialize mouse */
    mouse_init();

    /* Register IRQ handlers */
    irq_register_handler(IRQ_KEYBOARD, ps2_handle_irq);
    irq_register_handler(IRQ_MOUSE, ps2_handle_irq);
}
