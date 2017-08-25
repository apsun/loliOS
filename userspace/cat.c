#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <syscall.h>

int32_t
main(void)
{
    int32_t ret = 0;
    int32_t fd = -1;
    char buf[1024];

    /* Read file name */
    if (getargs(buf, sizeof(buf)) < 0) {
        puts("could not read arguments");
        ret = 3;
        goto exit;
    }

    /* Open file */
    if ((fd = open(buf)) < 0) {
        puts("file not found");
        ret = 2;
        goto exit;
    }

    /* Read from file, write to stdout */
    int32_t cnt;
    while ((cnt = read(fd, buf, sizeof(buf))) != 0) {
        if (cnt < 0) {
            puts("file read failed");
            ret = 3;
            goto exit;
        }

        if (write(1, buf, cnt) < 0) {
            ret = 3;
            goto exit;
        }
    }

exit:
    if (fd >= 0) close(fd);
    return ret;
}
