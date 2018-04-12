#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <syscall.h>

#define LINE_LENGTH 16
#define SCREEN_HEIGHT 24

bool
print_line(int fd, int *off)
{
    /* Read one line worth of bytes from file */
    uint8_t buf[LINE_LENGTH];
    int num = sizeof(buf);
    if ((num = read(fd, buf, num)) == 0) {
        return false;
    }

    /* Print offset */
    printf("%08x: ", *off);

    /* Print bytes as hex */
    int i;
    for (i = 0; i < num; ++i) {
        printf("%02x ", buf[i]);
    }

    /* Align char view */
    for (i = 0; i < LINE_LENGTH - num; ++i) {
        printf("   ");
    }

    /* Print bytes as char */
    for (i = 0; i < num; ++i) {
        char c = buf[i];
        if (c < 32 || c >= 127) {
            putchar('.');
        } else {
            putchar(c);
        }
    }

    putchar('\n');
    *off += num;
    return true;
}

bool
print_screen(int fd, int *off)
{
    int i;
    for (i = 0; i < SCREEN_HEIGHT; ++i) {
        if (!print_line(fd, off)) {
            return false;
        }
    }
    return true;
}

int
main(void)
{
    int ret = 1;
    int fd = -1;
    char buf[128];

    /* Read file name */
    if (getargs(buf, sizeof(buf)) < 0) {
        puts("Could not read arguments");
        goto cleanup;
    }

    /* Open file */
    if ((fd = open(buf)) < 0) {
        puts("File not found");
        goto cleanup;
    }

    /* Print file, one screen at a time */
    int off = 0;
    while (print_screen(fd, &off)) {
        printf("--More--");
        gets(buf, sizeof(buf));
    }
    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}
