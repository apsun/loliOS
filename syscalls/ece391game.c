#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define TUX_SET_LED     0x10
#define TUX_BUTTONS     0x12
#define TUX_INIT        0x13

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

    if ((fd = ece391_open("taux")) < 0) {
        puts("Could not open taux file\n");
        goto cleanup;
    }

    if (ece391_ioctl(fd, TUX_INIT, 0) < 0) {
        puts("ioctl(TUX_INIT) failed\n");
        goto cleanup;
    }

    if (ece391_ioctl(fd, TUX_SET_LED, 0x000f1234) < 0) {
        puts("ioctl(TUX_SET_LED) failed\n");
        goto cleanup;
    }

    uint8_t buttons;
    if (ece391_ioctl(fd, TUX_BUTTONS, (uint32_t)&buttons) < 0) {
        puts("ioctl(TUX_BUTTONS) failed\n");
        goto cleanup;
    }

    puts("OK\n");
    ret = 0;

cleanup:
    if (fd >= 0) ece391_close(fd);
    return ret;
}
