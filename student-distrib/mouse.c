#include "mouse.h"
#include "ps2.h"
#include "lib.h"
#include "process.h"

/*
 * Number of mouse buffers to allocate.
 * Right now we just have one per process.
 * IDK why they would open any more than that.
 * Obviously, this should be no more than the number
 * of processes * number of files per process.
 */
#define NUM_MOUSE_BUFFERS (MAX_PROCESSES)

/* Holds mouse input data */
mouse_input_buf_t mouse_input[NUM_MOUSE_BUFFERS];

/*
 * Open syscall for the mouse.
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
 * Read syscall for the mouse.
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
 * Write syscall for the mouse.
 */
int32_t
mouse_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return -1;
}

/*
 * Close syscall for the mouse.
 */
int32_t
mouse_close(file_obj_t *file)
{
    mouse_input[file->offset].count = 0;
    return 0;
}

/* Handles mouse interrupts */
void
mouse_handle_irq(uint8_t data[3])
{
    int32_t i;
    for (i = 0; i < NUM_MOUSE_BUFFERS; ++i) {
        mouse_input_buf_t *buf = &mouse_input[i];
        if (buf->count >= 0) {
            buf->buf[buf->count].flags = data[0];
            buf->buf[buf->count].dx = data[1];
            buf->buf[buf->count].dy = data[2];
            buf->count++;
        }
    }

    return;

    uint8_t flags = data[0];

    /* Discard packet if overflow */
    if ((flags & (MOUSE_X_OVERFLOW | MOUSE_Y_OVERFLOW)) != 0) {
        return;
    }

    /* Delta x movement */
    int32_t dx = data[1];
    if ((flags & MOUSE_X_SIGN) != 0) {
        dx |= 0xffffff00;
    }

    /* Delta y movement */
    int32_t dy = data[2];
    if ((flags & MOUSE_Y_SIGN) != 0) {
        dy |= 0xffffff00;
    }

    /* Buttons */
    bool left_down = !!(flags & MOUSE_LEFT);
    bool right_down = !!(flags & MOUSE_RIGHT);
    bool middle_down = !!(flags & MOUSE_MIDDLE);

    printf("delta_x: %d\n", dx);
    printf("left: %d\n", left_down);
    printf("right: %d\n", right_down);
    printf("middle: %d\n", middle_down);
}
