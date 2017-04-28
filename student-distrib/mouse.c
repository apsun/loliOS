#include "mouse.h"
#include "ps2.h"
#include "lib.h"
#include "process.h"

#define MAX_MOUSE_FILES (MAX_PROCESSES * MAX_FILES)
static file_obj_t *mouse_files[MAX_MOUSE_FILES];

/*
 * Open syscall for the mouse.
 */
int32_t
mouse_open(const uint8_t *filename, file_obj_t *file)
{
    int32_t i;
    for (i = 0; i < MAX_MOUSE_FILES; ++i) {
        if (mouse_files[i] == NULL) {
            mouse_files[i] = file;
            return 0;
        }
    }

    /* Not enough slots in the file tracker :-( */
    return -1;
}

/*
 * Read syscall for the mouse.
 */
int32_t
mouse_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    /* Must read the whole mouse data struct */
    if (nbytes < sizeof(mouse_data_t)) {
        return -1;
    }

    if (copy_to_user(buf, file-))
    return 0;
    
}

/*
 * Write syscall for the mouse.
 */
int32_t
mouse_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return 0;
}

/*
 * Close syscall for the mouse.
 */
int32_t
mouse_close(file_obj_t *file)
{
    return 0;
}

/* Handles mouse interrupts */
void
mouse_handle_irq(uint8_t data[3])
{
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
