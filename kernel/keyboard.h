#ifndef _KEYBOARD_H
#define _KEYBOARD_H

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
    KCTL_EOF,       /* Signal EOF in terminal input */
    KCTL_TERM1,     /* Switch to terminal 1 */
    KCTL_TERM2,     /* Switch to terminal 2 */
    KCTL_TERM3,     /* Switch to terminal 3 */
} kbd_input_ctrl_t;

/* Keyboard input struct */
typedef struct {
    kbd_input_type_t type;
    union {
        char character;
        kbd_input_ctrl_t control;
    };
} kbd_input_t;

/* Handles keyboard interrupts */
void keyboard_handle_irq(void);

/* Initializes the keyboard */
void keyboard_init(void);

#endif /* ASM */

#endif /* _KEYBOARD_H */
