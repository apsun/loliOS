#include <stddef.h>
#include <stdio.h>
#include <syscall.h>

int
main(void)
{
    int ret = 1;
    int fd = -1;
    char fname[33];

    /* Open the directory */
    if ((fd = create(".", OPEN_READ)) < 0) {
        fprintf(stderr, "Cannot open directory for reading\n");
        goto cleanup;
    }

    /* Read dir entries, print to stdout */
    int cnt;
    while ((cnt = read(fd, fname, sizeof(fname))) != 0) {
        if (cnt < 0) {
            fprintf(stderr, "Cannot read directory entry\n");
            goto cleanup;
        }

        fname[cnt] = '\0';
        puts(fname);
    }

    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}
