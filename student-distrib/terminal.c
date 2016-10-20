#include "terminal.h"
#include "types.h"
#include "lib.h"
#include "debug.h"
#include "keyboard.h"

typedef struct {
    /* Buffer to hold the characters */
    uint8_t buf[TERMINAL_BUF_SIZE];

    /* Number of characters in the buffer */
    int32_t count;
} input_buf_t;

static input_buf_t input_buffers[NUM_TERMINALS];

static input_buf_t *
get_current_input_buf(void)
{
    /* Change when we support multiple terminals */
    return &input_buffers[0];
}

static int32_t
terminal_read(uint8_t *dest, int32_t count)
{
    /* Note that we don't append a NUL terminator to
     * the output buffer! */

    int32_t i;
    input_buf_t *input_buf = get_current_input_buf();

    /* Only allow reads up to the end of the buffer */
    if (count > input_buf->count) {
        count = input_buf->count;
    }

    /* TODO: Block and wait for more input if there's no \n yet
     * and the number of characters in the buffer is less than count */

    /*
     * Copy characters from the input buffer, until
     * one of these conditions are met:
     *
     * 1. We reach a \n character
     * 2. We drained all the characters in the buffer (without a \n)
     * 3. We copied the maximum number of characters allowed
     */
    for (i = 0; i < count; ++i) {
        /* Copy from input buffer to dest buffer.
         * Stop after we hit a newline */
        if ((*dest++ = input_buf->buf[i]) == '\n') {
            break;
        }
    }

    /* Shift remaining characters to the front of the buffer */
    memmove(&input_buf[0], &input_buf[i], i);
    input_buf->count -= i;

    /* i holds the number of characters read */
    return i;
}

static int32_t
terminal_write(const uint8_t *src, int32_t count)
{
    /* TODO: Wat do? */
    return 0;
}

/* Handles a keyboard control sequence */
static void
handle_ctrl_input(kbd_input_ctrl_t ctrl)
{
    switch (ctrl) {
    case KCTL_CLEAR:
        clear();
        break;
    default:
        debugf("Invalid control sequence: %d\n", ctrl);
        break;
    }
}

static void
handle_char_input(uint8_t c)
{
    input_buf_t *input_buf = get_current_input_buf();
    if (input_buf->count == TERMINAL_BUF_SIZE) {
        debugf("Input buffer is full, ignoring\n");
        return;
    }

    /* TODO: synchronization */
    input_buf->buf[input_buf->count++] = c;
    putc(c);
}

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
