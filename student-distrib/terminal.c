#include "terminal.h"
#include "types.h"
#include "lib.h"
#include "debug.h"
#include "keyboard.h"
#include "process.h"
#include "paging.h"
#include "signal.h"

/* Address of the global video memory page */
#define VGA_MEMORY ((uint8_t *)VIDEO_PAGE_START)

/* Holds information about each terminal */
static terminal_state_t terminal_states[NUM_TERMINALS];

/* Index of the currently displayed terminal */
static int32_t display_terminal = -1;

/*
 * Returns the specified terminal.
 */
static terminal_state_t *
get_terminal(int32_t index)
{
    ASSERT(index >= 0 && index < NUM_TERMINALS);
    return &terminal_states[index];
}

/*
 * Returns the terminal corresponding to the currently
 * executing process. Note that THIS IS NOT NECESSARILY
 * THE DISPLAY TERMINAL!
 */
static terminal_state_t *
get_executing_terminal(void)
{
    pcb_t *pcb = get_executing_pcb();
    ASSERT(pcb != NULL);
    return get_terminal(pcb->terminal);
}

/*
 * Returns the terminal that is currently displayed on
 * the screen.
 */
static terminal_state_t *
get_display_terminal(void)
{
    return get_terminal(display_terminal);
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
 * in the specified terminal.
 */
static void
terminal_update_cursor(terminal_state_t *term)
{
    /* Ignore if this terminal isn't being displayed */
    if (term != get_display_terminal()) {
        return;
    }

    /* Write the position to the VGA cursor position registers */
    uint16_t pos = term->cursor.screen_y * NUM_COLS + term->cursor.screen_x;
    vga_set_register(VGA_REG_CURSOR_LO, (pos >> 0) & 0xff);
    vga_set_register(VGA_REG_CURSOR_HI, (pos >> 8) & 0xff);
}

/* Clears out a region of VGA memory (overwrites it with spaces) */
static void
vga_clear_region(uint8_t *ptr, int32_t num_chars)
{
    /*
     * Screen clear memset pattern, same as [0] = ' ', [1] = ATTRIB
     * Why not a simple for loop? Because I can.
     */
    int32_t pattern = (' ' << 0) | (ATTRIB << 8);
    memset_word(ptr, pattern, num_chars);
}

/*
 * Swaps the VGA video memory buffers.
 */
static void
terminal_swap_buffer(terminal_state_t *old, terminal_state_t *new)
{
    /* Old terminal must have been the display terminal */
    ASSERT(old->video_mem == VGA_MEMORY);

    /*
     * Copy the global VGA memory to the previously displayed
     * terminal's backing buffer, then point its active video
     * memory to the backing buffer
     */
    memcpy(old->backing_mem, VGA_MEMORY, VIDEO_MEM_SIZE);
    old->video_mem = old->backing_mem;

    /*
     * Copy the contents of the new terminal's backing buffer
     * into global VGA memory, then point its active video
     * memory to the global VGA memory
     */
    memcpy(VGA_MEMORY, new->backing_mem, VIDEO_MEM_SIZE);
    new->video_mem = VGA_MEMORY;
}

/*
 * Scrolls the specified terminal down. This does
 * NOT decrement the cursor y position!
 */
static void
terminal_scroll_down(terminal_state_t *term)
{
    int32_t bytes_per_row = NUM_COLS << 1;
    int32_t shift_count = VIDEO_MEM_SIZE - bytes_per_row;

    /* Shift rows forward by one row */
    memmove(term->video_mem, term->video_mem + bytes_per_row, shift_count);

    /* Clear out last row */
    vga_clear_region(term->video_mem + shift_count, bytes_per_row / 2);
}

/*
 * Writes a character at the current cursor position
 * in the specified terminal. This does NOT update
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

/*
 * Prints a character to the specified terminal.
 * This does NOT update the cursor position!
 */
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
}

/*
 * Clears the specified terminal and resets the cursor
 * position. This does NOT clear the input buffer.
 */
static void
terminal_clear_impl(terminal_state_t *term)
{
    /* Clear screen */
    vga_clear_region(term->video_mem, VIDEO_MEM_SIZE / 2);

    /* Reset cursor to top-left position */
    term->cursor.logical_x = 0;
    term->cursor.screen_x = 0;
    term->cursor.screen_y = 0;

    /* Update cursor position */
    if (term == get_display_terminal()) {
        terminal_update_cursor(term);
    }
}

/*
 * Sets the index of the DISPLAYED terminal. This is
 * NOT the same as the EXECUTING terminal! The index
 * must be in the range [0, NUM_TERMINALS).
 */
void
set_display_terminal(int32_t index)
{
    ASSERT(index >= 0 && index < NUM_TERMINALS);
    int32_t old_index = display_terminal;
    if (index == old_index) {
        return;
    }

    /* Set the new display terminal */
    terminal_state_t *old = get_display_terminal();
    display_terminal = index;
    terminal_state_t *new = get_display_terminal();

    /* Swap active VGA memory buffers */
    terminal_swap_buffer(old, new);

    /* Update the cursor position for the new terminal screen */
    terminal_update_cursor(new);

    /*
     * If the currently executing process has a vidmap
     * page and that terminal had its active memory buffer
     * swapped, update the vidmap page.
     */
    terminal_state_t *exec = get_executing_terminal();
    if (exec->vidmap && (old == exec || new == exec)) {
        paging_update_vidmap_page(exec->video_mem, true);
    }
}

/* Prints a character to the currently displayed terminal */
void
terminal_putc(uint8_t c)
{
    terminal_state_t *term = get_display_terminal();
    terminal_putc_impl(term, c);
    terminal_update_cursor(term);
}

/* Clears the curently displayed terminal screen */
void
terminal_clear(void)
{
    terminal_state_t *term = get_display_terminal();
    terminal_clear_impl(term);
    term->kbd_input.count = 0;
}

/* Clears the specified terminal's input buffers */
void
terminal_clear_input(int32_t terminal)
{
    terminal_state_t *term = get_terminal(terminal);
    term->kbd_input.count = 0;
    term->mouse_input.count = 0;
}

/*
 * Waits until the keyboard input buffer can be read. This is satisfied
 * when one of these conditions are met:
 *
 * 1. We have enough characters in the buffer to fill the output buffer
 * 2. We have a \n character in the buffer
 *
 * Returns the number of characters that should be read.
 */
static int32_t
terminal_wait_kbd_input(kbd_input_buf_t *input_buf, int32_t nbytes)
{
    /*
     * If nbytes <= number of chars in the buffer, we just
     * return as much as will fit.
     */
    int32_t count;
    while (nbytes > (count = input_buf->count)) {
        /*
         * Check if we have a newline character, if so, we should
         * only read up to and including that character
         */
        int32_t i;
        for (i = 0; i < count; ++i) {
            if (input_buf->buf[i] == '\n') {
                return i + 1;
            }
        }

        /* Exit early if we have a pending signal */
        if (signal_has_pending()) {
            return -1;
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
 * Open syscall for stdin/stdout. Always succeeds.
 */
int32_t
terminal_kbd_open(const uint8_t *filename, file_obj_t *file)
{
    return 0;
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
 * file - ignored.
 * buf - must point to a uint8_t array.
 * nbytes - the maximum number of chars to read.
 */
int32_t
terminal_stdin_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    /* Ensure buffer is valid */
    if (!is_user_writable(buf, nbytes)) {
        return -1;
    }

    terminal_state_t *term = get_executing_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;

    /* Only allow reads up to the end of the buffer */
    if (nbytes > KEYBOARD_BUF_SIZE) {
        nbytes = KEYBOARD_BUF_SIZE;
    }

    /*
     * Wait until we can read everything in one go
     * Interrupts must be disabled upon entry, and
     * will be disabled upon return.
     */
    nbytes = terminal_wait_kbd_input(input_buf, nbytes);

    /* Abort if we have pending signals */
    if (nbytes < 0) {
        return -1;
    }

    /* Copy input buffer to userspace */
    if (!copy_to_user(buf, (void *)input_buf->buf, nbytes)) {
        return -1;
    }

    /* Shift remaining characters to the front of the buffer */
    memmove((void *)&input_buf->buf[0], (void *)&input_buf->buf[nbytes], nbytes);
    input_buf->count -= nbytes;

    /* nbytes holds the number of characters read */
    return nbytes;
}

/*
 * Write syscall for stdin. Always fails.
 */
int32_t
terminal_stdin_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return -1;
}

/*
 * Read syscall for stdout. Always fails.
 */
int32_t
terminal_stdout_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    return -1;
}

/*
 * Write syscall for the terminal (stdout). Echos the characters
 * in buf to the terminal. The buffer should not contain any
 * NUL characters. Returns the number of characters written.
 *
 * file - ignored.
 * buf - must point to a uint8_t array
 * nbytes - the number of characters to write to the terminal
 */
int32_t
terminal_stdout_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    /*
     * Ensure the entire buffer is readable
     * before we begin, since we shouldn't have
     * partial writes to the terminal
     */
    if (!is_user_readable(buf, nbytes)) {
        return -1;
    }

    const uint8_t *src = (const uint8_t *)buf;
    terminal_state_t *term = get_executing_terminal();

    /* Print characters to the terminal (don't update cursor) */
    int32_t i;
    for (i = 0; i < nbytes; ++i) {
        terminal_putc_impl(term, src[i]);
    }

    /* Update cursor position */
    terminal_update_cursor(term);

    return nbytes;
}

/*
 * Close syscall for stdin/stdout. Always fails.
 */
int32_t
terminal_kbd_close(file_obj_t *file)
{
    return -1;
}

/*
 * Open syscall for the mouse. Always succeeds.
 */
int32_t
terminal_mouse_open(const uint8_t *filename, file_obj_t *file)
{
    return 0;
}

/*
 * Read syscall for the mouse. This does NOT block if no
 * inputs are available. Copies at most nbytes / sizeof(mouse_input_t)
 * input events to buf. If no events are available, this
 * simply returns 0.
 */
int32_t
terminal_mouse_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    /* Get the mouse buffer for the executing terminal */
    terminal_state_t *term = get_executing_terminal();
    mouse_input_buf_t *input_buf = &term->mouse_input;

    /*
     * Return either the number of inputs in the buffer,
     * or the number of inputs the user requested, whichever
     * one is smaller.
     */
    int32_t num_copy = nbytes / sizeof(mouse_input_t);
    if (num_copy > input_buf->count) {
        num_copy = input_buf->count;
    }

    /* Number of bytes we actually copy */
    int32_t num_bytes_copy = num_copy * sizeof(mouse_input_t);

    /* Copy input buffer to userspace */
    if (!copy_to_user(buf, (void *)input_buf->buf, num_bytes_copy)) {
        return -1;
    }

    /* Shift remaining inputs to the front */
    memmove((void *)&input_buf->buf[0], (void *)&input_buf->buf[num_copy], num_bytes_copy);
    input_buf->count -= num_copy;

    /* Return the number of bytes copied into the buffer */
    return num_bytes_copy;
}

/*
 * Write syscall for the mouse. Always fails.
 */
int32_t
terminal_mouse_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return -1;
}

/*
 * Close syscall for the mouse. Always succeeds.
 */
int32_t
terminal_mouse_close(file_obj_t *file)
{
    return 0;
}

/*
 * Handles CTRL-C input by sending an interrupt signal
 * to the process executing in the current terminal.
 */
static void
terminal_interrupt(void)
{
    pcb_t *pcb = get_pcb_by_terminal(display_terminal);
    if (pcb == NULL) {
        debugf("No process running in display terminal\n");
        return;
    }

    signal_raise(pcb->pid, SIG_INTERRUPT);
}

/* Handles a keyboard control sequence */
static void
handle_ctrl_input(kbd_input_ctrl_t ctrl)
{
    switch (ctrl) {
    case KCTL_CLEAR:
        terminal_clear();
        break;
    case KCTL_INTERRUPT:
        terminal_interrupt();
        break;
    case KCTL_TERM1:
    case KCTL_TERM2:
    case KCTL_TERM3:
        set_display_terminal(ctrl - KCTL_TERM1);
        break;
    default:
        ASSERT(0);
        break;
    }
}

/* Handles single-character keyboard input */
static void
handle_char_input(uint8_t c)
{
    /* We should insert characters into the currently displayed
     * terminal's input stream, not the currently executing terminal.
     */
    terminal_state_t *term = get_display_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;
    if (c == '\b' && input_buf->count > 0 && term->cursor.logical_x > 0) {
        input_buf->count--;
        terminal_putc_impl(term, c);
        terminal_update_cursor(term);
    } else if ((c != '\b' && input_buf->count < KEYBOARD_BUF_SIZE - 1) ||
               (c == '\n' && input_buf->count < KEYBOARD_BUF_SIZE)) {
        input_buf->buf[input_buf->count++] = c;
        terminal_putc_impl(term, c);
        terminal_update_cursor(term);
    }
}

/* Handles input from the keyboard */
void
terminal_handle_kbd_input(kbd_input_t input)
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
        ASSERT(0);
        break;
    }
}

/* Handles input from the mouse */
void
terminal_handle_mouse_input(mouse_input_t input)
{
    terminal_state_t *term = get_display_terminal();
    mouse_input_buf_t *input_buf = &term->mouse_input;
    if (input_buf->count == MOUSE_BUF_SIZE) {
        debugf("Mouse buffer full, dropping packet\n");
    } else if (input_buf->count >= 0) {
        input_buf->buf[input_buf->count++] = input;
    }
}

/*
 * Updates the vidmap page to point to the specified terminal's
 * active video memory page. If present is false, the vidmap page
 * is disabled. Returns the address of the vidmap page.
 */
void
terminal_update_vidmap(int32_t term_index, bool present)
{
    terminal_state_t *term = get_terminal(term_index);
    paging_update_vidmap_page(term->video_mem, present);
    term->vidmap = present;
}

/*
 * Initialize all terminals. This must be called before
 * any printing functions!
 */
void
terminal_init(void)
{
    int32_t i;

    for (i = 0; i < NUM_TERMINALS; ++i) {
        /*
         * Backing memory is the per-terminal page
         * Note that it's safe to do this before initializing paging
         * since everything is accessible at that point
         */
        terminal_states[i].backing_mem = (uint8_t *)TERMINAL_PAGE_START + KB(i * 4);

        /* Active memory points to the backing memory */
        terminal_states[i].video_mem = terminal_states[i].backing_mem;

        /* Initialize the terminal memory region */
        vga_clear_region(terminal_states[i].backing_mem, VIDEO_MEM_SIZE / 2);
    }

    /* First terminal's active video memory points to global VGA memory */
    terminal_states[0].video_mem = VGA_MEMORY;

    /* Set initially displayed terminal */
    display_terminal = 0;
}
