#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "types.h"
#include "keyboard.h"
#include "filesys.h"

#define TERMINAL_BUF_SIZE 128
#define NUM_TERMINALS 3

#define NUM_COLS  80
#define NUM_ROWS  25
#define ATTRIB    0x7
#define VIDEO_MEM_SIZE (NUM_ROWS * NUM_COLS * 2)

/* VGA registers */
#define VGA_REG_CURSOR_HI 0x0E
#define VGA_REG_CURSOR_LO 0x0F
#define VGA_PORT_INDEX    0x3D4
#define VGA_PORT_DATA     0x3D5

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
    int32_t screen_x;

    /* The cursor y-position in the current screen line. */
    int32_t screen_y;
} cursor_pos_t;

/* Combined terminal state information */
typedef struct {
    /* Input buffer */
    volatile input_buf_t input;

    /* Cursor position */
    cursor_pos_t cursor;

    /* Backing video memory */
    uint8_t *backing_mem;

    /* Pointer to the video memory where the contents
     * of this terminal should be displayed. Either points
     * to the global VGA video memory or to the per-terminal
     * backing_mem field.
     */
    uint8_t *video_mem;

    /*
     * True iff the process currently executing in this terminal
     * has called vidmap
     */
    bool vidmap;
} terminal_state_t;

/* Terminal syscall functions */
int32_t terminal_open(const uint8_t *filename, file_obj_t *file);
int32_t terminal_stdin_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t terminal_stdin_write(file_obj_t *file, const void *buf, int32_t nbytes);
int32_t terminal_stdout_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t terminal_stdout_write(file_obj_t *file, const void *buf, int32_t nbytes);
int32_t terminal_close(file_obj_t *file);

/* Sets the currently displayed terminal */
void set_display_terminal(int32_t index);

/* Prints a character to the curently executing terminal */
void terminal_putc(uint8_t c);

/* Clears the curently executing terminal */
void terminal_clear(void);

/* Handles keyboard input */
void terminal_handle_input(kbd_input_t input);

/* Updates the vidmap status for the specified terminal */
void terminal_update_vidmap(int32_t term_index, bool present);

/* Initializes the terminal */
void terminal_init(void);

#endif /* ASM */

#endif /* _TERMINAL_H */
