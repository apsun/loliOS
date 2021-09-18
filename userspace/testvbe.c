#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

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

/*
 * Screen dimensions.
 */
#define WIDTH 1280
#define HEIGHT 720
#define BPP 32
#define FBSIZE (WIDTH * HEIGHT * ((BPP + 1) / 8))

int
main(void)
{
    int ret = 1;

    int secs = 15;
    char args[128];
    if (getargs(args, sizeof(args)) >= 0) {
        secs = atoi(args);
        if (secs == 0) {
            fprintf(stderr, "Invalid duration: %s\n", args);
            goto cleanup;
        }
    }

    uint32_t *buf = malloc(FBSIZE);
    assert(buf != NULL);

    uint32_t col0 = 0x007f7fd5;
    uint32_t col1 = 0x0091eae4;
    int i;
    for (i = 0; i < WIDTH * HEIGHT; ++i) {
        buf[i] = RGB32(
            RGB32_R(col0) + (RGB32_R(col1) - RGB32_R(col0)) * (i % WIDTH) / WIDTH,
            RGB32_G(col0) + (RGB32_G(col1) - RGB32_G(col0)) * (i % WIDTH) / WIDTH,
            RGB32_B(col0) + (RGB32_B(col1) - RGB32_B(col0)) * (i % WIDTH) / WIDTH);
    }

    char *vbemem;
    vbemap((void **)&vbemem, WIDTH, HEIGHT, BPP);

    int start = monotime();
    int end = start + secs * 1000;

    int iter = 0;
    int flip = 0;
    while (monotime() < end) {
        memcpy(&vbemem[flip * FBSIZE], buf, FBSIZE);
        iter++;
        flip = vbeflip(vbemem);
    }

    vbeunmap(vbemem);
    printf("\n%d frames @ %dx%d (~%d fps)\n", iter, WIDTH, HEIGHT, iter / secs);
    ret = 0;

cleanup:
    return ret;
}
