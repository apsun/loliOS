#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <syscall.h>

/*
 * Size of buffer allocated for readahead. This is allocated
 * separately from the file object itself, since not all files
 * need to be read from (and hence a read buffer is useless).
 */
#define FILE_BUFSIZE 1024

/* FILE wrapper for the standard streams */
FILE __stdin = {.fd = STDIN_FILENO};
FILE __stdout = {.fd = STDOUT_FILENO};
FILE __stderr = {.fd = STDERR_FILENO};

/*
 * Wraps an existing file descriptor into a FILE. When the
 * FILE is closed, the descriptor will also be closed.
 */
FILE *
fdopen(int fd)
{
    FILE *fp = malloc(sizeof(FILE));
    if (fp == NULL) {
        return NULL;
    }

    fp->fd = fd;
    fp->buf = NULL;
    fp->offset = 0;
    fp->count = 0;
    return fp;
}

/*
 * Opens a file with the specified mode. The only
 * supported modes are r, w, and rw. Binary and append
 * modes are not supported.
 */
FILE *
fopen(const char *name, const char *mode)
{
    assert(name != NULL);
    assert(mode != NULL);
    assert(*mode != '\0');

    int flags = 0;
    do {
        switch (*mode) {
        case 'r':
            flags |= OPEN_READ;
            break;
        case 'w':
            flags |= OPEN_WRITE;
            break;
        default:
            return NULL;
        }
    } while (*++mode != '\0');

    int fd = create(name, flags);
    if (fd < 0) {
        return NULL;
    }

    FILE *fp = fdopen(fd);
    if (fp == NULL) {
        close(fd);
        return NULL;
    }
    return fp;
}

/*
 * Wrapper around read() syscall. WARNING: This API is
 * intentionally incompatible with the libc API.
 */
int
fread(FILE *fp, void *buf, int size)
{
    assert(fp != NULL);
    assert(buf != NULL);
    assert(size >= 0);

    /* If we have buffered data, return that first */
    if (fp->offset < fp->count) {
        if (size > fp->count - fp->offset) {
            size = fp->count - fp->offset;
        }

        memcpy(buf, &fp->buf[fp->offset], size);
        fp->offset += size;
        return size;
    }

    return read(fp->fd, buf, size);
}

/*
 * Wrapper around write() syscall. WARNING: this API is
 * intentionally incompatible with the libc API.
 */
int
fwrite(FILE *fp, const void *buf, int size)
{
    assert(fp != NULL);
    assert(buf != NULL);
    assert(size >= 0);

    return write(fp->fd, buf, size);
}

/*
 * Closes an open file, releasing the underlying file
 * descriptor.
 */
int
fclose(FILE *fp)
{
    assert(fp != NULL);

    int ret = close(fp->fd);
    free(fp->buf);
    free(fp);
    return ret;
}

/*
 * Prints a single character to the specified file stream.
 * Returns < 0 on error, c on success.
 */
int
fputc(char c, FILE *fp)
{
    assert(fp != NULL);

    int ret;
    do {
        ret = fwrite(fp, &c, 1);
    } while (ret == -EAGAIN || ret == -EINTR);

    if (ret < 0) {
        return -1;
    } else {
        return (unsigned char)c;
    }
}

/*
 * Prints a string to the specified file stream. No newline
 * is appended to the output. Returns the number of characters
 * written on success, < 0 on error.
 */
int
fputs(const char *s, FILE *fp)
{
    assert(s != NULL);
    assert(fp != NULL);

    int len = strlen(s);
    int total = 0;
    while (total < len) {
        int ret = fwrite(fp, &s[total], len - total);
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
 * Prints a single character to stdout. Returns < 0 on
 * error, c on success.
 */
int
putchar(char c)
{
    return fputc(c, stdout);
}

/*
 * Prints a string followed by a newline to stdout. Returns
 * the number of characters written (including the newline)
 * on success, < 0 on error.
 */
int
puts(const char *s)
{
    assert(s != NULL);
    int len = fputs(s, stdout);
    if (len >= 0) {
        if (putchar('\n') >= 0) {
            return len + 1;
        }
    }
    return len;
}

/*
 * Fills the readahead buffer with more data. Blocks until
 * data is available. This will reset the offset and count
 * values to zero if they are equal.
 */
static int
fget_readahead(FILE *fp)
{
    /* Allocate buffer if not already done */
    if (fp->buf == NULL) {
        fp->buf = malloc(FILE_BUFSIZE);
        if (fp->buf == NULL) {
            return -1;
        }
    }

    /* Compress buffer if possible */
    if (fp->offset == fp->count) {
        fp->offset = 0;
        fp->count = 0;
    }

    /* Read more data into the buffer */
    int ret;
    do {
        ret = read(fp->fd, &fp->buf[fp->count], FILE_BUFSIZE - fp->count);
    } while (ret == -EINTR || ret == -EAGAIN);

    /* Advance head pointer */
    if (ret > 0) {
        fp->count += ret;
    }

    return ret;
}

/*
 * Reads a single character from the specified file stream.
 * Returns the character on success, < 0 on error or EOF.
 */
int
fgetc(FILE *fp)
{
    assert(fp != NULL);

    /* If readahead buffer is empty, read some more data */
    if (fp->offset == fp->count) {
        if (fget_readahead(fp) <= 0) {
            return -1;
        }
    }

    /*
     * Pop first character from buf. Make sure that character
     * is promoted as unsigned instead of char, in case
     * char is signed and the character is < 0.
     */
    return (unsigned char)fp->buf[fp->offset++];
}

/*
 * Reads a string from the specified file stream, until
 * a newline or NUL is found, or the buffer is full. The
 * string is always NUL-terminated. Examples of valid outputs:
 *
 * abc\n\0 (if size > 4)
 * abc\0 (if size >= 4)
 *
 * Returns buf on success, NULL on EOF or I/O error.
 */
char *
fgets(char *buf, int size, FILE *fp)
{
    assert(buf != NULL);
    assert(size > 0);
    assert(fp != NULL);

    bool read_lf = false;
    int total_read = 0;
    do {
        /*
         * If the internal buffer is empty, read some more data
         * from the file.
         */
        if (fp->offset == fp->count) {
            int ret = fget_readahead(fp);
            if (ret <= 0) {
                return (total_read == 0) ? NULL : buf;
            }
        }

        /*
         * Clamp number of bytes read to actual output size. The
         * - 1 is to compensate for the fact that we must always
         *  NUL-terminate the output buffer.
         */
        int max_size = fp->count - fp->offset;
        if (max_size > size - 1 - total_read) {
            max_size = size - 1 - total_read;
        }

        /* Find a newline in the internal buffer (or \0) */
        int len;
        for (len = 0; len < max_size; ++len) {
            char c = fp->buf[fp->offset + len];
            if (c == '\0' || c == '\n') {
                read_lf = true;
                len++;
                break;
            }
        }

        /* Copy from internal buffer to output buffer */
        memcpy(&buf[total_read], &fp->buf[fp->offset], len);
        buf[total_read + len] = '\0';
        fp->offset += len;
        total_read += len;
    } while (!read_lf && total_read < size - 1);

    return buf;
}

/*
 * Reads a single character from stdin. Returns the
 * character read on success, < 0 on error or EOF.
 */
int
getchar(void)
{
    return fgetc(stdin);
}

/*
 * Reads a line from stdin. Blocks until a full
 * line is received. The returned string does not
 * contain the newline character. Returns buf on
 * success, NULL on EOF or I/O error.
 */
char *
gets(char *buf, int size)
{
    assert(buf != NULL);
    assert(size > 0);

    /* Read from stdin */
    char *ret = fgets(buf, size, stdin);
    if (ret == NULL) {
        return NULL;
    }

    /* Chop off trailing newline if present */
    int len = strlen(ret);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    return ret;
}

/*
 * State for printf (and friends). Holds information about
 * the destination buffer and the current modifier flags.
 * write is a callback that is run when the buffer is
 * full, allowing for arbitrarily large strings. true_len
 * is the "actual" length that the string would be (even
 * if it didn't fit in the buffer).
 */
typedef struct printf_arg {
    /* Static state */
    char *buf;
    int capacity;
    FILE *fp;
    bool (*write)(struct printf_arg *a, const char *s, int len);

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
 * Userspace printf flush function: calls fputs().
 */
static bool
printf_write(printf_arg_t *a, const char *s, int len)
{
    return fputs(s, a->fp) >= 0;
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
        if (!a->write(a, a->buf, a->count)) {
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
        if (!a->write(a, s, len)) {
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
        if (!a->write(a, a->buf, a->count)) {
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
    FILE *fp,
    bool (*wr)(printf_arg_t *a, const char *s, int len),
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
    a.fp = fp;
    a.write = wr;
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
    if (a.write != NULL && !a.write(&a, a.buf, a.count)) {
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
    return printf_impl(buf, size, NULL, NULL, format, args);
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
vfprintf(FILE *fp, const char *format, va_list args)
{
    char buf[256];
    return printf_impl(buf, sizeof(buf), fp, printf_write, format, args);
}

/*
 * Prints a string to the specified output stream.
 */
int
fprintf(FILE *fp, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfprintf(fp, format, args);
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
