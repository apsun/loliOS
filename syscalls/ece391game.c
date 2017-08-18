#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define TUX_SET_LED     0x10
#define TUX_BUTTONS     0x12
#define TUX_INIT        0x13

enum {
    TB_START  = 0x01,
    TB_A      = 0x02,
    TB_B      = 0x04,
    TB_C      = 0x08,
    TB_UP     = 0x10,
    TB_DOWN   = 0x20,
    TB_LEFT   = 0x40,
    TB_RIGHT  = 0x80,
    TB_ALL    = 0xff,
};

void
puts(const char *s)
{
    ece391_fdputs(1, (uint8_t *)s);
}

int
main(void)
{
    int ret = 1;
    int32_t fd = -1;

    if ((fd = ece391_open((uint8_t *)"taux")) < 0) {
        puts("Could not open taux file\n");
        goto cleanup;
    }

    if (ece391_ioctl(fd, TUX_INIT, 0) < 0) {
        puts("ioctl(TUX_INIT) failed\n");
        goto cleanup;
    }

    if (ece391_ioctl(fd, TUX_SET_LED, 0x000f0000) < 0) {
        puts("ioctl(TUX_SET_LED) failed\n");
        goto cleanup;
    }

    uint8_t prev_buttons = 0;
    uint32_t num = 0;
    uint8_t buttons;
    while (1) {
        if (ece391_ioctl(fd, TUX_BUTTONS, (uint32_t)&buttons) < 0) {
            puts("ioctl(TUX_BUTTONS) failed\n");
            goto cleanup;
        }

        if (buttons == prev_buttons) {
            continue;
        }

        num++;
        if (ece391_ioctl(fd, TUX_SET_LED, 0x000f0000 | num) < 0) {
            puts("ioctl(TUX_SET_LED) failed\n");
            goto cleanup;
        }

        puts("Buttons: ");
        if (buttons == 0)
            puts("none,");
        if (buttons & TB_START)
            puts("start,");
        if (buttons & TB_A)
            puts("a,");
        if (buttons & TB_B)
            puts("b,");
        if (buttons & TB_C)
            puts("c,");
        if (buttons & TB_UP)
            puts("up,");
        if (buttons & TB_DOWN)
            puts("down,");
        if (buttons & TB_LEFT)
            puts("left,");
        if (buttons & TB_RIGHT)
            puts("right,");
        puts("\b\n");

        prev_buttons = buttons;
    }

cleanup:
    if (fd >= 0) ece391_close(fd);
    return ret;
}
