#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
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
#define WIDTH 1024
#define HEIGHT 576
#define BPP 16
#define FBSIZE (WIDTH * HEIGHT * ((BPP + 1) / 8))

/*
 * Socket address to download pixel data from.
 */
#define SERVER_IP IP(10, 0, 2, 2)
#define SERVER_PORT 8989

static volatile bool exit = false;

static void
sigint_handler(int)
{
    exit = true;
}

int
main(void)
{
    int ret = 1;
    int sockfd = -1;
    char *vbemem = NULL;

    sigaction(SIGINT, sigint_handler);

    sockfd = socket(SOCK_TCP);
    if (sockfd < 0) {
        fprintf(stderr, "socket() failed\n");
        goto exit;
    }

    sock_addr_t addr;
    addr.ip = SERVER_IP;
    addr.port = SERVER_PORT;
    if (connect(sockfd, &addr) < 0) {
        fprintf(stderr, "connect() failed\n");
        goto exit;
    }

    if (vbemap((void **)&vbemem, WIDTH, HEIGHT, BPP) < 0) {
        fprintf(stderr, "vbemap() failed\n");
        goto exit;
    }

    int flip = 0;
    while (!exit) {
        int off = 0;
        while (!exit && off < FBSIZE) {
            int rret = read(sockfd, &vbemem[flip * FBSIZE + off], FBSIZE - off);
            if (rret < 0) {
                if (rret == -EAGAIN || rret == -EINTR) {
                    continue;
                }
                fprintf(stderr, "read() failed\n");
                goto exit;
            } else if (rret == 0) {
                break;
            }
            off += rret;
        }

        flip = vbeflip(vbemem);
        if (flip < 0) {
            fprintf(stderr, "vbeflip() failed\n");
            goto exit;
        }
    }

    ret = 0;

exit:
    if (vbemem != NULL) {
        vbeunmap(vbemem);
    }

    if (sockfd >= 0) {
        close(sockfd);
    }

    return ret;
}
