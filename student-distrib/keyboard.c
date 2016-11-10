#include "keyboard.h"
#include "irq.h"
#include "lib.h"
#include "debug.h"
#include "terminal.h"

/* Current pressed/toggled modifier key state */
static kbd_modifiers_t modifiers = KMOD_NONE;

/* Maps keycode values to printable characters. Data from:
 * http://www.comptechdoc.org/os/linux/howlinuxworks/linux_hlkeycodes.html
 */
static char keycode_map[4][NUM_KEYS] = {
    /* Neutral */
    {
        '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\0', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        '\0', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '\0', '*',
        '\0', ' ',
    },

    /* Shift */
    {
        '\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        '\b', '\0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
        '\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        '\0', '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '\0', '*',
        '\0', ' ',
    },

    /* Caps */
    {
        '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']',
        '\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
        '\0', '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '\0', '*',
        '\0', ' ',
    },

    /* Shift and caps */
    {
        '\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        '\b', '\0', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}',
        '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~',
        '\0', '|', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', '\0', '*',
        '\0', ' ',
    },
};

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
toggle_modifier_bit(kbd_modifiers_t mask)
{
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
 * Gets the currently pressed modifier state,
 * with left and right modifiers consolidated
 * (i.e. if either KMOD_LCTRL or KMOD_RCTRL are
 * 1, both bits will be set to 1 so you can just
 * test against KMOD_CTRL).
 */
static int
get_modifiers(void)
{
    kbd_modifiers_t mod = modifiers;
    if (mod & (KMOD_CTRL))  mod |= KMOD_CTRL;
    if (mod & (KMOD_SHIFT)) mod |= KMOD_SHIFT;
    if (mod & (KMOD_ALT))   mod |= KMOD_ALT;
    return (int)mod;
}

/*
 * Maps a keycode to the corresponding control sequence,
 * or KCTL_NONE if it does not correspond to anything.
 * Note that despite the name, this function handles
 * ALT key combinations too.
 */
static kbd_input_ctrl_t
keycode_to_ctrl(uint8_t keycode)
{
    switch (get_modifiers() & ~KMOD_CAPS) {
    case KMOD_CTRL:
        switch (keycode) {
        case KC_L: /* CTRL-L */
            return KCTL_CLEAR;
        }
        break;
    case KMOD_ALT:
        switch (keycode) {
        case KC_F1: /* ALT-F1 */
            return KCTL_TERM1;
        case KC_F2: /* ALT-F2 */
            return KCTL_TERM2;
        case KC_F3: /* ALT-F3 */
            return KCTL_TERM3;
        }
        break;
    }
    return KCTL_NONE;
}

/*
 * Maps a keycode to the corresponding printable character,
 * or '\0' if the character cannot be printed. Note that
 * '\n', '\t', and '\b' are considered "printable characters".
 */
static uint8_t
keycode_to_char(uint8_t keycode)
{
    /* Check if the keycode was out of range */
    if (keycode >= NUM_KEYS) {
        debugf("Unknown keycode: 0x%#x\n", keycode);
        return '\0';
    }

    switch (get_modifiers()) {
    case KMOD_NONE:
        return keycode_map[0][keycode];
    case KMOD_SHIFT:
        return keycode_map[1][keycode];
    case KMOD_CAPS:
        return keycode_map[2][keycode];
    case KMOD_CAPS | KMOD_SHIFT:
        return keycode_map[3][keycode];
    default:
        return '\0';
    }
}

/*
 * Maps a keycode to an input value (taking into consideration
 * currently pressed/toggled modifier keys).
 */
static kbd_input_t
keycode_to_input(uint8_t keycode)
{
    kbd_input_t input;
    kbd_input_ctrl_t ctrl;
    uint8_t c;
    input.type = KTYP_NONE;

    /* Check if it's a known control sequence */
    ctrl = keycode_to_ctrl(keycode);
    if (ctrl != KCTL_NONE) {
        input.type = KTYP_CTRL;
        input.value.control = ctrl;
        return input;
    }

    /* Check if it's a printable character */
    c = keycode_to_char(keycode);
    if (c != '\0') {
        input.type = KTYP_CHAR;
        input.value.character = c;
        return input;
    }

    /* None of the above */
    return input;
}

/*
 * Processes a keyboard packet, updating internal state
 * as necessary.
 *
 * The returned struct has type set to KTYP_CHAR if the
 * keycode and modifier combination corresponds to a printable
 * character, KTYP_CTRL if it corresponds to a control sequence,
 * and KTYP_NONE if it corresponds to neither (and can be ignored).
 */
static kbd_input_t
process_packet(uint8_t packet)
{
    /* Status is 1 if key was pressed, 0 if key was released */
    uint8_t status = !(packet & 0x80);
    uint8_t keycode = packet & 0x7F;
    kbd_modifiers_t mod = keycode_to_modifier(keycode);
    kbd_input_t input;
    input.type = KTYP_NONE;

    if (mod != KMOD_NONE) {
        /* Key pressed was a modifier */
        if (mod == KMOD_CAPS) {
            if (status == 1) {
                debugf("Toggled caps lock\n");
                toggle_modifier_bit(mod);
            }
        } else {
            debugf("Set modifier 0x%#x -> %d\n", mod, status);
            set_modifier_bit(status, mod);
        }
    } else if (status == 1) {
        /* Key pressed, return keystroke */
        input = keycode_to_input(keycode);
    } else {
        /* We don't handle anything on key up */
    }
    return input;
}

/* Handles keyboard interrupts from the PIC */
static void
handle_keyboard_irq(void)
{
    /*
     * Most significant bit is 1 if the key was released, 0 if pressed.
     * Remaining 7 bits represent the keycode of the character.
     */
    uint8_t packet = inb(KEYBOARD_PORT);

    /* Process packet, updating internal state if necessary */
    kbd_input_t input = process_packet(packet);

    /* Send it to the terminal for processing */
    terminal_handle_input(input);
}

/* Initializes keyboard interrupts */
void
keyboard_init(void)
{
    /* Register keyboard IRQ handler, enable interrupts */
    irq_register_handler(IRQ_KEYBOARD, handle_keyboard_irq);
}
