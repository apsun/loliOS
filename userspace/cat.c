#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static int
read_once(int fd, void *buf, int nbytes)
{
    int ret;
    do {
        ret = read(fd, buf, nbytes);
    } while (ret == -EAGAIN || ret == -EINTR);
    return ret;
}

static int
write_all(int fd, const void *buf, int nbytes)
{
    const char *bufp = buf;
    int total = 0;
    while (total < nbytes) {
        int ret = write(fd, &bufp[total], nbytes - total);
        if (ret == -EAGAIN || ret == -EINTR) {
            continue;
        } else if (ret < 0) {
            return ret;
        }
        total += ret;
    }
    return total;
}

static int
copy_stream(int outputfd, int inputfd)
{
    char buf[8192];
    int total = 0;
    while (1) {
        int read_cnt = read_once(inputfd, buf, sizeof(buf));
        if (read_cnt < 0) {
            fprintf(stderr, "read() returned %d\n", read_cnt);
            return -1;
        } else if (read_cnt == 0) {
            break;
        }

        int write_cnt = write_all(outputfd, buf, read_cnt);
        if (write_cnt < 0) {
            fprintf(stderr, "write() returned %d\n", write_cnt);
            return -1;
        }
        total += write_cnt;
    }

    return total;
}

int
main(void)
{
    int ret = 1;
    int fd = STDIN_FILENO;

    /*
     * If file name is specified as an argument, read from it.
     * Otherwise, read from stdin by default.
     */
    char filename[128];
    if (getargs(filename, sizeof(filename)) >= 0) {
        if ((fd = create(filename, OPEN_READ)) < 0) {
            fprintf(stderr, "%s: No such file or directory\n", filename);
            goto cleanup;
        }
    }

    if (copy_stream(STDOUT_FILENO, fd) < 0) {
        goto cleanup;
    }

    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}
