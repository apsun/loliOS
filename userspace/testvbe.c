#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <syscall.h>
#include <string.h>

/*
 * Converts 8-bit R/G/B values to a single 32bpp pixel.
 */
#define RGB32(r, g, b) ( \
    (((r) & 0xff) << 16) | \
    (((g) & 0xff) << 8) | \
    (((b) & 0xff) << 0))

/*
 * Decomposes a 32bpp pixel to the component colors.
 */
#define RGB32_R(rgb32) (((rgb32) >> 16) & 0xff)
#define RGB32_G(rgb32) (((rgb32) >> 8) & 0xff)
#define RGB32_B(rgb32) (((rgb32) >> 0) & 0xff)

int
main(void)
{
    int ret;

    uint32_t *vbemem;
    ret = vbemap((void **)&vbemem, 1920, 1080, 32);
    assert(ret >= 0);

    memset(vbemem, 0xff, 8 * 1024 * 1024);

    /*
     * TODO: Simple gradient lerp algorithm for testing
     *
     * https://uigradients.com/#AzurLane
     */
    uint32_t start = 0x007f7fd5;
    uint32_t end = 0x0091eae4;
    int i;
    for (i = 0; i < 1920 * 1080; ++i) {
        vbemem[i] = RGB32(
            RGB32_R(start) + (RGB32_R(end) - RGB32_R(start)) * (i % 1920) / 1920,
            RGB32_G(start) + (RGB32_G(end) - RGB32_G(start)) * (i % 1920) / 1920,
            RGB32_B(start) + (RGB32_B(end) - RGB32_B(start)) * (i % 1920) / 1920);
    }


    nanotime_t now;
    monotime(&now);
    nanotime_t target = now + SECONDS(3);
    monosleep(&target);

    ret = vbeunmap(&vbemem);
    assert(ret >= 0);
    return 0;
}
