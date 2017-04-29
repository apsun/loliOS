#ifndef _MOUSE_H
#define _MOUSE_H

#include "types.h"

/* Mouse packet flags */
#define MOUSE_LEFT (1 << 0)
#define MOUSE_RIGHT (1 << 1)
#define MOUSE_MIDDLE (1 << 2)
#define MOUSE_X_SIGN (1 << 4)
#define MOUSE_Y_SIGN (1 << 5)
#define MOUSE_X_OVERFLOW (1 << 6)
#define MOUSE_Y_OVERFLOW (1 << 7)

/* Number of inputs in the mouse buffer */
#define MOUSE_BUF_SIZE 64

#ifndef ASM

typedef struct {
    /*
     * Flag bits
     * 0 - left button down?
     * 1 - right button down?
     * 2 - middle button down?
     * 3 - ignored
     * 4 - x sign
     * 5 - y sign
     * 6 - x overflow
     * 7 - y overflow
     */
    uint8_t flags;

    /*
     * Mouse delta x (if x sign bit is 1, then this
     * should be OR'd with 0xFFFFFF00)
     */
    uint8_t dx;

    /*
     * Mouse delta y (if y sign bit is 1, then this
     * should be OR'd with 0xFFFFFF00)
     */
    uint8_t dy;
} mouse_input_t;

/* Mouse file data */
typedef struct {
    /* Mouse input buffer */
    volatile mouse_input_t buf[MOUSE_BUF_SIZE];

    /* Number of valid inputs in the buffer */
    volatile int32_t count;
} mouse_input_buf_t;

/* Handles mouse interrupts */
void mouse_handle_irq(void);

/* Initializes the mouse */
void mouse_init(void);

#endif /* ASM */

#endif /* _MOUSE_H */
