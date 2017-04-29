#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"

/* Various special keycodes */
#define KC_ESC       0x01
#define KC_LCTRL     0x1D
#define KC_RCTRL     0x61
#define KC_LSHIFT    0x2A
#define KC_RSHIFT    0x36
#define KC_LALT      0x38
#define KC_RALT      0x64
#define KC_CAPS_LOCK 0x3A
#define KC_C         0x2E
#define KC_L         0x26
#define KC_F1        0x3B
#define KC_F2        0x3C
#define KC_F3        0x3D
#define KC_BACKSPACE 0x0E
#define KC_DELETE    0x53
#define KC_TAB       0x0F
#define KC_1         0x02
#define KC_2         0x03
#define KC_3         0x04
#define KC_4         0x05
#define KC_5         0x06

/* Number of keys we handle */
#define NUM_KEYS 58

/* Size of the keyboard buffer */
#define KEYBOARD_BUF_SIZE 128

#ifndef ASM

/* Modifier key enum */
typedef enum {
    KMOD_NONE   = 0,
    KMOD_LCTRL  = 1 << 0,
    KMOD_RCTRL  = 1 << 1,
    KMOD_LSHIFT = 1 << 2,
    KMOD_RSHIFT = 1 << 3,
    KMOD_LALT   = 1 << 4,
    KMOD_RALT   = 1 << 5,
    KMOD_CAPS   = 1 << 6,
    KMOD_CTRL   = KMOD_LCTRL | KMOD_RCTRL,
    KMOD_SHIFT  = KMOD_LSHIFT | KMOD_RSHIFT,
    KMOD_ALT    = KMOD_LALT | KMOD_RALT,
} kbd_modifiers_t;

/* Keyboard input type */
typedef enum {
    KTYP_NONE, /* Invalid input */
    KTYP_CHAR, /* Printable character */
    KTYP_CTRL, /* Control sequence */
} kbd_input_type_t;

/* Keyboard control sequences */
typedef enum {
    KCTL_NONE,      /* Invalid control sequence */
    KCTL_CLEAR,     /* Clear the current terminal */
    KCTL_INTERRUPT, /* Send interrupt signal */
    KCTL_TERM1,     /* Switch to terminal 1 */
    KCTL_TERM2,     /* Switch to terminal 2 */
    KCTL_TERM3,     /* Switch to terminal 3 */
} kbd_input_ctrl_t;

/* Keyboard input struct */
typedef struct {
    kbd_input_type_t type;
    union {
        uint8_t character;
        kbd_input_ctrl_t control;
    } value;
} kbd_input_t;

/* Character input buffer */
typedef struct {
    /* Buffer to hold the characters */
    volatile uint8_t buf[KEYBOARD_BUF_SIZE];

    /* Number of characters in the buffer */
    volatile int32_t count;
} kbd_input_buf_t;

/* Handles keyboard interrupts */
void keyboard_handle_irq(void);

/* Initializes the keyboard */
void keyboard_init(void);

#endif /* ASM */

#endif /* _KEYBOARD_H */
