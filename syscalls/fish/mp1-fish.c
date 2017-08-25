#include "mp1.h"
#include "mp1-vga.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

#define WAIT 100
#define LEFT_X 40

static void
run_loop(int32_t rtc_fd, int32_t ticks)
{
    int32_t i;
    for (i = 0; i < ticks; ++i) {
        int32_t garbage;
        read(rtc_fd, &garbage, sizeof(garbage));
        mp1_rtc_tasklet(0);
    }
}

static int32_t
add_frames(const char *f0, const char *f1)
{
    /* Open frame files */
    int32_t fd0 = open(f0);
    int32_t fd1 = open(f1);
    if (fd0 < 0 || fd1 < 0) {
        return -1;
    }

    /* Initialize blink struct */
    blink_t b;
    b.on_length = 15;
    b.off_length = 15;

    /* Iterate files in parallel */
    bool eof0 = false;
    bool eof1 = false;
    char c0 = '\0';
    char c1 = '\0';
    int32_t row = 0;
    while (!eof0 || !eof1) {
        int32_t col = 0;
        while (1) {
            if (c0 != '\n' && read(fd0, &c0, 1) == 0) {
                c0 = '\n';
                eof0 = true;
            }

            if (c1 != '\n' && read(fd1, &c1, 1) == 0) {
                c1 = '\n';
                eof1 = true;
            }

            if (c0 == '\n' && c1 == '\n') {
                break;
            } else if ((c0 != ' ' && c0 != '\n') || (c1 != ' ' && c1 != '\n')) {
                b.on_char = (c0 == '\n') ? ' ' : c0;
                b.off_char = (c1 == '\n') ? ' ' : c1;
                b.location = row * SCREEN_WIDTH + col + LEFT_X;
                mp1_ioctl((uint32_t)&b, IOCTL_ADD);
            }

            col++;
        }

        if (eof0) {
            c0 = '\n';
            close(fd0);
        } else {
            c0 = '\0';
        }

        if (eof1) {
            c1 = '\n';
            close(fd1);
        } else {
            c1 = '\0';
        }

        row++;
    }

    return 0;
}

int32_t
main(void)
{
    vga_init();

    /* Initialize RTC to 32 ticks/sec */
    int32_t rtc_fd = open("rtc");
    int32_t rtc_freq = 32;
    write(rtc_fd, &rtc_freq, sizeof(rtc_freq));

    /* Load fish frames */
    if (add_frames("frame0.txt", "frame1.txt") < 0) {
        assert(0);
    }

    /* Run for a bit... */
    run_loop(rtc_fd, WAIT);

    /* Add I/M char */
    blink_t b;
    b.on_char = 'I';
    b.off_char = 'M';
    b.on_length = 7;
    b.off_length = 6;
    b.location = 6 * SCREEN_WIDTH + LEFT_X + 20;
    mp1_ioctl((uint32_t)&b, IOCTL_ADD);
    run_loop(rtc_fd, WAIT);

    /* Sync I/M char */
    mp1_ioctl((LEFT_X << 16) | b.location, IOCTL_SYNC);
    run_loop(rtc_fd, WAIT);

    /* Remove I/M char */
    mp1_ioctl(b.location, IOCTL_REMOVE);
    run_loop(rtc_fd, WAIT);

    close(rtc_fd);
    return 0;
}
