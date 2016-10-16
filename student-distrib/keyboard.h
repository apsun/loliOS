#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "i8259.h"
#include "types.h"
#include "lib.h"

#define KC_ESC       0x01
#define KC_LCTRL     0x1D
#define KC_RCTRL     0x61
#define KC_LSHIFT    0x2A
#define KC_RSHIFT    0x36
#define KC_LALT      0x38
#define KC_RALT      0x64
#define KC_CAPS_LOCK 0x3A

/* Memory port of the keyboard */
#define KEYBOARD_PORT 0x60

/* Number of keys we handle */
#define NUM_KEYS 58

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

/* Initializes keyboard interrupts */
void keyboard_init(void);

/* Handles a keyboard interrupt */
void keyboard_handle_interrupt(void);

#endif /* ASM */

#endif /* _KEYBOARD_H */
