#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "types.h"
#include "file.h"
#include "keyboard.h"
#include "mouse.h"

/* Number of supported terminals */
#define NUM_TERMINALS 3

/* ioctl() commands */
#define STDIN_NONBLOCK 1

#ifndef ASM

/* Cursor position information */
typedef struct {
    /* The cursor x-position in the current logical line.
     * This value can extend beyond NUM_COLS. This is used
     * to determine whether we can backspace across screen lines.
     * This is reset to 0 whenever encountering a '\n' character.
     */
    int logical_x;

    /* The cursor x-position in the current screen line.
     * This value must be less than NUM_COLS.
     */
    int screen_x;

    /* The cursor y-position in the current screen line. */
    int screen_y;
} cursor_pos_t;

/* Combined terminal state information */
typedef struct {
    /* Keyboard input buffer */
    kbd_input_buf_t kbd_input;

    /* Mouse input buffer */
    mouse_input_buf_t mouse_input;

    /* Cursor position */
    cursor_pos_t cursor;

    /* Backing video memory */
    uint8_t *backing_mem;

    /*
     * Pointer to the video memory where the contents
     * of this terminal should be displayed. Either points
     * to the global VGA video memory or to the per-terminal
     * backing_mem field.
     */
    uint8_t *video_mem;

    /*
     * True iff the process currently executing in this terminal
     * has called vidmap.
     */
    bool vidmap;
} terminal_state_t;

/* Terminal syscall functions */
int terminal_kbd_open(const char *filename, file_obj_t *file);
int terminal_stdin_read(file_obj_t *file, void *buf, int nbytes);
int terminal_stdin_write(file_obj_t *file, const void *buf, int nbytes);
int terminal_stdout_read(file_obj_t *file, void *buf, int nbytes);
int terminal_stdout_write(file_obj_t *file, const void *buf, int nbytes);
int terminal_kbd_close(file_obj_t *file);
int terminal_stdin_ioctl(file_obj_t *file, int req, int arg);
int terminal_stdout_ioctl(file_obj_t *file, int req, int arg);

/* Mouse syscall handlers */
int terminal_mouse_open(const char *filename, file_obj_t *file);
int terminal_mouse_read(file_obj_t *file, void *buf, int nbytes);
int terminal_mouse_write(file_obj_t *file, const void *buf, int nbytes);
int terminal_mouse_close(file_obj_t *file);
int terminal_mouse_ioctl(file_obj_t *file, int req, int arg);

/* Sets the currently displayed terminal */
void set_display_terminal(int index);

/* Prints a character to the curently executing terminal */
void terminal_putc(char c);

/* Prints a string to the currently displayed terminal */
void terminal_puts(const char *s);

/* Clears the curently executing terminal */
void terminal_clear(void);

/* Clears the specified terminal's input buffers */
void terminal_clear_input(int terminal);

/* Handles keyboard input */
void terminal_handle_kbd_input(kbd_input_t input);

/* Handles mouse input */
void terminal_handle_mouse_input(mouse_input_t input);

/* Updates the vidmap status for the specified terminal */
void terminal_update_vidmap(int term_index, bool present);

/* Initializes the terminal */
void terminal_init(void);

#endif /* ASM */

#endif /* _TERMINAL_H */