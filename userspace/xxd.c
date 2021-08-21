#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <syscall.h>

#define LINE_LENGTH 16
#define SCREEN_HEIGHT 24

typedef struct {
    char buf[128];
    bool interactive : 1;
    char *argv;
} args_t;

static void
reset_stdin(void)
{
    int in = create("tty", OPEN_READ);
    assert(in >= 0);
    dup(in, STDIN_FILENO);
    close(in);
}

static int
print_line(int fd, int *off)
{
    /* Read one line worth of bytes from file */
    uint8_t buf[LINE_LENGTH];
    int num = 0;
    int ret;
    while (num < (int)sizeof(buf)) {
        ret = read(fd, &buf[num], sizeof(buf) - num);
        if (ret == -EINTR || ret == -EAGAIN) {
            continue;
        } else if (ret < 0 || (ret == 0 && num == 0)) {
            return ret;
        } else if (ret == 0) {
            break;
        }
        num += ret;
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
    return ret;
}

static int
print_screen(int fd, int *off)
{
    int i;
    for (i = 0; i < SCREEN_HEIGHT; ++i) {
        int ret = print_line(fd, off);
        if (ret <= 0) {
            return ret;
        }
    }
    return 1;
}

static bool
parse_args(args_t *args)
{
    args->argv = args->buf;

    if (getargs(args->buf, sizeof(args->buf)) < 0) {
        args->buf[0] = '\0';
        return true;
    }

    while (1) {
        if (*args->argv == ' ') {
            args->argv++;
        } else if (*args->argv == '-') {
            char c;
            while ((c = *++args->argv) && c != ' ') {
                switch (c) {
                case 'i':
                    args->interactive = true;
                    break;
                default:
                    fprintf(stderr, "Unknown option: %c\n", c);
                    return false;
                }
            }
        } else {
            return true;
        }
    }
}

int
main(void)
{
    int ret = 1;
    int fd = 0;

    /*
     * Parse arguments.
     *
     * Note: it makes no sense to use both interactive input
     * and stdin as input, and will totally confuse the reader
     * (and also lose some input!)
     *
     * Interactive input should only be used when stdin is
     * redirected to a non-tty file. Since we can't check
     * this though, assume the user knows what they're doing.
     */
    args_t args = {.interactive = false};
    if (!parse_args(&args)) {
        goto cleanup;
    }

    /* If in interactive mode, reset stdin */
    if (args.interactive) {
        if (*args.argv == '\0') {
            if ((fd = dup(0, -1)) < 0) {
                fprintf(stderr, "Failed to dup stdin\n");
                goto cleanup;
            }
        }

        reset_stdin();
    }

    /* If file is specified, use that as input */
    if (*args.argv != '\0') {
        if ((fd = create(args.argv, OPEN_READ)) < 0) {
            fprintf(stderr, "%s: No such file or directory\n", args.argv);
            goto cleanup;
        }
    }

    /* Print file, one screen at a time */
    int off = 0;
    int status;
    char buf[129];
    while ((status = print_screen(fd, &off)) > 0) {
        if (args.interactive) {
            fprintf(stderr, "--More--");
            gets(buf, sizeof(buf));
        }
    }

    if (status < 0) {
        fprintf(stderr, "Failed to read from file\n");
        goto cleanup;
    }

    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}
