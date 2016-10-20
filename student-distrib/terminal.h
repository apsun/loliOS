#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "types.h"
#include "keyboard.h"

#define TERMINAL_BUF_SIZE 128
#define NUM_TERMINALS 1 /* TODO: Support 3 of these */

#ifndef ASM

/* Handles keyboard input */
void terminal_handle_input(kbd_input_t input);

#endif /* ASM */

#endif /* _TERMINAL_H */
