#include "terminal.h"
#include "types.h"
#include "lib.h"
#include "debug.h"
#include "keyboard.h"

/* Character input buffer */
typedef struct {
    /* Buffer to hold the characters */
    uint8_t buf[TERMINAL_BUF_SIZE];

    /* Number of characters in the buffer */
    int32_t count;
} input_buf_t;

/* Array of input buffers, one for each terminal */
static volatile input_buf_t input_buffers[NUM_TERMINALS];

/* Index of the currently active terminal */
static int32_t active_terminal = 0;

static int screen_x;
static int screen_y;
static char* video_mem = (char *)VIDEO;

/*
 * Returns the input buffer for the terminal running
 * the current process. Note that THIS IS NOT THE ACTIVE
 * TERMINAL, since there may be processes running in
 * non-active terminals.
 */
static volatile input_buf_t *
get_current_input_buf(void)
{
    /* TODO: Change this */
    return &input_buffers[0];
}

/* Sets the index of the displayed terminal */
static void
set_active_terminal(int32_t index)
{
    ASSERT(index >= 0 && index < NUM_TERMINALS);
    /* active_terminal = index; */
    /* TODO: Swap display buffers */
}

/* Performs scrolling when a new line is reached */
static void
scroll_down(void)
{
    /* TODO */
}

/* Prints a character to the currently active terminal */
void
terminal_putc(uint8_t c)
{
    if(c == '\n' || c == '\r') {
        screen_y = (screen_y + 1) % NUM_ROWS;
        screen_x=0;
    } else if (c == '\b') {
        screen_x = (screen_x - 1 + NUM_COLS) % NUM_COLS;
        *(uint8_t *)(video_mem + ((NUM_COLS*screen_y + screen_x) << 1)) = ' ';
        *(uint8_t *)(video_mem + ((NUM_COLS*screen_y + screen_x) << 1) + 1) = ATTRIB;
    } else if (c == '\t') {
        puts("    ");
    } else {
        *(uint8_t *)(video_mem + ((NUM_COLS*screen_y + screen_x) << 1)) = c;
        *(uint8_t *)(video_mem + ((NUM_COLS*screen_y + screen_x) << 1) + 1) = ATTRIB;
        screen_x++;
        screen_x %= NUM_COLS;
        screen_y = (screen_y + (screen_x / NUM_COLS)) % NUM_ROWS;
    }
}

/*
 * Clears the active terminal and resets the cursor
 * position. This does NOT clear the input buffer.
 */
void
terminal_clear(void)
{
    int32_t i;
    for(i=0; i<NUM_ROWS*NUM_COLS; i++) {
        *(uint8_t *)(video_mem + (i << 1)) = ' ';
        *(uint8_t *)(video_mem + (i << 1) + 1) = ATTRIB;
    }

    /* Reset cursor to top-left position */
    screen_x = 0;
    screen_y = 0;
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
    /* Note that we don't append a NUL terminator to
     * the output buffer! The caller has to handle it! */
    int32_t i;
    uint8_t *dest = (uint8_t *)buf;
    volatile input_buf_t *input_buf = get_current_input_buf();

    /* Only allow reads up to the end of the buffer */
    if (nbytes > TERMINAL_BUF_SIZE) {
        nbytes = TERMINAL_BUF_SIZE;
    }

    /*
     * Copy characters from the input buffer, until
     * one of these conditions are met:
     *
     * 1. We reach a \n character
     * 2. We drained all the characters in the buffer (without a \n)
     * 3. We copied the maximum number of characters allowed
     */
    for (i = 0; i < nbytes; ++i) {
        while (i >= input_buf->count) {
            /* Wait for some more input (nicely, to save CPU cycles).
             * There's no race condition between sti and hlt here,
             * since sti will only take effect after the following
             * instruction (hlt) has been executed.
             */
            sti();
            hlt();
            cli();
        }

        /* Copy from input buffer to dest buffer.
         * Stop after we hit a newline */
        if ((*dest++ = input_buf->buf[i]) == '\n') {
            i++;
            break;
        }
    }

    /* Shift remaining characters to the front of the buffer
     * Interrupts are cleared at this point so no need to worry
     * about discarding the volatile qualifier */
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
    int32_t i;
    const uint8_t *src = (const uint8_t *)buf;
    for (i = 0; i < nbytes; ++i) {
        terminal_putc(src[i]);
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
        set_active_terminal(0);
        break;
    case KCTL_TERM2:
        set_active_terminal(1);
        break;
    case KCTL_TERM3:
        set_active_terminal(2);
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
    volatile input_buf_t *input_buf = get_current_input_buf();
    if (input_buf->count == TERMINAL_BUF_SIZE) {
        debugf("Input buffer is full, ignoring character\n");
        return;
    }

    input_buf->buf[input_buf->count++] = c;
    terminal_putc(c);
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
