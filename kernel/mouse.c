#include "mouse.h"
#include "types.h"
#include "debug.h"
#include "terminal.h"
#include "ps2.h"

/* Handles mouse interrupts. */
void
mouse_handle_irq(void)
{
    /* Read mouse packets from PS/2 data port */
    int flags = ps2_read_data_nonblocking();
    int dx = ps2_read_data_nonblocking();
    int dy = ps2_read_data_nonblocking();
    if (flags < 0 || dx < 0 || dy < 0) {
        debugf("Got mouse IRQ but no data to read\n");
        return;
    }

    mouse_input_t input;
    input.flags = flags;
    input.dx = dx;
    input.dy = dy;

    /* Deliver input to terminal */
    terminal_handle_mouse_input(input);
}

/* Initializes the mouse. */
void
mouse_init(void)
{
    /* Enable PS/2 port on controller */
    ps2_write_command(PS2_CMD_ENABLE_MOUSE);

    /* Enable interrupts on controller */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    uint8_t config_byte = ps2_read_data_blocking();
    config_byte |= 0x02;
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config_byte);

    /* Enable device */
    ps2_write_command(PS2_CMD_NEXT_MOUSE);
    ps2_write_data(PS2_MOUSE_ENABLE);
    ps2_wait_ack();
}
