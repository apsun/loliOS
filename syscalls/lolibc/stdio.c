#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

void
putc(char c)
{
    write(1, &c, 1);
}

void
puts(const char *s)
{
    assert(s != NULL);
    write(1, s, strlen(s));
    putc('\n');
}

char
getc(void)
{
    char c;
    read(0, &c, 1);
    return c;
}

char *
gets(char *buf, int32_t n)
{
    assert(buf != NULL);
    assert(n > 0);

    int32_t cnt = read(0, buf, n - 1);
    if (cnt > 0 && buf[cnt - 1] == '\n') {
        cnt--;
    }
    buf[cnt] = '\0';
    return buf;
}

typedef struct {
    char *buf;
    int32_t size;
} printf_buf_t;

typedef struct {
    int32_t pad_width;
    bool left_align;
    bool positive_sign;
    bool space_sign;
    bool alternate_format;
    bool pad_zeros;
} printf_arg_t;

static void
printf_append_string(printf_buf_t *b, const char *s)
{
    if (b->buf != NULL) {
        int32_t ret = strscpy(b->buf, s, b->size);
        if (ret >= 0) {
            b->buf += ret;
            b->size -= ret;
        } else {
            b->buf = NULL;
        }
    }
}

static void
printf_append_char(printf_buf_t *b, char c)
{
    if (b->buf != NULL) {
        if (b->size > 1) {
            b->buf[0] = c;
            b->buf[1] = '\0';
            b->buf++;
            b->size--;
        } else {
            b->buf = NULL;
        }
    }
}

static void
printf_pad(printf_buf_t *b, char pad, int32_t width)
{
    while (width-- > 0) {
        printf_append_char(b, pad);
    }
}

static void
printf_do_string(printf_buf_t *b, printf_arg_t *a, const char *s)
{
    if (a->left_align) {
        printf_append_string(b, s);
        printf_pad(b, ' ', a->pad_width - strlen(s));
    } else {
        printf_pad(b, ' ', a->pad_width - strlen(s));
        printf_append_string(b, s);
    }
}

static void
printf_do_char(printf_buf_t *b, printf_arg_t *a, char c)
{
    if (a->left_align) {
        printf_append_char(b, c);
        printf_pad(b, ' ', a->pad_width - 1);
    } else {
        printf_pad(b, ' ', a->pad_width - 1);
        printf_append_char(b, c);
    }
}

static void
printf_stoupper(char *buf)
{
    while (*buf) {
        *buf = toupper(*buf);
        buf++;
    }
}

static void
printf_do_uint(printf_buf_t *b, printf_arg_t *a, uint32_t num, int32_t radix, bool upper)
{
    char utoa_buf[64];
    utoa(num, utoa_buf, radix);
    if (upper) {
        printf_stoupper(utoa_buf);
    }

    int32_t pad_width = a->pad_width - strlen(utoa_buf);
    if (a->left_align) {
        printf_append_string(b, utoa_buf);
        printf_pad(b, ' ', pad_width);
    } else {
        printf_pad(b, a->pad_zeros ? '0' : ' ', pad_width);
        printf_append_string(b, utoa_buf);
    }
}

static void
printf_do_int(printf_buf_t *b, printf_arg_t *a, int32_t num, int32_t radix, bool upper)
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
    int32_t pad_width = a->pad_width - strlen(utoa_buf);
    if (sign_char != '\0') {
        pad_width--;
    }

    if (a->left_align) {
        if (sign_char != '\0') {
            printf_append_char(b, sign_char);
        }
        printf_append_string(b, utoa_buf);
        printf_pad(b, ' ', pad_width);
    } else {
        /*
         * If padding with zeros, print sign then padding,
         * otherwise print padding then sign.
         */
        if (a->pad_zeros) {
            if (sign_char != '\0') {
                printf_append_char(b, sign_char);
            }
            printf_pad(b, '0', pad_width);
        } else {
            printf_pad(b, ' ', pad_width);
            if (sign_char != '\0') {
                printf_append_char(b, sign_char);
            }
        }
        printf_append_string(b, utoa_buf);
    }
}

int32_t
vsnprintf(char *buf, int32_t size, const char *format, va_list args)
{
    assert(buf != NULL);
    assert(size > 0);
    assert(format != NULL);

    /* Put these together to make them more convenient to pass */
    printf_buf_t b;
    b.buf = buf;
    b.size = size;

    /* Ensure string is NUL-terminated */
    *buf = '\0';

    for (; *format != '\0'; format++) {
        if (*format != '%') {
            printf_append_char(&b, *format);
            continue;
        }

        /* Whether we're currently reading a width format */
        bool in_width_format = false;

        /* Conversion spec arguments */
        printf_arg_t a;

        /* Width of numbers with padding */
        a.pad_width = 0;

        /* Align numbers on the left instead of right? */
        a.left_align = false;

        /* Print the sign even if positive? */
        a.positive_sign = false;

        /* Insert space for sign if positive? */
        a.space_sign = false;

        /* Use alternate conversion format? */
        a.alternate_format = false;

        /* Pad numbers with zeros? */
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
            printf_append_char(&b, '%');
            break;

        /* Print a number in lowercase hexadecimal form */
        case 'x':
            printf_do_uint(&b, &a, va_arg(args, uint32_t), 16, false);
            break;

        /* Print a number in uppercase hexadecimal form */
        case 'X':
            printf_do_uint(&b, &a, va_arg(args, uint32_t), 16, true);
            break;

        /* Print a number in unsigned decimal form */
        case 'u':
            printf_do_uint(&b, &a, va_arg(args, uint32_t), 10, false);
            break;

        /* Print a number in signed decimal form */
        case 'd':
        case 'i':
            printf_do_int(&b, &a, va_arg(args, int32_t), 10, false);
            break;

        /* Print a number in octal form */
        case 'o':
            printf_do_uint(&b, &a, va_arg(args, uint32_t), 8, false);
            break;

        /* Print a single character */
        case 'c':
            printf_do_char(&b, &a, (char)va_arg(args, uint32_t));
            break;

        /* Print a NULL-terminated string */
        case 's':
            printf_do_string(&b, &a, va_arg(args, const char *));
            break;

        /* Fail fast on any other characters */
        default:
            abort();
            break;
        }
    }

    /* Return number of chars (excluding NUL) printed or -1 on error */
    if (b.buf == NULL) {
        return -1;
    } else {
        return size - b.size;
    }
}

int32_t
snprintf(char *buf, int32_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int32_t ret = vsnprintf(buf, size, format, args);
    va_end(args);
    return ret;
}

int32_t
vprintf(const char *format, va_list args)
{
    /*
     * Too lazy to write this properly, just pray that
     * nobody has strings longer than 4096 characters...
     */
    char buf[4096];
    int32_t len = vsnprintf(buf, sizeof(buf), format, args);
    if (len > 0) {
        write(1, buf, len);
    }
    return len;
}

int32_t
printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int32_t ret = vprintf(format, args);
    va_end(args);
    return ret;
}
