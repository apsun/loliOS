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
static void
ps2_write_command(uint8_t cmd)
{
    ps2_wait_input();
    outb(cmd, PS2_PORT_CMD);
}

/*
 * Writes a byte to the PS/2 data port.
 */
static void
ps2_write_data(uint8_t data)
{
    ps2_wait_input();
    outb(data, PS2_PORT_DATA);
}

/*
 * Reads a byte from the PS/2 data port.
 */
static uint8_t
ps2_read_data(void)
{
    ps2_wait_output();
    return inb(PS2_PORT_DATA);
}

/*
 * Waits for a PS/2 ACK packet.
 */
static void
ps2_wait_ack(void)
{
    uint8_t ack = ps2_read_data();
    ASSERT(ack == PS2_DATA_ACK);
}

/*
 * Sends a byte to the PS/2 keyboard.
 */
static void
ps2_write_keyboard(uint8_t data)
{
    ps2_write_data(data);
    ps2_wait_ack();
}

/*
 * Sends a byte to the PS/2 mouse.
 */
static void
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
            uint8_t data = ps2_read_data();
            keyboard_handle_irq(data);
        } else {
            uint8_t data[3];
            data[0] = ps2_read_data();
            data[1] = ps2_read_data();
            data[2] = ps2_read_data();
            mouse_handle_irq(data);
        }
    }
}

/*
 * Initializes the PS/2 devices.
 */
void
ps2_init(void)
{
    /* Enable PS/2 ports */
    ps2_write_command(PS2_CMD_ENABLE_KEYBOARD);
    ps2_write_command(PS2_CMD_ENABLE_MOUSE);

    /* Read config byte */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    uint8_t config_byte = ps2_read_data();

    /* Enable keyboard and mouse interrupts */
    config_byte |= 0x03;

    /* Write config byte */
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config_byte);

    /* Enable mouse packet streaming */
    ps2_write_mouse(PS2_MOUSE_ENABLE);

    /* Register IRQ handlers */
    irq_register_handler(IRQ_KEYBOARD, ps2_handle_irq);
    irq_register_handler(IRQ_MOUSE, ps2_handle_irq);
}
