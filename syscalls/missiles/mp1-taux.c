#include "mp1-taux.h"
#include <assert.h>
#include <stddef.h>
#include <syscall.h>

void
taux_display_str(int taux_fd, const char *str)
{
    if (ioctl(taux_fd, TUX_SET_LED_STR, (int)str) < 0) {
        assert(0);
    }
}

void
taux_display_time(int taux_fd, int num_seconds)
{
    int packed = 0;
    int num_minutes = num_seconds / 60;
    num_seconds = num_seconds % 60;

    /*
     * Seconds should occupy the lower 2 hex digits (1 byte)
     * Minutes should occupy the higher 2 hex digits
     */
    packed |= ((num_seconds % 10 + num_seconds / 10 * 16) & 0xFF);
    packed |= ((num_minutes % 10 + num_minutes / 10 * 16) & 0xFF) << 8;

    /*
     * Seconds are always shown, only show one minute digit
     * unless >= 10 minutes have elapsed.
     * X.X.X.X.
     * ? 1 1 1
     */
    if (num_minutes < 10) {
        packed |= 0x7 << 16; /* 0111 */
    } else {
        packed |= 0xf << 16; /* 1111 */
    }

    /*
     * Display a decimal point as the minute-second delimiter
     * X.X.X.X.
     *  0 1 0 0
     */
    packed |= 0x4 << 24; /* 0100 */

    /* Call the ioctl! */
    if (ioctl(taux_fd, TUX_SET_LED, packed) < 0) {
        assert(0);
    }
}

void
taux_display_coords(int taux_fd, int x, int y)
{
    int packed = 0;

    packed |= ((y % 10 + y / 10 * 16) & 0xFF);
    packed |= ((x % 10 + x / 10 * 16) & 0xFF) << 8;
    packed |= 0xf << 16; /* All LEDs on */
    packed |= 0x4 << 24; /* Decimal point in middle */

    if (ioctl(taux_fd, TUX_SET_LED, packed) < 0) {
        assert(0);
    }
}

void
taux_display_num(int taux_fd, int num)
{
    int packed = (
        num / 1 % 10 +
        num / 10 % 10 * 16 +
        num / 100 % 10 * 256 +
        num / 1000 % 10 * 4096);

    /* Don't display leading zeros */
    if (num >= 0) packed |= 1 << 16;
    if (num >= 10) packed |= 1 << 17;
    if (num >= 100) packed |= 1 << 18;
    if (num >= 1000) packed |= 1 << 19;

    if (ioctl(taux_fd, TUX_SET_LED, packed) < 0) {
        assert(0);
    }
}

int
taux_get_input(int taux_fd)
{
    static int prev_raw_buttons = 0;

    /* Get raw button data from driver... */
    int raw_buttons;
    ioctl(taux_fd, TUX_BUTTONS, (int)&raw_buttons);

    /* ... and convert to normalized form */
    int buttons = 0;

    /* Check if A/B/C/start button changed from up -> down */
    if ((raw_buttons & TB_A) && !(prev_raw_buttons & TB_A)) buttons |= TB_A;
    if ((raw_buttons & TB_B) && !(prev_raw_buttons & TB_B)) buttons |= TB_B;
    if ((raw_buttons & TB_C) && !(prev_raw_buttons & TB_C)) buttons |= TB_C;
    if ((raw_buttons & TB_START) && !(prev_raw_buttons & TB_START)) buttons |= TB_START;

    /* Update state for up/down/left/right */
    buttons &= 0x0f;
    buttons |= (raw_buttons & 0xf0);

    /* Update saved raw button data */
    prev_raw_buttons = raw_buttons;
    return buttons;
}
