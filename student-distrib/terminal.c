#include "terminal.h"
#include "types.h"
#include "lib.h"
#include "debug.h"
#include "keyboard.h"

/* Holds information about each terminal */
static terminal_state_t terminal_states[NUM_TERMINALS] = {
    {
        /* First terminal initially points directly to the VGA buffer */
        .video_mem = (uint8_t *)VIDEO_MEM
    },
    {
        /* Remaining terminals point to their backing buffers */
        .video_mem = terminal_states[1].backing_mem
    },
    {
        .video_mem = terminal_states[2].backing_mem
    },
};

/* Index of the currently displayed terminal */
static int32_t display_terminal = 0;

/* VGA video memory */
static uint8_t *global_video_mem = (uint8_t *)VIDEO_MEM;

/*
 * Returns the terminal corresponding to the currently
 * executing process. Note that THIS IS NOT NECESSARILY
 * THE DISPLAY TERMINAL!
 */
terminal_state_t *
get_executing_terminal(void)
{
    /* TODO: Change this in CP5 */
    return &terminal_states[display_terminal];
}

/*
 * Returns the terminal that is currently displayed on
 * the screen.
 */
terminal_state_t *
get_display_terminal(void)
{
    return &terminal_states[display_terminal];
}

/*
 * Sets the contents of a VGA register.
 * index - the register index
 * value - the new contents of the register
 */
static void
vga_set_register(uint8_t index, uint8_t value)
{
    outb(index, VGA_PORT_INDEX);
    outb(value, VGA_PORT_DATA);
}

/*
 * Sets the VGA cursor position to the cursor position
 * in the specified terminal. Only has an effect if the
 * terminal is currently displayed.
 */
static void
terminal_update_cursor(terminal_state_t *term)
{
    /* Write the position to the VGA cursor position registers */
    uint16_t pos = term->cursor.screen_y * NUM_COLS + term->cursor.screen_x;
    vga_set_register(VGA_REG_CURSOR_LO, (pos >> 0) & 0xff);
    vga_set_register(VGA_REG_CURSOR_HI, (pos >> 8) & 0xff);
}

/*
 * Swaps the VGA video memory buffers.
 */
static void
terminal_swap_buffer(terminal_state_t *old, terminal_state_t *new)
{
    /*
     * The code works fine if we don't check this edge case,
     * it's just an optimization to prevent 2 pointless memcpys.
     */
    if (old == new) {
        return;
    }

    /*
     * Copy the global VGA memory to the previously displayed
     * terminal's backing buffer, then point its active video
     * memory to the backing buffer
     */
    memcpy(old->backing_mem, global_video_mem, VIDEO_MEM_SIZE);
    old->video_mem = old->backing_mem;

    /*
     * Copy the contents of the new terminal's backing buffer
     * into global VGA memory, then point its active video
     * memory to the global VGA memory
     */
    memcpy(global_video_mem, new->backing_mem, VIDEO_MEM_SIZE);
    new->video_mem = global_video_mem;
}

/*
 * Scrolls the specified terminal down. This does
 * NOT decrement the cursor y position!
 */
static void
terminal_scroll_down(terminal_state_t *term)
{
    int32_t bytes_per_row = NUM_COLS << 1;
    int32_t shift_count = ((NUM_COLS * NUM_ROWS) << 1) - bytes_per_row;

    /* Shift rows forward by one row */
    memmove(term->video_mem, term->video_mem + bytes_per_row, shift_count);

    /* Clear out last row */
    memset(term->video_mem + shift_count, 0, bytes_per_row);
}

/*
 * Writes a character at the current cursor position
 * in the specified terminal. This does NOT increment
 * the cursor position!
 */
static void
terminal_write_char(terminal_state_t *term, uint8_t c)
{
    int32_t x = term->cursor.screen_x;
    int32_t y = term->cursor.screen_y;
    term->video_mem[((y * NUM_COLS + x) << 1) + 0] = c;
    term->video_mem[((y * NUM_COLS + x) << 1) + 1] = ATTRIB;
}

/* Prints a character to the specified terminal */
static void
terminal_putc_impl(terminal_state_t *term, uint8_t c)
{
    if (c == '\n') {
        /* Reset x position, increment y position */
        term->cursor.logical_x = 0;
        term->cursor.screen_x = 0;
        term->cursor.screen_y++;

        /* Scroll if we're at the bottom */
        if (term->cursor.screen_y >= NUM_ROWS) {
            terminal_scroll_down(term);
            term->cursor.screen_y--;
        }
    } else if (c == '\r') {
        /* Just reset x position */
        term->cursor.logical_x = 0;
        term->cursor.screen_x = 0;
    } else if (c == '\b') {
        /* Only allow when there's something on this logical line */
        if (term->cursor.logical_x > 0) {
            term->cursor.logical_x--;
            term->cursor.screen_x--;

            /* If we're off-screen, move the cursor back up a line */
            if (term->cursor.screen_x < 0) {
                term->cursor.screen_y--;
                term->cursor.screen_x += NUM_COLS;
            }

            /* Clear the character under the cursor */
            terminal_write_char(term, ' ');
        }
    } else {
        /* Write the character to screen */
        terminal_write_char(term, c);

        /* Move the cursor rightwards, with text wrapping */
        term->cursor.logical_x++;
        term->cursor.screen_x++;
        if (term->cursor.screen_x >= NUM_COLS) {
            term->cursor.screen_y++;
            term->cursor.screen_x -= NUM_COLS;
        }

        /* Scroll if we wrapped some text at the bottom */
        if (term->cursor.screen_y >= NUM_ROWS) {
            terminal_scroll_down(term);
            term->cursor.screen_y--;
        }
    }

    /* Update cursor position */
    terminal_update_cursor(term);
}

/*
 * Clears the specified terminal and resets the cursor
 * position. This does NOT clear the input buffer.
 */
static void
terminal_clear_impl(terminal_state_t *term)
{
    int32_t i;

    /* Clear characters */
    for (i = 0; i < NUM_ROWS * NUM_COLS; ++i) {
        term->video_mem[(i << 1) + 0] = ' ';
        term->video_mem[(i << 1) + 1] = ATTRIB;
    }

    /* Reset cursor to top-left position */
    term->cursor.logical_x = 0;
    term->cursor.screen_x = 0;
    term->cursor.screen_y = 0;

    /* Update cursor position */
    terminal_update_cursor(term);
}

/*
 * Sets the index of the DISPLAYED terminal. This is
 * NOT the same as the EXECUTING terminal! The index
 * must be in the range [0, NUM_TERMINALS).
 */
void
set_display_terminal(int32_t index)
{
    terminal_state_t *old = get_display_terminal();
    terminal_state_t *new;

    ASSERT(index >= 0 && index < NUM_TERMINALS);
    display_terminal = index;
    new = get_display_terminal();

    terminal_swap_buffer(old, new);
    terminal_update_cursor(new);
}

/* Prints a character to the currently executing terminal */
void
terminal_putc(uint8_t c)
{
    terminal_state_t *term = get_executing_terminal();
    terminal_putc_impl(term, c);
}

/* Clears the curently executing terminal screen */
void
terminal_clear(void)
{
    terminal_state_t *term = get_executing_terminal();
    terminal_clear_impl(term);
}

/*
 * Waits until the input buffer can be read. This is satisfied
 * when one of these conditions are met:
 *
 * 1. We have enough characters in the buffer to fill the output buffer
 * 2. We have a \n character in the buffer
 *
 * Returns the number of characters that should be read.
 */
static int32_t
wait_until_readable(volatile input_buf_t *input_buf, int32_t nbytes)
{
    int i;

    /*
     * If nbytes <= number of chars in the buffer, we just
     * return as much as will fit.
     */
    while (nbytes > input_buf->count) {
        /*
         * Check if we have a newline character, if so, we should
         * only read up to and including that character
         */
        for (i = 0; i < input_buf->count; ++i) {
            if (input_buf->buf[i] == '\n') {
                return i + 1;
            }
        }

        /*
         * Wait for some more input (nicely, to save CPU cycles).
         * There's no race condition between sti and hlt here,
         * since sti will only take effect after the following
         * instruction (hlt) has been executed.
         */
        sti();
        hlt();
        cli();
    }

    return nbytes;
}

/*
 * Read syscall for the terminal (stdin). Reads up to nbytes
 * characters or the first line break, whichever occurs
 * first. Returns the number of characters read.
 * The output from this function is *NOT* NUL-terminated!
 *
 * This call will block until the requested number of
 * characters are available or a newline is encountered.
 *
 * buf - must point to a uint8_t array.
 * nbytes - the maximum number of chars to read.
 */
int32_t
terminal_read(int32_t fd, void *buf, int32_t nbytes)
{
    terminal_state_t *term = get_executing_terminal();
    volatile input_buf_t *input_buf = &term->input;
    uint8_t *dest = (uint8_t *)buf;
    int32_t i;

    /* Only allow reads up to the end of the buffer */
    if (nbytes > TERMINAL_BUF_SIZE) {
        nbytes = TERMINAL_BUF_SIZE;
    }

    /* Wait until we can read everything in one go */
    nbytes = wait_until_readable(input_buf, nbytes);

    /* Copy from input buffer to dest buffer */
    for (i = 0; i < nbytes; ++i) {
        *dest++ = input_buf->buf[i];
    }

    /*
     * Shift remaining characters to the front of the buffer
     * Interrupts are cleared at this point so no need to worry
     * about discarding the volatile qualifier
     */
    memmove((void *)&input_buf->buf[0], (void *)&input_buf->buf[i], i);
    input_buf->count -= i;

    /* i holds the number of characters read */
    return i;
}

/*
 * Write syscall for the terminal (stdout). Echos the characters
 * in buf to the terminal. The buffer should not contain any
 * NUL characters. Returns the number of characters written.
 *
 * buf - must point to a uint8_t array
 * nbytes - the number of characters to write to the terminal
 */
int32_t
terminal_write(int32_t fd, const void *buf, int32_t nbytes)
{
    terminal_state_t *term = get_executing_terminal();
    int32_t i;
    const uint8_t *src = (const uint8_t *)buf;
    for (i = 0; i < nbytes; ++i) {
        terminal_putc_impl(term, src[i]);
    }
    return nbytes;
}

/* Handles a keyboard control sequence */
static void
handle_ctrl_input(kbd_input_ctrl_t ctrl)
{
    switch (ctrl) {
    case KCTL_CLEAR:
        terminal_clear();
        break;
    case KCTL_TERM1:
        set_display_terminal(0);
        break;
    case KCTL_TERM2:
        set_display_terminal(1);
        break;
    case KCTL_TERM3:
        set_display_terminal(2);
        break;
    default:
        debugf("Invalid control sequence: %d\n", ctrl);
        break;
    }
}

/* Handles single-character keyboard input */
static void
handle_char_input(uint8_t c)
{
    terminal_state_t *term = get_executing_terminal();
    volatile input_buf_t *input_buf = &term->input;
    if (c == '\b' && input_buf->count > 0 && term->cursor.logical_x > 0) {
        input_buf->count--;
        terminal_putc_impl(term, c);
    } else if (c != '\b' && input_buf->count < TERMINAL_BUF_SIZE) {
        input_buf->buf[input_buf->count++] = c;
        terminal_putc_impl(term, c);
    }
}

/* Handles input from the keyboard */
void
terminal_handle_input(kbd_input_t input)
{
    switch (input.type) {
    case KTYP_CHAR:
        handle_char_input(input.value.character);
        break;
    case KTYP_CTRL:
        handle_ctrl_input(input.value.control);
        break;
    case KTYP_NONE:
        break;
    default:
        debugf("Invalid input type: %d\n", input.type);
        break;
    }
}
