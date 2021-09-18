#include "terminal.h"
#include "types.h"
#include "debug.h"
#include "string.h"
#include "process.h"
#include "paging.h"
#include "signal.h"
#include "scheduler.h"
#include "syscall.h"
#include "vga.h"

/* EOT (CTRL-D) character */
#define EOT '\x04'

/* Terminal config */
#define NUM_COLS 80
#define NUM_ROWS 25
#define ATTRIB 0x07
#define ATTRIB_BSOD 0x1F
#define VIDEO_MEM ((uint8_t *)VIDEO_PAGE_START)
#define VIDEO_MEM_SIZE (NUM_ROWS * NUM_COLS * 2)

/* Holds information about each terminal */
static terminal_state_t terminal_states[NUM_TERMINALS];

/* Index of the currently displayed terminal */
static int display_terminal = 0;

/*
 * Returns a terminal given its index number.
 */
static terminal_state_t *
get_terminal(int index)
{
    assert(index >= 0 && index < NUM_TERMINALS);
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
    assert(pcb != NULL);
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

/* Clears out a region of VGA memory (overwrites it with spaces) */
static void
terminal_clear_region(uint8_t *ptr, int num_chars, char attrib)
{
    uint16_t pattern = (' ' << 0) | (attrib << 8);
    memset_word(ptr, pattern, num_chars);
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
    vga_set_cursor_location(pos);
}

/*
 * Swaps the VGA video memory buffers.
 */
static void
terminal_swap_buffer(terminal_state_t *old, terminal_state_t *new)
{
    /* Old terminal must have been the display terminal */
    assert(old->video_mem == VIDEO_MEM);

    /*
     * Copy the global VGA memory to the previously displayed
     * terminal's backing buffer, then point its active video
     * memory to the backing buffer
     */
    memcpy(old->backing_mem, VIDEO_MEM, VIDEO_MEM_SIZE);
    old->video_mem = old->backing_mem;

    /*
     * Copy the contents of the new terminal's backing buffer
     * into global VGA memory, then point its active video
     * memory to the global VGA memory
     */
    memcpy(VIDEO_MEM, new->backing_mem, VIDEO_MEM_SIZE);
    new->video_mem = VIDEO_MEM;
}

/*
 * Scrolls the specified terminal down. This does
 * NOT decrement the cursor y position!
 */
static void
terminal_scroll_down(terminal_state_t *term)
{
    int bytes_per_row = NUM_COLS << 1;
    int shift_count = VIDEO_MEM_SIZE - bytes_per_row;

    /* Shift rows forward by one row */
    memmove(term->video_mem, term->video_mem + bytes_per_row, shift_count);

    /* Clear out last row */
    terminal_clear_region(term->video_mem + shift_count, bytes_per_row / 2, term->attrib);
}

/*
 * Writes a character at the current cursor position
 * in the specified terminal. This does NOT update
 * the cursor position!
 */
static void
terminal_write_char(terminal_state_t *term, char c)
{
    int x = term->cursor.screen_x;
    int y = term->cursor.screen_y;
    int offset = (y * NUM_COLS + x) << 1;
    term->video_mem[offset] = c;
}

/*
 * Prints a character to the specified terminal.
 * This does NOT update the cursor position!
 */
static void
terminal_putc_impl(terminal_state_t *term, char c)
{
    cursor_pos_t *cur = &term->cursor;

    if (c == '\n') {
        /* Reset x position, increment y position */
        cur->logical_x = 0;
        cur->screen_x = 0;
        cur->screen_y++;

        /* Scroll if we're at the bottom */
        if (cur->screen_y >= NUM_ROWS) {
            terminal_scroll_down(term);
            cur->screen_y--;
        }
    } else if (c == '\r') {
        /* Just reset x position */
        cur->logical_x = 0;
        cur->screen_x = 0;
    } else if (c == '\b') {
        /*
         * Only allow when there's something on this logical line
         * and we are not already at the top-left corner (prevent
         * cursor from wrapping to negative y position)
         */
        if (cur->logical_x > 0 && !(cur->screen_x == 0 && cur->screen_y == 0)) {
            cur->logical_x--;
            cur->screen_x--;

            /* If we're off-screen, move the cursor back up a line */
            if (cur->screen_x < 0) {
                cur->screen_y--;
                cur->screen_x += NUM_COLS;
            }

            /* Clear the character under the cursor */
            terminal_write_char(term, ' ');
        }
    } else {
        /* Write the character to screen */
        terminal_write_char(term, c);

        /* Move the cursor rightwards, with text wrapping */
        cur->logical_x++;
        cur->screen_x++;
        if (cur->screen_x >= NUM_COLS) {
            cur->screen_y++;
            cur->screen_x -= NUM_COLS;
        }

        /* Scroll if we wrapped some text at the bottom */
        if (cur->screen_y >= NUM_ROWS) {
            terminal_scroll_down(term);
            cur->screen_y--;
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
    terminal_clear_region(term->video_mem, VIDEO_MEM_SIZE / 2, term->attrib);

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
set_display_terminal(int index)
{
    assert(index >= 0 && index < NUM_TERMINALS);
    int old_index = display_terminal;
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
        paging_update_vidmap_page((uintptr_t)exec->video_mem, true);
    }
}

/* Prints a character to the currently displayed terminal */
void
terminal_putc(char c)
{
    terminal_state_t *term = get_display_terminal();
    terminal_putc_impl(term, c);
    terminal_update_cursor(term);
}

/* Prints a string to the currently displayed terminal */
void
terminal_puts(const char *s)
{
    terminal_state_t *term = get_display_terminal();
    while (*s) {
        terminal_putc_impl(term, *s++);
    }
    terminal_update_cursor(term);
}

/*
 * Clears the currently displayed terminal screen and
 * all associated input.
 */
void
terminal_clear(void)
{
    terminal_state_t *term = get_display_terminal();
    terminal_clear_impl(term);
    term->kbd_input.count = 0;
    term->mouse_input.count = 0;
}

/* Clears the specified terminal's input buffers */
void
terminal_clear_input(int terminal)
{
    terminal_state_t *term = get_terminal(terminal);
    term->kbd_input.count = 0;
    term->mouse_input.count = 0;
}

/*
 * Clears the currently displayed terminal and puts
 * it into a BSOD state.
 */
void
terminal_clear_bsod(void)
{
    terminal_state_t *term = get_display_terminal();
    term->attrib = ATTRIB_BSOD;
    terminal_clear_impl(term);
}

/*
 * Checks if the keyboard input buffer has enough data to be
 * read. Returns the number of characters that should be read,
 * or -EAGAIN if there is currently nothing to read.
 */
static int
terminal_check_kbd_input(terminal_state_t *term, int nbytes)
{
    pcb_t *pcb = get_executing_pcb();
    kbd_input_buf_t *input_buf = &term->kbd_input;

    /*
     * If the process is not in the foreground group, don't
     * allow the caller to read
     */
    if (term->fg_group != pcb->group) {
        debugf("Attempting to read from background group (fg=%d, curr=%d)\n",
            term->fg_group, pcb->group);
        return -1;
    }

    /*
     * Check if we have a newline or EOT character. If so, we should
     * read up to and including that character (or as many chars
     * as fits in the buffer)
     */
    int count = input_buf->count;
    int i;
    for (i = 0; i < count; ++i) {
        if (input_buf->buf[i] == '\n' || input_buf->buf[i] == EOT) {
            if (nbytes > i + 1) {
                nbytes = i + 1;
            }
            return nbytes;
        }
    }

    return -EAGAIN;
}

/*
 * read() syscall handler for stdin. Reads up to nbytes
 * characters or the first line break or EOT, whichever occurs
 * first. Returns the number of characters read.
 * The output from this function is *NOT* NUL-terminated!
 *
 * This call will block until a newline or EOT is encountered.
 */
static int
terminal_tty_read(file_obj_t *file, void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    terminal_state_t *term = get_executing_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;

    /*
     * Wait until there's a newline/EOT in the buffer or we
     * have pending signals to handle
     */
    nbytes = BLOCKING_WAIT(
        terminal_check_kbd_input(term, nbytes),
        input_buf->sleep_queue,
        file->nonblocking);
    if (nbytes < 0) {
        return nbytes;
    }

    /* Don't actually copy the EOT character */
    int ncopy = nbytes;
    if (input_buf->buf[ncopy - 1] == EOT) {
        ncopy--;
    }

    /* Copy input buffer to userspace */
    if (!copy_to_user(buf, input_buf->buf, ncopy)) {
        return -1;
    }

    /* Shift remaining characters to the front of the buffer */
    memmove(
        &input_buf->buf[0],
        &input_buf->buf[nbytes],
        input_buf->count - nbytes);
    input_buf->count -= nbytes;

    /* ncopy holds the number of characters copied */
    return ncopy;
}

/*
 * write() syscall handler for stdout. Echos the characters
 * in buf to the terminal. Returns the number of characters written.
 */
static int
terminal_tty_write(file_obj_t *file, const void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    /* Cannot write if not in foreground group */
    terminal_state_t *term = get_executing_terminal();
    pcb_t *pcb = get_executing_pcb();
    if (term->fg_group != pcb->group) {
        debugf("Attempting to print from background group (fg=%d, curr=%d)\n",
            term->fg_group, pcb->group);
        return -1;
    }

    /* Copy and print in chunks */
    int copied = 0;
    char block[256];
    const char *bufp = buf;
    while (copied < nbytes) {
        int to_copy = sizeof(block);
        if (to_copy > nbytes - copied) {
            to_copy = nbytes - copied;
        }

        /* Copy some characters from userspace */
        if (!copy_from_user(block, &bufp[copied], to_copy)) {
            break;
        }
        copied += to_copy;

        /* Print characters to the terminal (don't update cursor) */
        int i;
        for (i = 0; i < to_copy; ++i) {
            terminal_putc_impl(term, block[i]);
        }
    }

    /* If no chars copied, buf must be invalid, no need to update cursor */
    if (copied == 0) {
        return -1;
    } else {
        terminal_update_cursor(term);
        return copied;
    }
}

/*
 * read() syscall handler for the mouse. Copies at most
 * nbytes worth of input events into buf (see mouse_input_t for
 * the meaning of the event data). It is possible to read
 * only part of an input event if nbytes is not a multiple
 * of sizeof(mouse_input_t) - the next read() will return
 * the remaining part of the event.
 */
static int
terminal_mouse_read(file_obj_t *file, void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    /* Check that caller is in the foreground group */
    terminal_state_t *term = get_executing_terminal();
    pcb_t *pcb = get_executing_pcb();
    if (term->fg_group != pcb->group) {
        debugf("Attempting to read mouse from background group (fg=%d, curr=%d)\n",
            term->fg_group, pcb->group);
        return -1;
    }

    /* Wait until there's input to read */
    mouse_input_buf_t *input_buf = &term->mouse_input;
    int max_read = BLOCKING_WAIT(
        input_buf->count > 0 ? input_buf->count : -EAGAIN,
        input_buf->sleep_queue,
        file->nonblocking);
    if (max_read < 0) {
        return max_read;
    }

    /*
     * If previous read was partial, next read shall return only
     * the remainder of the partially read input. This is to allow
     * callers to "align" their read offsets. In other words, if
     * read() ever returns 1 or 2, either the buffer is too small
     * or the previous read was partial.
     */
    if (max_read % sizeof(mouse_input_t) != 0) {
        max_read = max_read % sizeof(mouse_input_t);
    }

    /*
     * Return either the number of inputs in the buffer,
     * or the number of inputs the user requested, whichever
     * one is smaller. Note that this may not fit an entire
     * input, in which case we return a partial input instead
     * of failing or blocking forever.
     */
    if (nbytes > max_read) {
        nbytes = max_read;
    }

    /* Copy input buffer to userspace */
    if (!copy_to_user(buf, input_buf->buf, nbytes)) {
        return -1;
    }

    /* Shift remaining inputs to the front */
    memmove(
        &input_buf->buf[0],
        &input_buf->buf[nbytes],
        input_buf->count - nbytes);
    input_buf->count -= nbytes;

    /* Return the number of bytes copied into the buffer */
    return nbytes;
}

/*
 * Handles CTRL-C input by sending an interrupt signal
 * to the foreground process group in the displayed terminal.
 */
static void
terminal_interrupt(void)
{
    terminal_state_t *term = get_display_terminal();
    int pgrp = term->fg_group;
    if (pgrp == 0) {
        debugf("No foreground process group in display terminal\n");
        return;
    }
    signal_kill(-pgrp, SIGINT);
}

/*
 * Handles CTRL-D input by injecting a EOT character into
 * the displayed terminal's input buffer.
 */
static void
terminal_eof(void)
{
    terminal_state_t *term = get_display_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;
    if (input_buf->count < array_len(input_buf->buf)) {
        input_buf->buf[input_buf->count++] = EOT;
        scheduler_wake_all(&input_buf->sleep_queue);
    }
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
    case KCTL_EOF:
        terminal_eof();
        break;
    case KCTL_TERM1:
    case KCTL_TERM2:
    case KCTL_TERM3:
        set_display_terminal(ctrl - KCTL_TERM1);
        break;
    default:
        panic("Unknown control code");
        break;
    }
}

/* Handles single-character keyboard input */
static void
handle_char_input(char c)
{
    terminal_state_t *term = get_display_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;
    if (c == '\b' && input_buf->count > 0 && term->cursor.logical_x > 0) {
        input_buf->count--;
        terminal_putc_impl(term, c);
        terminal_update_cursor(term);
    } else if ((c != '\b' && input_buf->count < array_len(input_buf->buf) - 1) ||
               (c == '\n' && input_buf->count < array_len(input_buf->buf))) {
        input_buf->buf[input_buf->count++] = c;
        terminal_putc_impl(term, c);
        terminal_update_cursor(term);
    }

    /* Wake all processes waiting on input to this terminal */
    scheduler_wake_all(&input_buf->sleep_queue);
}

/* Handles input from the keyboard */
void
terminal_handle_kbd_input(kbd_input_t input)
{
    switch (input.type) {
    case KTYP_CHAR:
        handle_char_input(input.character);
        break;
    case KTYP_CTRL:
        handle_ctrl_input(input.control);
        break;
    case KTYP_NONE:
        break;
    default:
        panic("Unknown keyboard input type");
        break;
    }
}

/* Handles input from the mouse */
void
terminal_handle_mouse_input(mouse_input_t input)
{
    terminal_state_t *term = get_display_terminal();
    mouse_input_buf_t *input_buf = &term->mouse_input;

    /*
     * Only copy the input into the buffer if the entire
     * event will fit. Otherwise, just discard it.
     */
    if (input_buf->count + sizeof(input) <= sizeof(input_buf->buf)) {
        input_buf->buf[input_buf->count++] = input.flags;
        input_buf->buf[input_buf->count++] = input.dx;
        input_buf->buf[input_buf->count++] = input.dy;
        scheduler_wake_all(&input_buf->sleep_queue);
    }
}

/*
 * Updates the vidmap page to point to the specified terminal's
 * active video memory page. If present is false, the vidmap page
 * is disabled.
 */
void
terminal_update_vidmap(int term_index, bool present)
{
    terminal_state_t *term = get_terminal(term_index);
    paging_update_vidmap_page((uintptr_t)term->video_mem, present);
    term->vidmap = present;
}

/* Combined file ops for the stdin/stdout streams */
static const file_ops_t terminal_tty_fops = {
    .read = terminal_tty_read,
    .write = terminal_tty_write,
};

/* Mouse file ops */
static const file_ops_t terminal_mouse_fops = {
    .read = terminal_mouse_read,
};

/*
 * Opens stdin, stdout, and stderr as fds 0, 1, and 2
 * respectively for a given process.
 */
int
terminal_open_streams(file_obj_t **files)
{
    /* Create stdin stream */
    file_obj_t *in = file_obj_alloc(&terminal_tty_fops, OPEN_READ, true);
    if (in == NULL) {
        return -1;
    }

    /* Create stdout stream */
    file_obj_t *out = file_obj_alloc(&terminal_tty_fops, OPEN_WRITE, true);
    if (out == NULL) {
        file_obj_free(in, true);
        return -1;
    }

    /* Create stderr stream */
    file_obj_t *err = file_obj_alloc(&terminal_tty_fops, OPEN_WRITE, true);
    if (err == NULL) {
        file_obj_free(in, true);
        file_obj_free(out, true);
        return -1;
    }

    /* Bind to file descriptors */
    file_desc_bind(files, 0, in);
    file_desc_bind(files, 1, out);
    file_desc_bind(files, 2, err);
    return 0;
}

/*
 * tcsetpgrp() compatibility function for code running
 * inside the kernel during early kernel boot, when
 * there is no executing process yet.
 */
void
terminal_tcsetpgrp_impl(int terminal, int pgrp)
{
    terminal_state_t *term = get_terminal(terminal);
    term->fg_group = pgrp;
    scheduler_wake_all(&term->kbd_input.sleep_queue);
}

/*
 * tcgetpgrp() syscall handler. Returns the foreground
 * process group of the terminal that this process is
 * executing in.
 */
__cdecl int
terminal_tcgetpgrp(void)
{
    terminal_state_t *term = get_executing_terminal();
    return term->fg_group;
}

/*
 * tcsetpgrp() syscall handler. Sets the foreground
 * process group of the terminal that this process is
 * executing in.
 */
__cdecl int
terminal_tcsetpgrp(int pgrp)
{
    if (pgrp < 0) {
        debugf("Invalid pgrp: %d\n", pgrp);
        return -1;
    } else if (pgrp == 0) {
        pgrp = get_executing_pcb()->group;
    }

    terminal_state_t *term = get_executing_terminal();
    term->fg_group = pgrp;
    scheduler_wake_all(&term->kbd_input.sleep_queue);
    return 0;
}

/*
 * Initialize all terminals. This must be called before
 * any printing functions!
 */
void
terminal_init(void)
{
    int i;
    for (i = 0; i < NUM_TERMINALS; ++i) {
        terminal_state_t *term = &terminal_states[i];

        /* Point backing memory to the per-terminal page */
        term->backing_mem = (uint8_t *)TERMINAL_PAGE_START + i * KB(4);

        /* Active memory points to the backing memory */
        term->video_mem = term->backing_mem;

        /* Set terminal text and background color */
        term->attrib = ATTRIB;

        /* Initialize the terminal memory region */
        terminal_clear_region(term->backing_mem, VIDEO_MEM_SIZE / 2, term->attrib);

        /* Initialize other fields */
        term->vidmap = false;
        term->fg_group = -1;
        term->kbd_input.count = 0;
        term->mouse_input.count = 0;
        list_init(&term->kbd_input.sleep_queue);
        list_init(&term->mouse_input.sleep_queue);
    }

    /* First terminal's active video memory points to global VGA memory */
    terminal_states[display_terminal].video_mem = VIDEO_MEM;

    /* Register mouse file ops (stdin/stdout handled specially) */
    file_register_type(FILE_TYPE_MOUSE, &terminal_mouse_fops);

    /* Register tty file type so programs can recover stdin/stdout */
    file_register_type(FILE_TYPE_TTY, &terminal_tty_fops);
}
