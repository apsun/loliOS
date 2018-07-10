#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static int
input(int fd, char *buf, int buf_size, int *offset)
{
    /* Check if we have space left to read */
    int to_read = buf_size - *offset;
    if (to_read == 0) {
        return -EAGAIN;
    }

    /* Read data into buffer */
    int ret = read(fd, &buf[*offset], to_read);
    if (ret <= 0) {
        return ret;
    }

    /* Advance offset */
    *offset += ret;
    return ret;
}

static int
output(int fd, char *buf, int *count)
{
    /* Check if we have anything to write */
    if (*count == 0) {
        return -EAGAIN;
    }

    /* Write out buffer contents */
    int ret = write(fd, buf, *count);
    if (ret <= 0) {
        return ret;
    }

    /* Shift remaining bytes up */
    memmove(&buf[0], &buf[ret], *count - ret);
    *count -= ret;
    return ret;
}

static int
copy_stream(int outputfd, int inputfd)
{
    /*
     * Buffer size of 8192 chosen because it happens to correlate
     * with the TCP inbox size and the music buffer size.
     */
    char buf[8192];
    int offset = 0;
    int read_cnt;
    int write_cnt;
    int total_copied = 0;
    while (1) {
        /* Read bytes from input stream */
        read_cnt = input(inputfd, buf, sizeof(buf), &offset);
        if (read_cnt < 0 && read_cnt != -EINTR && read_cnt != -EAGAIN) {
            fprintf(stderr, "read() returned %d\n", read_cnt);
            return -1;
        }

        if (read_cnt == 0 && offset == 0) {
            break;
        }

        /* Write bytes to output stream */
        write_cnt = output(outputfd, buf, &offset);
        if (write_cnt < 0 && write_cnt != -EINTR && write_cnt != -EAGAIN) {
            fprintf(stderr, "write() returned %d\n", write_cnt);
            return -1;
        }

        if (write_cnt > 0) {
            total_copied += write_cnt;
        }

        /* If we can't read or write, yield the rest of our timeslice */
        bool cant_read = (read_cnt == -EAGAIN || offset == sizeof(buf));
        bool cant_write = (write_cnt = -EAGAIN || (offset == 0 && write_cnt == 0));
        if (cant_read && cant_write) {
            yield();
        }
    }

    return total_copied;
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
        if ((fd = open(filename)) < 0) {
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
