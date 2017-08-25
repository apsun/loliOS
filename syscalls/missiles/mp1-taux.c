#include "mp1-taux.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>

void
taux_display_time(int32_t taux_fd, int32_t num_seconds)
{
    uint32_t packed = 0;
    int32_t num_minutes = num_seconds / 60;
    num_seconds = num_seconds % 60;

    /*
     * Seconds should occupy the lower 2 hex digits (1 byte)
     * Minutes should occupy the higher 2 hex digits
     */
    packed |= (uint8_t)(num_seconds % 10 + num_seconds / 10 * 16);
    packed |= (uint8_t)(num_minutes % 10 + num_minutes / 10 * 16) << 8;

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
    ioctl(taux_fd, TUX_SET_LED, packed);
}

uint8_t
taux_get_input(int32_t taux_fd)
{
    static uint8_t prev_raw_buttons = 0;

    /* Get raw button data from driver... */
    uint8_t raw_buttons;
    ioctl(taux_fd, TUX_BUTTONS, (uint32_t)&raw_buttons);

    /* ... and convert to normalized form */
    uint8_t buttons = 0;

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
