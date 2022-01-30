#include "terminal.h"
#include "types.h"
#include "debug.h"
#include "string.h"
#include "math.h"
#include "process.h"
#include "paging.h"
#include "signal.h"
#include "wait.h"
#include "vga.h"
#include "myalloc.h"
#include "poll.h"

/*
 * Executing terminal: the terminal corresponding to the currently
 * executing process.
 *
 * Display terminal: the terminal selected by the user using the
 * ALT-F* keys. If the framebuffer is not active, this is also the
 * foreground terminal.
 *
 * Foreground terminal: the terminal that is mapped to video memory.
 * When the framebuffer is active, it is the foreground terminal.
 * Otherwise, the display terminal is the foreground terminal.
 */

/* EOT (CTRL-D) character */
#define EOT '\x04'

/* White text on black background */
#define ATTRIB 0x07

/* White text on blue background */
#define ATTRIB_BSOD 0x1F

/* Backing "video memory" when a terminal is in the background */
__aligned(KB(4))
static uint8_t terminal_bg_mem[NUM_TERMINALS][KB(4)];

/* Holds information about each terminal */
static terminal_t terminal_states[NUM_TERMINALS];

/*
 * Holds the index of the terminal that the user selected. May
 * or may not be in the foreground, depending on whether the
 * VBE framebuffer is currently active.
 */
static int display_terminal = 0;

/*
 * If >= 0, holds the terminal in which the VBE framebuffer is
 * currently active. Otherwise, the VGA card is in text mode.
 */
static int fb_terminal = -1;

/*
 * Returns a terminal given its index number.
 */
static terminal_t *
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
static terminal_t *
get_executing_terminal(void)
{
    pcb_t *pcb = get_executing_pcb();
    assert(pcb != NULL);
    return get_terminal(pcb->terminal);
}

/*
 * Returns the display terminal. This is the terminal that
 * kernel output should be sent to.
 */
static terminal_t *
get_display_terminal(void)
{
    return get_terminal(display_terminal);
}

/*
 * Returns the foreground terminal. This is the terminal that
 * user input should be sent to.
 */
static terminal_t *
get_foreground_terminal(void)
{
    if (fb_terminal >= 0) {
        return get_terminal(fb_terminal);
    } else {
        return get_terminal(display_terminal);
    }
}

/*
 * Sets the global text mode cursor position from the given
 * terminal, if it is the display terminal and the framebuffer
 * is not active.
 */
static void
terminal_update_cursor(terminal_t *term)
{
    if (fb_terminal < 0 && term == get_display_terminal()) {
        vga_set_cursor_location(term->cursor.screen_x, term->cursor.screen_y);
    }
}

/*
 * Updates the vidmap page for the executing process to point
 * to the correct location. Takes the process terminal and vidmap
 * status as inputs to prevent a circular dependency with process.h.
 */
void
terminal_update_vidmap_page(int terminal_idx, bool vidmap)
{
    terminal_t *term = get_terminal(terminal_idx);
    paging_update_vidmap_page((uintptr_t)term->active_mem, vidmap);
}

/*
 * Updates the vidmap page for the executing process to point
 * to the correct location.
 */
static void
terminal_update_executing_vidmap_page(void)
{
    pcb_t *pcb = get_executing_pcb();
    terminal_update_vidmap_page(pcb->terminal, pcb->vidmap);
}

/*
 * Copies the text-mode contents of the terminal to the
 * background buffer and points the active memory to it.
 */
static void
terminal_enter_background(terminal_t *term)
{
    assert(term->active_mem == (uint8_t *)VGA_TEXT_PAGE_START);
    memcpy(term->bg_mem, (uint8_t *)VGA_TEXT_PAGE_START, VGA_TEXT_SIZE);
    term->active_mem = term->bg_mem;
}

/*
 * Copies the contents of the terminal from the background
 * buffer into video memory and points the active memory to it.
 */
static void
terminal_enter_foreground(terminal_t *term)
{
    assert(term->active_mem == term->bg_mem);
    memcpy((uint8_t *)VGA_TEXT_PAGE_START, term->bg_mem, VGA_TEXT_SIZE);
    term->active_mem = (uint8_t *)VGA_TEXT_PAGE_START;
}

/*
 * Sets the display terminal index. Swaps the video memory
 * and updates the text mode cursor location, if the framebuffer
 * is not currently active.
 */
void
terminal_set_display(int index)
{
    assert(index >= 0 && index < NUM_TERMINALS);

    int old_index = display_terminal;
    if (index == old_index) {
        return;
    }

    display_terminal = index;

    /*
     * If framebuffer is currently active, defer rest
     * of the logic to terminal_reset_framebuffer().
     */
    if (fb_terminal >= 0) {
        return;
    }

    terminal_t *old = get_terminal(old_index);
    terminal_enter_background(old);

    terminal_t *new = get_terminal(index);
    terminal_enter_foreground(new);
    terminal_update_cursor(new);
    terminal_update_executing_vidmap_page();
}

/*
 * Enables framebuffer mode in the given terminal. This makes
 * it the foreground terminal and globally locks it in place
 * until terminal_reset_framebuffer() is called. Must be called
 * before VBE mode is enabled.
 */
void
terminal_set_framebuffer(int index)
{
    assert(index >= 0 && index < NUM_TERMINALS);
    assert(fb_terminal < 0);

    terminal_t *term = get_display_terminal();
    terminal_enter_background(term);
    terminal_update_executing_vidmap_page();

    fb_terminal = index;
}

/*
 * Disables framebuffer mode. Must be called after text mode
 * is restored.
 */
void
terminal_reset_framebuffer(void)
{
    if (fb_terminal < 0) {
        return;
    }

    fb_terminal = -1;

    terminal_t *term = get_display_terminal();
    terminal_enter_foreground(term);
    terminal_update_cursor(term);
    terminal_update_executing_vidmap_page();
}

/*
 * Writes a character at the current cursor position.
 */
static void
terminal_write_char(terminal_t *term, char c)
{
    cursor_pos_t *cur = &term->cursor;
    vga_write_char(term->active_mem, cur->screen_x, cur->screen_y, c);
}

/*
 * Prints a character to the specified terminal.
 * This does NOT update the cursor position!
 */
static void
terminal_putc_impl(terminal_t *term, char c)
{
    cursor_pos_t *cur = &term->cursor;

    if (c == '\n') {
        /* Reset x position, increment y position */
        cur->logical_x = 0;
        cur->screen_x = 0;
        cur->screen_y++;

        /* Scroll if we're at the bottom */
        if (cur->screen_y >= VGA_TEXT_ROWS) {
            vga_scroll_down(term->active_mem, term->attrib);
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
                cur->screen_x += VGA_TEXT_COLS;
            }

            /* Clear the character under the cursor */
            terminal_write_char(term, '\x00');
        }
    } else {
        /* Write the character to screen */
        terminal_write_char(term, c);

        /* Move the cursor rightwards, with text wrapping */
        cur->logical_x++;
        cur->screen_x++;
        if (cur->screen_x >= VGA_TEXT_COLS) {
            cur->screen_y++;
            cur->screen_x -= VGA_TEXT_COLS;
        }

        /* Scroll if we wrapped some text at the bottom */
        if (cur->screen_y >= VGA_TEXT_ROWS) {
            vga_scroll_down(term->active_mem, term->attrib);
            cur->screen_y--;
        }
    }
}

/*
 * Writes a buffer of characters to the display terminal.
 */
void
terminal_write_chars(const char *buf, int len)
{
    terminal_t *term = get_display_terminal();
    while (len--) {
        terminal_putc_impl(term, *buf++);
    }
    terminal_update_cursor(term);
}

/*
 * Clears the specified terminal and resets the cursor
 * position. This does NOT clear the input buffer.
 */
static void
terminal_clear_screen(terminal_t *term)
{
    vga_clear_screen(term->active_mem, term->attrib);

    /* Reset cursor to top-left position */
    term->cursor.logical_x = 0;
    term->cursor.screen_x = 0;
    term->cursor.screen_y = 0;
    terminal_update_cursor(term);
}

/*
 * Clears the display terminal and puts it into a BSOD state.
 */
void
terminal_clear_bsod(void)
{
    terminal_t *term = get_display_terminal();
    term->attrib = ATTRIB_BSOD;
    terminal_clear_screen(term);
}

/*
 * Clears the display terminal screen and all associated input.
 */
void
terminal_clear(void)
{
    terminal_t *term = get_display_terminal();
    terminal_clear_screen(term);
    term->kbd_input.count = 0;
    term->mouse_input.count = 0;
}

/*
 * Checks if the keyboard input buffer has enough data to be
 * read. Returns the number of characters that should be read,
 * or -EAGAIN if there is currently nothing to read.
 */
static int
terminal_tty_get_readable_bytes(terminal_t *term, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

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
    terminal_t *term = get_executing_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;

    /* Wait until there's a newline/EOT in the buffer */
    nbytes = WAIT_INTERRUPTIBLE(
        terminal_tty_get_readable_bytes(term, nbytes),
        &input_buf->sleep_queue,
        file->nonblocking);
    if (nbytes <= 0) {
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
    terminal_t *term = get_executing_terminal();
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
 * poll() syscall handler for stdin/stdout. Sets the read bit
 * if there is a full (\n-terminated) line in the keyboard buffer
 * to read. The write bit is always set.
 */
static int
terminal_tty_poll(file_obj_t *file, wait_node_t *readq, wait_node_t *writeq)
{
    int revents = 0;
    terminal_t *term = get_executing_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;

    revents |= POLL_READ(
        terminal_tty_get_readable_bytes(term, INT_MAX),
        &input_buf->sleep_queue,
        readq);

    revents |= OPEN_WRITE;

    return revents;
}

/*
 * Returns the number of readable bytes in the mouse input
 * buffer, or -EAGAIN if the buffer is empty.
 */
static int
terminal_mouse_get_readable_bytes(terminal_t *term, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    pcb_t *pcb = get_executing_pcb();
    mouse_input_buf_t *input_buf = &term->mouse_input;

    /* Check that caller is in the foreground group */
    if (term->fg_group != pcb->group) {
        debugf("Attempting to read mouse from background group (fg=%d, curr=%d)\n",
            term->fg_group, pcb->group);
        return -1;
    }

    if (input_buf->count == 0) {
        return -EAGAIN;
    }

    /*
     * If previous read was partial, next read shall return only
     * the remainder of the partially read input. This is to allow
     * callers to "align" their read offsets. In other words, if
     * read() ever returns 1 or 2, either the buffer is too small
     * or the previous read was partial.
     */
    int max_read = input_buf->count;
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
    return min(nbytes, max_read);
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
    terminal_t *term = get_executing_terminal();
    mouse_input_buf_t *input_buf = &term->mouse_input;

    /* Wait until we have any events to read */
    nbytes = WAIT_INTERRUPTIBLE(
        terminal_mouse_get_readable_bytes(term, nbytes),
        &input_buf->sleep_queue,
        file->nonblocking);
    if (nbytes <= 0) {
        return nbytes;
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
 * poll() syscall handler for the mouse. Sets the write bit if
 * there are any events to read.
 */
static int
terminal_mouse_poll(file_obj_t *file, wait_node_t *readq, wait_node_t *writeq)
{
    int revents = 0;
    terminal_t *term = get_executing_terminal();
    mouse_input_buf_t *input_buf = &term->mouse_input;

    revents |= POLL_READ(
        terminal_mouse_get_readable_bytes(term, INT_MAX),
        &input_buf->sleep_queue,
        readq);

    return revents;
}

/*
 * Handles CTRL-C input by sending an interrupt signal
 * to the foreground process group in the displayed terminal.
 */
static void
terminal_interrupt(void)
{
    terminal_t *term = get_foreground_terminal();
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
    terminal_t *term = get_foreground_terminal();
    kbd_input_buf_t *input_buf = &term->kbd_input;
    if (input_buf->count < array_len(input_buf->buf)) {
        input_buf->buf[input_buf->count++] = EOT;
        wait_queue_wake(&input_buf->sleep_queue);
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
    case KCTL_PANIC:
        panic("User-triggered panic\n");
        break;
    case KCTL_MEMDUMP:
        mya_dump_state();
        break;
    case KCTL_TERM1:
    case KCTL_TERM2:
    case KCTL_TERM3:
        terminal_set_display(ctrl - KCTL_TERM1);
        break;
    default:
        panic("Unknown control code\n");
        break;
    }
}

/* Handles single-character keyboard input */
static void
handle_char_input(char c)
{
    terminal_t *term = get_foreground_terminal();
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
    wait_queue_wake(&input_buf->sleep_queue);
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
        panic("Unknown keyboard input type\n");
        break;
    }
}

/* Handles input from the mouse */
void
terminal_handle_mouse_input(mouse_input_t input)
{
    terminal_t *term = get_foreground_terminal();
    mouse_input_buf_t *input_buf = &term->mouse_input;

    /*
     * Only copy the input into the buffer if the entire
     * event will fit. Otherwise, just discard it.
     */
    if (input_buf->count + sizeof(input) <= sizeof(input_buf->buf)) {
        input_buf->buf[input_buf->count++] = input.flags;
        input_buf->buf[input_buf->count++] = input.dx;
        input_buf->buf[input_buf->count++] = input.dy;
        wait_queue_wake(&input_buf->sleep_queue);
    }
}

/* Combined file ops for the stdin/stdout streams */
static const file_ops_t terminal_tty_fops = {
    .read = terminal_tty_read,
    .write = terminal_tty_write,
    .poll = terminal_tty_poll,
};

/* Mouse file ops */
static const file_ops_t terminal_mouse_fops = {
    .read = terminal_mouse_read,
    .poll = terminal_mouse_poll,
};

/*
 * Opens stdin, stdout, and stderr as fds 0, 1, and 2
 * respectively for a given process.
 */
int
terminal_open_streams(file_obj_t **files)
{
    int ret;
    file_obj_t *in = NULL;
    file_obj_t *out = NULL;
    file_obj_t *err = NULL;

    /* Create stdin stream */
    in = file_obj_alloc(&terminal_tty_fops, OPEN_READ);
    if (in == NULL) {
        ret = -1;
        goto exit;
    }

    /* Create stdout stream */
    out = file_obj_alloc(&terminal_tty_fops, OPEN_WRITE);
    if (out == NULL) {
        ret = -1;
        goto exit;
    }

    /* Create stderr stream */
    err = file_obj_alloc(&terminal_tty_fops, OPEN_WRITE);
    if (err == NULL) {
        ret = -1;
        goto exit;
    }

    /* Bind to file descriptors */
    file_desc_bind(files, 0, in);
    file_desc_bind(files, 1, out);
    file_desc_bind(files, 2, err);
    ret = 0;

exit:
    if (err != NULL) {
        file_obj_release(err);
    }
    if (out != NULL) {
        file_obj_release(out);
    }
    if (in != NULL) {
        file_obj_release(in);
    }
    return ret;
}

/*
 * tcsetpgrp() compatibility function for code running
 * inside the kernel during early kernel boot, when
 * there is no executing process yet.
 */
void
terminal_tcsetpgrp_impl(int terminal, int pgrp)
{
    terminal_t *term = get_terminal(terminal);
    term->fg_group = pgrp;
}

/*
 * tcgetpgrp() syscall handler. Returns the foreground
 * process group of the terminal that this process is
 * executing in.
 */
__cdecl int
terminal_tcgetpgrp(void)
{
    terminal_t *term = get_executing_terminal();
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

    terminal_t *term = get_executing_terminal();
    term->fg_group = pgrp;
    return 0;
}

/*
 * vidmap() syscall handler. Enables the vidmap page and
 * copies its address to screen_start.
 */
__cdecl int
terminal_vidmap(uint8_t **screen_start)
{
    pcb_t *pcb = get_executing_pcb();

    uint8_t *addr = (uint8_t *)VIDMAP_PAGE_START;
    if (!copy_to_user(screen_start, &addr, sizeof(addr))) {
        return -1;
    }

    pcb->vidmap = true;
    terminal_update_vidmap_page(pcb->terminal, pcb->vidmap);
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
        terminal_t *term = &terminal_states[i];
        term->bg_mem = terminal_bg_mem[i];
        term->attrib = ATTRIB;
        term->fg_group = -1;
        term->kbd_input.count = 0;
        term->mouse_input.count = 0;
        list_init(&term->kbd_input.sleep_queue);
        list_init(&term->mouse_input.sleep_queue);

        if (i == display_terminal) {
            term->active_mem = (uint8_t *)VGA_TEXT_PAGE_START;
        } else {
            term->active_mem = term->bg_mem;
            vga_clear_screen(term->active_mem, term->attrib);
        }
    }

    /* Register mouse file ops (stdin/stdout handled specially) */
    file_register_type(FILE_TYPE_MOUSE, &terminal_mouse_fops);

    /* Register tty file type so programs can recover stdin/stdout */
    file_register_type(FILE_TYPE_TTY, &terminal_tty_fops);
}
