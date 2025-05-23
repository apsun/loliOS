#include "keyboard.h"
#include "types.h"
#include "debug.h"
#include "terminal.h"
#include "ps2.h"

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
#define KC_D         0x20
#define KC_L         0x26
#define KC_P         0x19
#define KC_M         0x32
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

/* Current pressed/toggled modifier key state */
static kbd_modifiers_t kbd_modifiers = KMOD_NONE;

/*
 * Maps keycode values to printable characters. Data from:
 * http://www.comptechdoc.org/os/linux/howlinuxworks/linux_hlkeycodes.html
 */
static const char keycode_map[4][NUM_KEYS] = {
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
set_modifier_bit(int bit, kbd_modifiers_t mask)
{
    if (bit) {
        kbd_modifiers |= mask;
    } else {
        kbd_modifiers &= ~mask;
    }
}

/* Toggles a keyboard modifier bit */
static void
toggle_modifier_bit(kbd_modifiers_t mask)
{
    kbd_modifiers ^= mask;
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
    kbd_modifiers_t mod = kbd_modifiers;
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
        case KC_C: /* CTRL-C */
            return KCTL_INTERRUPT;
        case KC_D: /* CTRL-D */
            return KCTL_EOF;
        case KC_P: /* CTRL-P */
            return KCTL_PANIC;
        case KC_M: /* CTRL-M */
            return KCTL_MEMDUMP;
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
 * '\n' and '\b' are considered "printable characters".
 */
static char
keycode_to_char(uint8_t keycode)
{
    /* Check if the keycode was out of range */
    if (keycode >= NUM_KEYS) {
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
    input.type = KTYP_NONE;
    input.character = '\0';

    /* Check if it's a known control sequence */
    kbd_input_ctrl_t ctrl = keycode_to_ctrl(keycode);
    if (ctrl != KCTL_NONE) {
        input.type = KTYP_CTRL;
        input.control = ctrl;
        return input;
    }

    /* Check if it's a printable character */
    char c = keycode_to_char(keycode);
    if (c != '\0') {
        input.type = KTYP_CHAR;
        input.character = c;
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
    /*
     * Most significant bit is 1 if the key was released, 0 if pressed.
     * Remaining 7 bits represent the keycode of the character.
     */
    int status = !(packet & 0x80);
    uint8_t keycode = packet & 0x7F;

    kbd_input_t input;
    input.type = KTYP_NONE;

    /* Try to handle as a modifier key */
    kbd_modifiers_t mod = keycode_to_modifier(keycode);
    if (mod != KMOD_NONE) {
        /* Key pressed was a modifier */
        if (mod == KMOD_CAPS) {
            if (status == 1) {
                toggle_modifier_bit(mod);
            }
        } else {
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

/* Handles keyboard interrupts. */
void
keyboard_handle_irq(void)
{
    /* Read keycode packet */
    int packet = ps2_read_data_nonblocking();
    if (packet < 0) {
        debugf("Got keyboard IRQ but no data to read\n");
        return;
    }

    /* Process packet, updating internal state if necessary */
    kbd_input_t input = process_packet(packet);

    /* Send it to the terminal for processing */
    terminal_handle_kbd_input(input);
}

/* Initializes the keyboard. */
void
keyboard_init(void)
{
    /* Enable PS/2 port on controller */
    ps2_write_command(PS2_CMD_ENABLE_KEYBOARD);

    /* Enable interrupts on controller */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    uint8_t config_byte = ps2_read_data_blocking();
    config_byte |= 0x01;
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config_byte);

    /*
     * Spamming keys at startup seems to put the keyboard into
     * a weird state, so reset it just in case.
     */
    ps2_write_data(PS2_KEYBOARD_RESET);
    ps2_wait_ack();
}
