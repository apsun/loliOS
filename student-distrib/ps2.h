#ifndef _PS2_H
#define _PS2_H

#include "types.h"

#define PS2_PORT_DATA 0x60
#define PS2_PORT_STATUS 0x64
#define PS2_PORT_CMD 0x64
#define PS2_STATUS_HAS_OUT (1 << 0)
#define PS2_STATUS_HAS_IN (1 << 1)
#define PS2_STATUS_IS_MOUSE (1 << 5)
#define PS2_CMD_ENABLE_KEYBOARD 0xAE
#define PS2_CMD_ENABLE_MOUSE 0xA8
#define PS2_CMD_NEXT_MOUSE 0xD4
#define PS2_CMD_READ_CONFIG 0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_MOUSE_ENABLE 0xF4
#define PS2_MOUSE_RESOLUTION 0xE8
#define PS2_MOUSE_SAMPLE_RATE 0xF3
#define PS2_DATA_ACK 0xFA

#ifndef ASM

/* Sends a command to the PS/2 controller. */
void ps2_write_command(uint8_t cmd);

/* Writes a byte to the PS/2 data port. */
void ps2_write_data(uint8_t data);

/* Reads a byte from the PS/2 data port. */
uint8_t ps2_read_data(void);

/* Waits for a PS/2 ACK packet. */
void ps2_wait_ack(void);

/* Sends a byte to the PS/2 keyboard. */
void ps2_write_keyboard(uint8_t data);

/* Sends a byte to the PS/2 mouse. */
void ps2_write_mouse(uint8_t data);

/* Initializes PS/2 devices and sets up IRQs. */
void ps2_init(void);

#endif /* ASM */

#endif /* _PS2_H */
