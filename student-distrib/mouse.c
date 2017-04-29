#include "mouse.h"
#include "ps2.h"
#include "lib.h"
#include "process.h"
#include "debug.h"

/*
 * Number of mouse buffers to allocate.
 * Right now we just have one per process.
 * IDK why they would open any more than that.
 * Obviously, this should be no more than the number
 * of processes * number of files per process.
 */
#define NUM_MOUSE_BUFFERS (MAX_PROCESSES)

/* Holds mouse input data */
static mouse_input_buf_t mouse_input[NUM_MOUSE_BUFFERS];

/*
 * Open syscall for the mouse. Allocates an input
 * buffer for this file.
 */
int32_t
mouse_open(const uint8_t *filename, file_obj_t *file)
{
    int32_t i;
    for (i = 0; i < NUM_MOUSE_BUFFERS; ++i) {
        if (mouse_input[i].count < 0) {
            mouse_input[i].count = 0;
            file->offset = i;
            return 0;
        }
    }

    /* Not enough input buffers available :-( */
    return -1;
}

/*
 * Read syscall for the mouse. This does NOT block if no
 * inputs are available. Copies at most nbytes / sizeof(mouse_input_t)
 * input events to buf. If no events are available, this
 * simply returns 0.
 */
int32_t
mouse_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    /* Find the buffer corresponding to this file */
    mouse_input_buf_t *input_buf = &mouse_input[file->offset];

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
    if (!copy_to_user(buf, input_buf->buf, num_bytes_copy)) {
        return -1;
    }

    /* Shift remaining inputs up */
    memmove(&input_buf->buf[0], (void *)&input_buf->buf[num_copy], num_bytes_copy);
    input_buf->count -= num_copy;

    /* Return the number of bytes copied into the buffer */
    return num_bytes_copy;
}

/*
 * Write syscall for the mouse. Always fails.
 */
int32_t
mouse_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return -1;
}

/*
 * Close syscall for the mouse. Frees the input buffer
 * corresponding to this file.
 */
int32_t
mouse_close(file_obj_t *file)
{
    mouse_input[file->offset].count = 0;
    return 0;
}

/* Handles mouse interrupts. */
void
mouse_handle_irq(void)
{
    /* Read packet data */
    uint8_t flags = ps2_read_data();
    uint8_t dx = ps2_read_data();
    uint8_t dy = ps2_read_data();

    /* Deliver to available input buffers */
    int32_t i;
    for (i = 0; i < NUM_MOUSE_BUFFERS; ++i) {
        mouse_input_buf_t *buf = &mouse_input[i];
        if (buf->count == MOUSE_BUF_SIZE) {
            debugf("Mouse buffer full, dropping packet\n");
        } else if (buf->count >= 0) {
            buf->buf[buf->count].flags = flags;
            buf->buf[buf->count].dx = dx;
            buf->buf[buf->count].dy = dy;
            buf->count++;
        }
    }
}

/* Initializes the mouse. */
void
mouse_init(void)
{
    /* Initialize input buffer */
    int32_t i;
    for (i = 0; i < NUM_MOUSE_BUFFERS; ++i) {
        mouse_input[i].count = -1;
    }

    /* Enable PS/2 port */
    ps2_write_command(PS2_CMD_ENABLE_MOUSE);

    /* Read config byte */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    uint8_t config_byte = ps2_read_data();

    /* Enable mouse interrupts */
    config_byte |= 0x02;

    /* Write config byte */
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config_byte);

    /* Enable mouse packet streaming */
    ps2_write_mouse(PS2_MOUSE_ENABLE);
}
