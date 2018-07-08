#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <syscall.h>

/*
 * Prints a single character to the specified file stream.
 */
int
fputc(char c, int fd)
{
    assert(fd >= 0);

    int ret;
    do {
        ret = write(fd, &c, 1);
    } while (ret == -EAGAIN || ret == -EINTR);

    if (ret < 0) {
        return -1;
    } else {
        return c;
    }
}

/*
 * Writes the given data to the specified file stream.
 * Returns the number of bytes written, or a negative value
 * on error. Note that this does not conform to the normal
 * libc fwrite() API.
 */
int
fwrite(const void *ptr, int n, int fd)
{
    assert(ptr != NULL);
    assert(n >= 0);
    assert(fd >= 0);

    const char *buf = ptr;
    int total = 0;
    while (total < n) {
        int ret = write(fd, &buf[total], n - total);
        if (ret == -EAGAIN || ret == -EINTR) {
            continue;
        } else if (ret < 0) {
            return ret;
        } else {
            total += ret;
        }
    }

    return total;
}

/*
 * Prints a string to the specified file stream. No newline
 * is appended to the output.
 */
int
fputs(const char *s, int fd)
{
    assert(s != NULL);
    assert(fd >= 0);
    return fwrite(s, strlen(s), fd);
}

/*
 * Prints a single character to stdout.
 */
int
putchar(char c)
{
    return fputc(c, stdout);
}

/*
 * Prints a string followed by a newline to stdout.
 */
int
puts(const char *s)
{
    assert(s != NULL);
    fputs(s, stdout);
    return putchar('\n');
}

/*
 * Reads a line from stdin. This performs internal buffering
 * to prevent cases where more than one line is returned in
 * a single read. This preserves the original \n in the string,
 * and does NOT terminate it with a \0. Does not block; caller
 * must check for -EAGAIN or -EINTR. Returns the number of
 * characters read (including \n, without \0).
 */
static int
read_line(char *buf, int buf_size)
{
    /* Used to buffer data from stdin */
    static int stdin_total = 0;
    static char stdin_buf[8192];

    /* How much space do we have left? */
    int remaining = sizeof(stdin_buf) - stdin_total;
    if (remaining == 0) {
        return -1;
    }

    /* Look for a newline */
    int lf;
    for (lf = 0; lf < stdin_total; ++lf) {
        if (stdin_buf[lf] == '\n') {
            break;
        }
    }

    /*
     * If we don't already have a full line buffered,
     * read some more data from stdin and retry.
     */
    if (lf == stdin_total) {
        int ret = read(stdin, &stdin_buf[stdin_total], remaining);
        if (ret <= 0) {
            return ret;
        }
        stdin_total += ret;
        return read_line(buf, buf_size);
    }

    /* Check if we have a full line but it won't fit in the buffer */
    if (lf >= buf_size) {
        return -1;
    }

    /* Copy up to and including newline */
    int consumed = lf + 1;
    memcpy(buf, stdin_buf, consumed);

    /* Shift remaining chars over */
    memmove(&stdin_buf[0], &stdin_buf[consumed], stdin_total - consumed);
    stdin_total -= consumed;
    return consumed;
}

/*
 * Reads a line from stdin. Blocks until a full
 * line is received. The returned string does not
 * contain the newline character.
 */
char *
gets(char *buf, int size)
{
    assert(buf != NULL);
    assert(size > 0);

    while (1) {
        int ret = read_line(buf, size);
        if (ret == -EAGAIN || ret == -EINTR) {
            continue;
        } else if (ret <= 0) {
            return NULL;
        } else {
            buf[ret - 1] = '\0';
            return buf;
        }
    }
}

/*
 * State for printf (and friends). Holds information about
 * the destination buffer and the current modifier flags.
 * write is a callback that is run when the buffer is
 * full, allowing for arbitrarily large strings. true_len
 * is the "actual" length that the string would be (even
 * if it didn't fit in the buffer).
 */
typedef struct {
    /* Static state */
    char *buf;
    int capacity;
    int fd;
    bool (*write)(int fd, const char *s, int len);

    /* Per-call dynamic state */
    int count;
    int true_len;
    bool error;

    /* Per-format dynamic state */
    int pad_width;
    bool left_align       : 1;
    bool positive_sign    : 1;
    bool space_sign       : 1;
    bool alternate_format : 1;
    bool pad_zeros        : 1;
} printf_arg_t;

/*
 * Userspace printf flush function: calls the write syscall.
 */
static bool
printf_write(int fd, const char *s, int len)
{
    return fwrite(s, len, fd) == len;
}

/*
 * Appends a string to the printf buffer. May also
 * flush the buffer, if it is full and a flush callback
 * is available.
 */
static bool
printf_append_string(printf_arg_t *a, const char *s)
{
    /* If we've already hit an error condition, fail fast */
    if (a->buf == NULL) {
        a->true_len += strlen(s);
        return false;
    }

    /* Try copying it into the buffer */
    int ret = strscpy(&a->buf[a->count], s, a->capacity - a->count);
    if (ret >= 0) {
        a->count += ret;
        a->true_len += ret;
        return true;
    }

    /* Try flushing buffer and restart */
    if (a->count > 0 && a->write != NULL) {
        if (!a->write(a->fd, a->buf, a->count)) {
            a->error = true;
            a->buf = NULL;
            return false;
        }
        a->count = 0;
        return printf_append_string(a, s);
    }

    /* String too long for buffer, bypass it if possible */
    if (a->count == 0 && a->write != NULL) {
        int len = strlen(s);
        a->true_len += len;
        if (!a->write(a->fd, s, len)) {
            a->error = true;
            a->buf = NULL;
            return false;
        }
        return true;
    }

    /* String too long and we have nowhere to flush it to */
    a->true_len += strlen(s);
    a->buf = NULL;
    return false;
}

/*
 * Appends a single character to the printf buffer.
 */
static bool
printf_append_char(printf_arg_t *a, char c)
{
    /* If we've already hit an error condition, fail fast */
    if (a->buf == NULL) {
        a->true_len++;
        return false;
    }

    /* Try copying it into the buffer */
    if (a->capacity - a->count > 1) {
        a->buf[a->count++] = c;
        a->buf[a->count] = '\0';
        a->true_len++;
        return true;
    }

    /* Try flushing buffer and restart */
    if (a->count > 0 && a->write != NULL) {
        if (!a->write(a->fd, a->buf, a->count)) {
            a->error = true;
            a->buf = NULL;
            return false;
        }
        a->count = 0;
        return printf_append_char(a, c);
    }

    /* Buffer is full and we have nowhere to flush it to */
    a->true_len++;
    a->buf = NULL;
    return false;
}

/*
 * Appends the specified number of characters (repeated)
 * to the printf buffer.
 */
static void
printf_pad(printf_arg_t *a, char pad, int width)
{
    while (width-- > 0) {
        printf_append_char(a, pad);
    }
}

/*
 * Handles the %s printf case.
 */
static void
printf_do_string(printf_arg_t *a, const char *s)
{
    if (a->left_align) {
        printf_append_string(a, s);
        printf_pad(a, ' ', a->pad_width - strlen(s));
    } else {
        printf_pad(a, ' ', a->pad_width - strlen(s));
        printf_append_string(a, s);
    }
}

/*
 * Handles the %c printf case.
 */
static void
printf_do_char(printf_arg_t *a, char c)
{
    if (a->left_align) {
        printf_append_char(a, c);
        printf_pad(a, ' ', a->pad_width - 1);
    } else {
        printf_pad(a, ' ', a->pad_width - 1);
        printf_append_char(a, c);
    }
}

/*
 * Converts a string to uppercase.
 */
static void
printf_stoupper(char *buf)
{
    while (*buf) {
        *buf = toupper(*buf);
        buf++;
    }
}

/*
 * Handles the %u, %x, and %o printf cases.
 */
static void
printf_do_uint(printf_arg_t *a, unsigned int num, int radix, bool upper)
{
    char utoa_buf[64];
    utoa(num, utoa_buf, radix);
    if (upper) {
        printf_stoupper(utoa_buf);
    }

    int pad_width = a->pad_width - strlen(utoa_buf);
    if (a->left_align) {
        printf_append_string(a, utoa_buf);
        printf_pad(a, ' ', pad_width);
    } else {
        printf_pad(a, a->pad_zeros ? '0' : ' ', pad_width);
        printf_append_string(a, utoa_buf);
    }
}

/*
 * Handles the %d and %i printf cases.
 */
static void
printf_do_int(printf_arg_t *a, int num, int radix, bool upper)
{
    char utoa_buf[64];
    utoa((num < 0) ? -num : num, utoa_buf, radix);
    if (upper) {
        printf_stoupper(utoa_buf);
    }

    /* What sign to print? */
    char sign_char = '\0';
    if (num < 0) {
        sign_char = '-';
    } else if (a->positive_sign) {
        sign_char = '+';
    } else if (a->space_sign) {
        sign_char = ' ';
    }

    /* Save one additional character for sign */
    int pad_width = a->pad_width - strlen(utoa_buf);
    if (sign_char != '\0') {
        pad_width--;
    }

    if (a->left_align) {
        if (sign_char != '\0') {
            printf_append_char(a, sign_char);
        }
        printf_append_string(a, utoa_buf);
        printf_pad(a, ' ', pad_width);
    } else {
        /*
         * If padding with zeros, print sign then padding,
         * otherwise print padding then sign.
         */
        if (a->pad_zeros) {
            if (sign_char != '\0') {
                printf_append_char(a, sign_char);
            }
            printf_pad(a, '0', pad_width);
        } else {
            printf_pad(a, ' ', pad_width);
            if (sign_char != '\0') {
                printf_append_char(a, sign_char);
            }
        }
        printf_append_string(a, utoa_buf);
    }
}

/*
 * printf common implementation. If write is not null,
 * the characters in the buffer are guaranteed to be
 * flushed before returning. Returns the "true length"
 * of the string, ignoring buffer overflow.
 */
static int
printf_impl(
    char *buf,
    int size,
    int fd,
    bool (*write)(int fd, const char *s, int len),
    const char *format,
    va_list args)
{
    assert(buf != NULL);
    assert(size > 0);
    assert(format != NULL);

    /* Ensure string is NUL-terminated */
    *buf = '\0';

    printf_arg_t a;
    a.buf = buf;
    a.capacity = size;
    a.fd = fd;
    a.write = write;
    a.count = 0;
    a.true_len = 0;
    a.error = false;

    for (; *format != '\0'; format++) {
        if (*format != '%') {
            printf_append_char(&a, *format);
            continue;
        }

        /* Whether we're currently reading a width format */
        bool in_width_format = false;

        /* Reset format flags */
        a.pad_width = 0;
        a.left_align = false;
        a.positive_sign = false;
        a.space_sign = false;
        a.alternate_format = false;
        a.pad_zeros = false;

consume_format:

        /* Consume a character */
        format++;

        /* Conversion specifiers */
        switch (*format) {

        /* Align left instead of right */
        case '-':
            a.left_align = true;
            goto consume_format;

        /* Prepend + if positive signed number */
        case '+':
            a.positive_sign = true;
            goto consume_format;

        /* Prepend space if positive signed number */
        case ' ':
            a.space_sign = true;
            goto consume_format;

        /* Use alternate formatting */
        case '#':
            a.alternate_format = true;
            goto consume_format;

        /* Padding width */
        case '0':
            /* If not in a width specifier, treat as flag */
            if (!in_width_format) {
                a.pad_zeros = true;
                goto consume_format;
            }

            /* Otherwise, fallthrough as a normal digit */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            in_width_format = true;
            a.pad_width = a.pad_width * 10 + (*format - '0');
            goto consume_format;

        /* Print a literal '%' character */
        case '%':
            printf_append_char(&a, '%');
            break;

        /* Print a number in lowercase hexadecimal form */
        case 'x':
            printf_do_uint(&a, va_arg(args, unsigned int), 16, false);
            break;

        /* Print a number in uppercase hexadecimal form */
        case 'X':
            printf_do_uint(&a, va_arg(args, unsigned int), 16, true);
            break;

        /* Print a number in unsigned decimal form */
        case 'u':
            printf_do_uint(&a, va_arg(args, unsigned int), 10, false);
            break;

        /* Print a number in signed decimal form */
        case 'd':
        case 'i':
            printf_do_int(&a, va_arg(args, int), 10, false);
            break;

        /* Print a number in octal form */
        case 'o':
            printf_do_uint(&a, va_arg(args, unsigned int), 8, false);
            break;

        /* Print a single character */
        case 'c':
            printf_do_char(&a, (char)va_arg(args, int));
            break;

        /* Print a NUL-terminated string */
        case 's':
            printf_do_string(&a, va_arg(args, const char *));
            break;

        /* Fail on other characters */
        default:
            abort();
            break;
        }
    }
    
    /* Flush any remaining characters */
    if (a.write != NULL && !a.write(a.fd, a.buf, a.count)) {
        a.error = true;
    }

    /* Return -1 if I/O error occurred, true length otherwise */
    if (a.error) {
        return -1;
    } else {
        return a.true_len;
    }
}

/*
 * Prints a string to a fixed-size buffer, va_list version.
 */
int
vsnprintf(char *buf, int size, const char *format, va_list args)
{
    return printf_impl(buf, size, -1, NULL, format, args);
}

/*
 * Prints a string to a fixed-size buffer.
 */
int
snprintf(char *buf, int size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, size, format, args);
    va_end(args);
    return ret;
}

/*
 * Prints a string to the specified output stream,
 * va_list version.
 */
int
vfprintf(int fd, const char *format, va_list args)
{
    char buf[256];
    return printf_impl(buf, sizeof(buf), fd, printf_write, format, args);
}

/*
 * Prints a string to the specified output stream.
 */
int
fprintf(int fd, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfprintf(fd, format, args);
    va_end(args);
    return ret;
}

/*
 * Prints a string to stdout, va_list version.
 */
int
vprintf(const char *format, va_list args)
{
    return vfprintf(stdout, format, args);
}

/*
 * Prints a string to stdout.
 */
int
printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}
