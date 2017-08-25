#include <types.h>
#include <sys.h>
#include <io.h>
#include <string.h>

#define TUX_SET_LED 0x10
#define TUX_BUTTONS 0x12
#define TUX_INIT    0x13

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

int32_t
main(void)
{
    int32_t ret = 0;
    int32_t fd = -1;

    if ((fd = open("taux")) < 0) {
        puts("Could not open taux file");
        ret = 2;
        goto exit;
    }

    if (ioctl(fd, TUX_INIT, 0) < 0) {
        puts("ioctl(TUX_INIT) failed");
        ret = 1;
        goto exit;
    }

    if (ioctl(fd, TUX_SET_LED, 0x000f0000) < 0) {
        puts("ioctl(TUX_SET_LED) failed");
        ret = 1;
        goto exit;
    }

    uint8_t prev_buttons = 0;
    uint32_t num = 0;
    uint8_t buttons;
    while (1) {
        if (ioctl(fd, TUX_BUTTONS, (uint32_t)&buttons) < 0) {
            puts("ioctl(TUX_BUTTONS) failed");
            ret = 1;
            goto exit;
        }

        if (buttons == prev_buttons) {
            continue;
        }

        num++;
        if (ioctl(fd, TUX_SET_LED, 0x000f0000 | num) < 0) {
            puts("ioctl(TUX_SET_LED) failed");
            ret = 1;
            goto exit;
        }

        printf("Buttons: ");
        if (buttons == 0)
            printf("none,");
        if (buttons & TB_START)
            printf("start,");
        if (buttons & TB_A)
            printf("a,");
        if (buttons & TB_B)
            printf("b,");
        if (buttons & TB_C)
            printf("c,");
        if (buttons & TB_UP)
            printf("up,");
        if (buttons & TB_DOWN)
            printf("down,");
        if (buttons & TB_LEFT)
            printf("left,");
        if (buttons & TB_RIGHT)
            printf("right,");
        printf("\b\n");

        prev_buttons = buttons;
    }

exit:
    if (fd >= 0) close(fd);
    return ret;
}
