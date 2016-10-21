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

/* Character input buffer */
typedef struct {
    /* Buffer to hold the characters */
    uint8_t buf[TERMINAL_BUF_SIZE];

    /* Number of characters in the buffer */
    int32_t count;
} input_buf_t;

/* Cursor position information */
typedef struct {
    /* The cursor x-position in the current logical line.
     * This value can extend beyond NUM_COLS. This is used
     * to determine whether we can backspace across screen lines.
     * This is reset to 0 whenever encountering a '\n' character.
     */
    int32_t logical_x;

    /* The cursor x-position in the current screen line.
     * This value must be less than NUM_COLS.
     */
    int screen_x;

    /* The cursor y-position in the current screen line. */
    int screen_y;
} cursor_pos_t;

/* Combined terminal state information */
typedef struct {
    /* Input buffer */
    volatile input_buf_t input;

    /* Cursor position */
    cursor_pos_t cursor;

    /* Video memory base pointer */
    uint8_t *video_mem;
} terminal_state_t;

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
