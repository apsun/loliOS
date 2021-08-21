#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>

typedef enum {
    MODE_LINES,
    MODE_BYTES,
} mode_t;

typedef struct {
    mode_t mode;
    int count;
} args_t;

static bool
parse_args(args_t *args)
{
    args->mode = MODE_LINES;
    args->count = 10;

    char buf[128];
    if (getargs(buf, sizeof(buf)) < 0) {
        return true;
    }
    char *bufp = buf;

    while (1) {
        if (*bufp == ' ') {
            bufp++;
        } else if (*bufp == '-') {
            char c;
            while ((c = *++bufp) && c != ' ') {
                switch (c) {
                case 'n':
                    args->mode = MODE_LINES;
                    break;
                case 'c':
                    args->mode = MODE_BYTES;
                    break;
                default:
                    fprintf(stderr, "Unknown option: %c\n", c);
                    return false;
                }
            }
        } else {
            args->count = atoi(bufp);
            if (args->count == 0) {
                fprintf(stderr, "Invalid line/byte count: %s\n", bufp);
                return false;
            }
            return true;
        }
    }
}

/*
 * Writes all bytes in buf. Returns either size, or < 0 on error.
 */
int
write_all(int fd, const char *buf, int size)
{
    int written = 0;
    while (written < size) {
        int ret = write(fd, &buf[written], size - written);
        if (ret == -EINTR || ret == -EAGAIN) {
            continue;
        } else if (ret < 0) {
            return ret;
        }
        written += ret;
    }
    return written;
}

/*
 * Writes up to the specified limit number of bytes from buf.
 * Returns the number of bytes written, or < 0 on error.
 * 
 * Upon return, size contains the new number of characters in buf.
 */
int
write_bytes(int fd, char *buf, int *size, int limit)
{
    int to_write = *size;
    if (to_write > limit) {
        to_write = limit;
    }

    int written = write_all(STDOUT_FILENO, buf, to_write);
    if (written < 0) {
        return written;
    }

    memmove(&buf[0], &buf[written], *size - written);
    *size -= written;

    return written;
}

/*
 * If buf contains at least one \n, writes up to and including
 * the first \n character and returns 1. Otherwise, writes the
 * entire buffer and returns 0. Returns < 0 on error.
 *
 * Upon return, size contains the new number of characters in buf.
 */
int
write_line(int fd, char *buf, int *size)
{
    int to_write = *size;
    char *lf = strchr(buf, '\n');
    if (lf != NULL) {
        to_write = lf - buf + 1;
    }

    int written = write_all(fd, buf, to_write);
    if (written < 0) {
        return written;
    }

    memmove(&buf[0], &buf[written], *size - written);
    *size -= written;

    if (lf != NULL) {
        return 1;
    } else {
        return 0;
    }
}

int
main(void)
{
    int ret = 1;
    args_t args;
    if (!parse_args(&args)) {
        goto exit;
    }

    char buf[8192];
    int offset = 0;
    int total = 0;
    
    while (total < args.count) {
        int nr = read(STDIN_FILENO, &buf[offset], sizeof(buf) - offset);
        if (nr == 0 && offset == 0) {
            ret = 0;
            goto exit;
        } else if (nr == -EINTR || nr == -EAGAIN) {
            nr = 0;
        } else if (nr < 0) {
            fprintf(stderr, "read() returned %d\n", nr);
            goto exit;
        }
        offset += nr;

        int nw;
        switch (args.mode) {
        case MODE_BYTES:
            nw = write_bytes(STDOUT_FILENO, buf, &offset, args.count - total);
            break;
        case MODE_LINES:
            nw = write_line(STDOUT_FILENO, buf, &offset);
            break;
        default:
            abort();
        }

        if (nw < 0) {
            fprintf(stderr, "write() returned %d\n", nw);
            goto exit;
        }
        total += nw;
    }

    ret = 0;

exit:
    return ret;
}
