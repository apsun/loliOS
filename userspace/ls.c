#include <stddef.h>
#include <stdio.h>
#include <syscall.h>

int
main(void)
{
    int ret = 0;
    int fd = -1;
    char fname[33];

    /* Open the directory */
    if ((fd = open(".")) < 0) {
        puts("directory open failed");
        ret = 2;
        goto exit;
    }

    /* Read dir entries, print to stdout */
    int cnt;
    while ((cnt = read(fd, fname, sizeof(fname))) != 0) {
        if (cnt < 0) {
            puts("directory entry read failed");
            ret = 3;
            goto exit;
        }

        fname[cnt] = '\0';
        puts(fname);
    }

exit:
    if (fd >= 0) close(fd);
    return ret;
}