/*
 * keyboard input handler
 * "Copyright (c) 2016 by Emre Ulusoy."
 */

#include "keyboard.h"
#include "debug.h"

/* Current pressed/toggled modifier key state */
static kbd_modifiers_t modifiers;

/* Maps keycode values to printable characters */
static char keycode_map[4][NUM_KEYS] = {
    /* Neutral */
    {
        '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        '\0', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '\0', '*',
        '\0', ' ',
    },

    /* Shift */
    {
        '\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
        '\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        '\0', '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '\0', '*',
        '\0', ' ',
    },

    /* Caps */
    {
        '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']',
        '\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
        '\0', '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '\0', '*',
        '\0', ' ',
    },

    /* Shift and caps */
    {
        '\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}',
        '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~',
        '\0', '|', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', '\0', '*',
        '\0', ' ',
    },
};

/* Initializes keyboard interrupts */
void
keyboard_init(void)
{
    /* Enable keyboard IRQ */
    enable_irq(IRQ_KEYBOARD);
}

/* Sets a keyboard modifier bit */
static void
set_modifier_bit(uint8_t bit, kbd_modifiers_t mask)
{
    if (bit) {
        modifiers |= mask;
    } else {
        modifiers &= ~mask;
    }
}

/* Toggles a keyboard modifier bit */
static void
toggle_modifier_bit(kbd_modifiers_t mask) {
    modifiers ^= mask;
}

/*
 * Maps a keycode to a modifier key.
 * Returns KMOD_NONE if the keycode is not a modifier.
 */
static kbd_modifiers_t
keycode_to_modifier(uint8_t keycode)
{
    switch (keycode) {
    case KC_LCTRL:
        return KMOD_LCTRL;
    case KC_RCTRL:
        return KMOD_RCTRL;
    case KC_LSHIFT:
        return KMOD_LSHIFT;
    case KC_RSHIFT:
        return KMOD_RSHIFT;
    case KC_LALT:
        return KMOD_LALT;
    case KC_RALT:
        return KMOD_RALT;
    case KC_CAPS_LOCK:
        return KMOD_CAPS;
    default:
        return KMOD_NONE;
    }
}

/*
 * Maps a keycode to a printable character.
 * Returns '\0' if the key is not printable.
 */
static uint8_t
keycode_to_char(uint8_t keycode)
{
    /* Check if the keycode was out of range */
    if (keycode >= NUM_KEYS) {
        debugf("Unknown keycode: %x\n", keycode);
        return '\0';
    }

    /* Map the keycode to the appropriate character */
    switch ((int)modifiers) {
    case KMOD_NONE:
        return keycode_map[0][keycode];
    case KMOD_LSHIFT:
    case KMOD_RSHIFT:
    case KMOD_SHIFT:
        return keycode_map[1][keycode];
    case KMOD_CAPS:
        return keycode_map[2][keycode];
    case KMOD_CAPS | KMOD_LSHIFT:
    case KMOD_CAPS | KMOD_RSHIFT:
    case KMOD_CAPS | KMOD_SHIFT:
        return keycode_map[3][keycode];
    default:
        debugf("Unhandled modifier combination: %x\n", modifiers);
        return '\0';
    }
}

/*
 * Processes a keyboard packet. Returns the printable character
 * corresponding to the packet (taking into consideration the
 * current modifier state), or '\0' if the key cannot be printed.
 */
static uint8_t
process_packet(uint8_t packet)
{
    /* 1 if key was pressed, 0 if key was released */
    uint8_t status = !(packet & 0x80);
    uint8_t keycode = packet & 0x7F;
    kbd_modifiers_t mod = keycode_to_modifier(keycode);
    if (mod != KMOD_NONE) {
        /* Key pressed was a modifier */
        if (mod == KMOD_CAPS) {
            if (status == 1) {
                debugf("Toggled caps lock\n");
                toggle_modifier_bit(mod);
            }
        } else {
            debugf("Set modifier %x -> %d\n", mod, status);
            set_modifier_bit(status, mod);
        }
        return '\0';
    } else if (status == 1) {
        /* Key pressed, return keystroke */
        return keycode_to_char(keycode);
    } else {
        /* We don't handle anything on key up */
        return '\0';
    }
}

/* Handles keyboard interrupts from the PIC */
void
keyboard_handle_interrupt(void)
{
    /*
     * Most significant bit is 1 if the key was released, 0 if pressed.
     * Remaining 7 bits represent the keycode of the character.
     */
    uint8_t packet = inb(KEYBOARD_PORT);

    /* Process packet, updating internal state if necessary */
    uint8_t print_char = process_packet(packet);

    /* Echo character to screen if it's printable */
    if (print_char != '\0') {
        putc(print_char);
    }

    /* Unmask keyboard interrupts */
    send_eoi(IRQ_KEYBOARD);
}
