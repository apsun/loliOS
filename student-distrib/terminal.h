#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "types.h"
#include "keyboard.h"

#define TERMINAL_BUF_SIZE 128
#define NUM_TERMINALS 1 /* TODO: Support 3 of these */

#define VIDEO 0xB8000
#define NUM_COLS 80
#define NUM_ROWS 25
#define ATTRIB 0x7

#ifndef ASM

/* Terminal syscall functions */
int32_t terminal_read(int32_t fd, void *buf, int32_t nbytes);
int32_t terminal_write(int32_t fd, const void *buf, int32_t nbytes);

/* Prints a character to the terminal */
void terminal_putc(uint8_t c);

/* Clears the terminal screen */
void terminal_clear(void);

/* Handles keyboard input */
void terminal_handle_input(kbd_input_t input);

#endif /* ASM */

#endif /* _TERMINAL_H */
