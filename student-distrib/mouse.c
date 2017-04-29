#include "mouse.h"
#include "ps2.h"
#include "lib.h"
#include "process.h"
#include "debug.h"
#include "terminal.h"

/* Handles mouse interrupts. */
void
mouse_handle_irq(void)
{
    /* Read mouse packets from PS/2 data port */
    mouse_input_t input;
    input.flags = ps2_read_data();
    input.dx = ps2_read_data();
    input.dy = ps2_read_data();

    /* Deliver input to terminal */
    terminal_handle_mouse_input(input);
}

/* Initializes the mouse. */
void
mouse_init(void)
{
    /* Enable PS/2 port */
    ps2_write_command(PS2_CMD_ENABLE_MOUSE);

    /* Read config byte */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    uint8_t config_byte = ps2_read_data();

    /* Enable mouse interrupts */
    config_byte |= 0x02;

    /* Write config byte */
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config_byte);

    /* Enable mouse packet streaming */
    ps2_write_mouse(PS2_MOUSE_ENABLE);
}
