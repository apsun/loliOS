#include "taux.h"
#include "serial.h"
#include "debug.h"
#include "lib.h"
#include "paging.h"

#define DECIMAL_PT (1 << 4)

/* Maps hexadecimal digits to bits on the segment display */
static const uint8_t hex_to_segment_map[16] = {
    0xe7, /* 0 - ABCDEF  */
    0x06, /* 1 - BC      */
    0xcb, /* 2 - ABGED   */
    0x8f, /* 3 - ABGCD   */
    0x2e, /* 4 - FGBC    */
    0xad, /* 5 - AFGCD   */
    0xed, /* 6 - AFEDCG  */
    0x86, /* 7 - ABC     */
    0xef, /* 8 - ABCDEFG */
    0xae, /* 9 - AFGBC   */
    0xee, /* A - AFBGEC  */
    0x6d, /* b - FEGCD   */
    0xe1, /* C - AFED    */
    0x4f, /* d - BGEDC   */
    0xe9, /* E - AFEGD   */
    0xe8, /* F - AFGE    */
};

/* Maps letters to bits on the segment display */
static const uint8_t alpha_to_segment_map[26] = {
    0xee, /* A */
    0x6d, /* b */
    0xe1, /* C */
    0x4f, /* d */
    0xe9, /* E */
    0xe8, /* F */
    0xaf, /* g */
    0x6c, /* h */
    0x60, /* I */
    0x47, /* J */
    0x00, /* K - undisplayable */
    0x61, /* L */
    0x00, /* M - undisplayable */
    0xe6, /* n */
    0xe7, /* O */
    0xea, /* P */
    0xae, /* q */
    0xe2, /* r */
    0xad, /* S */
    0x69, /* t */
    0x67, /* U */
    0x00, /* V - undisplayable */
    0x00, /* W - undisplayable */
    0x00, /* X - undisplayable */
    0x2f, /* y */
    0xcb, /* Z */
};

/* Number of pending ACKs */
static int32_t pending_acks = 0;

/* Holds the current pressed state of the buttons */
static uint8_t button_status = 0;

/* Holds the last converted value sent to the TUX_SET_LED[_STR] ioctl */
static uint8_t led_segments[4];

/* Whether we should send a LED_SET packet when free (no pending ACKs) */
static bool set_led_pending = false;

/*
 * Converts a LED status value (from the TUX_SET_LED
 * ioctl) to the LED segment format used by the taux
 * controller. The buffer must be at 4 bytes long.
 */
static void
taux_convert_set_led(uint32_t led_status, uint8_t buf[4])
{
    uint16_t num = (uint16_t)(led_status);
    uint8_t which = (uint8_t)(led_status >> 16);
    uint8_t decimals = (uint8_t)(led_status >> 24);

    /* Map digits to segments */
    buf[0] = hex_to_segment_map[(num & 0x000f) >> 0];
    buf[1] = hex_to_segment_map[(num & 0x00f0) >> 4];
    buf[2] = hex_to_segment_map[(num & 0x0f00) >> 8];
    buf[3] = hex_to_segment_map[(num & 0xf000) >> 12];

    /* Clear LEDs which shouldn't be "on" */
    if (!(which & 0x1)) buf[0] = 0;
    if (!(which & 0x2)) buf[1] = 0;
    if (!(which & 0x4)) buf[2] = 0;
    if (!(which & 0x8)) buf[3] = 0;

    /* Add decimal segment to appropriate LEDs */
    if (decimals & 0x1) buf[0] |= DECIMAL_PT;
    if (decimals & 0x2) buf[1] |= DECIMAL_PT;
    if (decimals & 0x4) buf[2] |= DECIMAL_PT;
    if (decimals & 0x8) buf[3] |= DECIMAL_PT;
}

/*
 * Converts a 4 char array to the LED segment
 * format used by the taux controller. The buffer
 * must be at 4 bytes long. Note that some characters
 * are undisplayable: K, M, V, W, X, and all
 * non-alphanumeric characters.
 */
static int32_t
taux_convert_set_led_str(const char *str, uint8_t buf[4])
{
    uint8_t tmp[4];

    int32_t i, j;
    for (i = 0, j = 0; i < 4; ++i, ++j) {
        char c = str[j];
        uint8_t seg;

        /* Convert characters from string */
        if (c == ' ') {
            seg = 0;
        } else {
            if (c >= '0' && c <= '9') {
                seg = hex_to_segment_map[c - '0'];
            } else if (c >= 'a' && c <= 'z') {
                seg = alpha_to_segment_map[c - 'a'];
            } else if (c >= 'A' && c <= 'Z') {
                seg = alpha_to_segment_map[c - 'A'];
            } else {
                return -1;
            }

            if (seg == 0) {
                return -1;
            }
        }

        /* If next char in string is a decimal point, turn on DP LED */
        if (str[j + 1] == '.') {
            seg |= DECIMAL_PT;
            j++;
        }

        tmp[i] = seg;
    }

    /* String must be exactly 4 chars (excluding decimal points) */
    if (str[j] != '\0') {
        return -1;
    }

    /* Copy string to buffer in reverse */
    for (i = 0; i < 4; ++i) {
        buf[i] = tmp[3 - i];
    }
    return 0;
}

/*
 * Sends a command to the taux controller. This will
 * increment the number of pending ACKs.
 */
static void
taux_send_cmd(uint8_t cmd)
{
    serial_write(TAUX_COM_PORT, cmd);
    pending_acks++;
}

/*
 * Sends a SET_LED command to the taux controller. This
 * will increment the number of pending ACKs, and reset
 * the SET_LED pending flag.
 */
static void
taux_send_cmd_set_led(uint8_t led_segments[4])
{
    uint8_t buf[6];
    buf[0] = MTCP_LED_SET;

    /* Always set all 4 LEDs */
    buf[1] = 0xf;

    /* Copy in the segment data */
    memcpy(&buf[2], led_segments, 4);

    uint32_t i;
    for (i = 0; i < sizeof(buf); ++i) {
        serial_write(TAUX_COM_PORT, buf[i]);
    }
    pending_acks++;
    set_led_pending = false;
}

/*
 * Handles the INIT ioctl() call.
 */
static int32_t
taux_ioctl_init(void)
{
    /*
     * Don't allow INIT if we're waiting on any ACKs.
     * Technically we should only allow INIT to be called once,
     * but it's not really harmful to do it multiple times.
     * This is just to prevent spamming.
     */
    if (pending_acks > 0) {
        return -1;
    }

    taux_send_cmd(MTCP_BIOC_ON);
    taux_send_cmd(MTCP_LED_USR);
    taux_send_cmd(MTCP_POLL);
    taux_send_cmd_set_led(led_segments);
    return 0;
}

/*
 * Handles the SET_LED ioctl() call.
 */
static int32_t
taux_ioctl_set_led(uint32_t arg)
{
    /* Convert and save the LED status */
    taux_convert_set_led(arg, led_segments);
    set_led_pending = true;

    /* If we're not waiting for something else, send the packet immediately */
    if (pending_acks == 0) {
        taux_send_cmd_set_led(led_segments);
    }

    /* No way we can fail under normal circumstances */
    return 0;
}

/*
 * Handles the SET_LED_STR ioctl() call.
 */
static int32_t
taux_ioctl_set_led_str(uint32_t arg)
{
    /* Copy string to kernel (max length = 8 chars + NUL) */
    char str[8 + 1];
    if (!strscpy_from_user(str, (const char *)arg, sizeof(str))) {
        debugf("String too long or invalid\n");
        return -1;
    }

    /* Convert the string to led segment format */
    if (taux_convert_set_led_str(str, led_segments) < 0) {
        debugf("Invalid string format\n");
        return -1;
    }
    set_led_pending = true;

    /* If we're not waiting for something else, send the packet immediately */
    if (pending_acks == 0) {
        taux_send_cmd_set_led(led_segments);
    }

    return 0;
}

/*
 * Handles the GET_BUTTONS ioctl() call.
 */
static int32_t
taux_ioctl_get_buttons(uint32_t arg)
{
    uint8_t *ptr = (uint8_t *)arg;
    if (!copy_to_user(ptr, &button_status, sizeof(button_status))) {
        debugf("Invalid pointer; could not copy button status\n");
        return -1;
    }
    return 0;
}

/*
 * Handles any received ACK packets.
 */
static void
taux_handle_ack(void)
{
    /* One less ACK to worry about */
    pending_acks--;

    /*
     * This should never happen! If it does, our assumption
     * that we don't get any ACKs for commands sent before
     * a RESET is probably wrong.
     */
    ASSERT(pending_acks >= 0);

    /*
     * If we have no more pending ACKs and someone tried to set the LEDs
     * while we were waiting, handle that now.
     */
    if (pending_acks == 0 && set_led_pending) {
        taux_send_cmd_set_led(led_segments);
    }
}

/*
 * Handles any received RESET packets.
 */
static void
taux_handle_reset(void)
{
    /* Assume RESET causes any in-flight commands to be dropped */
    pending_acks = 0;

    /* Re-initialize the taux controller */
    taux_ioctl_init();
}

/*
 * Handles any received BIOC_EVENT packets.
 */
static void
taux_handle_bioc_event(uint8_t b, uint8_t c)
{
    uint8_t button_status_new = 0;
    /* Bits in raw packet are active-low, so invert them */
    /* Left and down bits are annoyingly reversed */
    button_status_new |= ~b & 0xf;        /* CBAS    */
    button_status_new |= !(c & 0x1) << 4; /* Up      */
    button_status_new |= !(c & 0x4) << 5; /* Down    */
    button_status_new |= !(c & 0x2) << 6; /* Left    */
    button_status_new |= !(c & 0x8) << 7; /* Right   */
    button_status = button_status_new;
}

/*
 * Handles any received POLL_OK packets.
 */
static void
taux_handle_poll_ok(uint8_t b, uint8_t c)
{
    /* Treat POLL_OK as a combined BIOC event and ACK packet */
    taux_handle_bioc_event(b, c);
    taux_handle_ack();
}

/*
 * Taux controller open syscall handler. Always succeeds.
 */
int32_t
taux_open(const char *filename, file_obj_t *file)
{
    return 0;
}

/*
 * Taux controller read syscall handler. Does nothing,
 * but always returns success (avoids grep failing).
 */
int32_t
taux_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    return 0;
}

/*
 * Taux controller write syscall handler. Always fails.
 */
int32_t
taux_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return -1;
}

/*
 * Taux controller close syscall handler. Always succeeds.
 */
int32_t
taux_close(file_obj_t *file)
{
    return 0;
}

/*
 * Taux controller ioctl syscall handler.
 */
int32_t
taux_ioctl(file_obj_t *file, uint32_t req, uint32_t arg)
{
    switch (req) {
    case TUX_INIT:
        return taux_ioctl_init();
    case TUX_SET_LED:
        return taux_ioctl_set_led(arg);
    case TUX_BUTTONS:
        return taux_ioctl_get_buttons(arg);
    case TUX_SET_LED_STR:
        return taux_ioctl_set_led_str(arg);
    default:
        return -1;
    }
}

/*
 * Handles response packets from the taux controller.
 * A "packet" is a group of three bytes, the first being
 * a response code, and the remaining two being extra data.
 */
static void
taux_handle_packet(uint8_t packet[3])
{
    uint8_t a = packet[0];
    uint8_t b = packet[1];
    uint8_t c = packet[2];

    switch (a) {
    case MTCP_POLL_OK:
        taux_handle_poll_ok(b, c);
        break;
    case MTCP_BIOC_EVENT:
        taux_handle_bioc_event(b, c);
        break;
    case MTCP_RESET:
        taux_handle_reset();
        break;
    case MTCP_ACK:
        taux_handle_ack();
        break;
    case MTCP_ERROR:
        taux_handle_ack();
        break;
    default:
        debugf("Unhandled packet: %x\n", a);
        break;
    }
}

/*
 * Called when there is data to be read in the UART buffer.
 */
static void
taux_handle_irq(void)
{
    static uint8_t buf[12];
    static int32_t count = 0;

    /* Consume all available data */
    int32_t read;
    while ((read = serial_read_all(TAUX_COM_PORT, &buf[count], sizeof(buf) - count)) > 0) {
        /* Update number of chars now in the buffer */
        count += read;

        /* Now scan for complete packets */
        int32_t i;
        for (i = 0; i < count - 2; ++i) {
            /*
             * Align the "frame": first byte has high bit == 0,
             * next 2 have high bit == 1. If at any point the
             * alignment goes out of sync, just discard it
             * and find the next good reference point.
             */
            if ((buf[i + 0] & 0x80) == 0 &&
                (buf[i + 1] & 0x80) != 0 &&
                (buf[i + 2] & 0x80) != 0) {

                /* Process the packet */
                taux_handle_packet(&buf[i]);

                /* Skip over the other 2 bytes */
                i += 2;
            }
        }

        /* Shift over the remaining chars for the next iteration */
        memmove(&buf[0], &buf[i], count - i);
        count -= i;
    }
}

/*
 * Initializes the taux controller driver.
 */
void
taux_init(void)
{
    serial_init(
        TAUX_COM_PORT,
        TAUX_BAUD_RATE,
        TAUX_CHAR_BITS,
        TAUX_STOP_BITS,
        TAUX_PARITY,
        TAUX_TRIGGER_LEVEL,
        taux_handle_irq);
}
