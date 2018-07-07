#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static void
reset_stdout(void)
{
    int stdout = create("tty", OPEN_WRITE);
    assert(stdout >= 0);
    dup(stdout, 1);
    close(stdout);
}

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

int
main(void)
{
    int ret = 1;
    int fd = 0;

    /*
     * If file name is specified as an argument, read from it.
     * Otherwise, read from stdin by default.
     */
    char filename[128];
    if (getargs(filename, sizeof(filename)) >= 0) {
        if ((fd = open(filename)) < 0) {
            reset_stdout();
            printf("%s: No such file or directory\n", filename);
            goto cleanup;
        }
    }

    /*
     * Buffer size of 8192 chosen because it happens to correlate
     * with the TCP inbox size and the music buffer size.
     */
    char buf[8192];
    int offset = 0;
    int read_cnt;
    int write_cnt;
    while (1) {
        read_cnt = input(fd, buf, sizeof(buf), &offset);
        if (read_cnt < 0 && read_cnt != -EINTR && read_cnt != -EAGAIN) {
            reset_stdout();
            printf("read() returned %d\n", read_cnt);
            goto cleanup;
        }

        if (read_cnt == 0 && offset == 0) {
            break;
        }

        write_cnt = output(1, buf, &offset);
        if (write_cnt < 0 && write_cnt != -EINTR && write_cnt != -EAGAIN) {
            reset_stdout();
            printf("write() returned %d\n", write_cnt);
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}
