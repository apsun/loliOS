#include <stdio.h>
#include <assert.h>
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

static void
strpcats(char **dest, const char *src, int32_t *size)
{
    if (*dest != NULL) {
        int32_t ret = strscpy(*dest, src, *size);
        if (ret >= 0) {
            *dest += ret;
            *size -= ret;
        } else {
            *dest = NULL;
        }
    }
}

static void
strpcatc(char **dest, char c, int32_t *size)
{
    if (*dest != NULL) {
        if (*size > 1) {
            **dest = c;
            (*dest)++;
            **dest = '\0';
            (*size)--;
        } else {
            *dest = NULL;
        }
    }
}

static void
strppadc(char **dest, char pad, int32_t *size, int32_t width)
{
    while (width-- > 0) {
        strpcatc(dest, pad, size);
    }
}

static void
strpcatu(
    char **dest, uint32_t num, int32_t radix, int32_t *size,
    int32_t pad_width,
    bool left_align, /* - */
    bool pad_zeros)  /* 0 */
{
    char utoa_buf[64];
    utoa(num, utoa_buf, radix);

    pad_width -= strlen(utoa_buf);

    if (!left_align) {
        strppadc(dest, pad_zeros ? '0' : ' ', size, pad_width);
        strpcats(dest, utoa_buf, size);
    } else {
        strpcats(dest, utoa_buf, size);
        strppadc(dest, ' ', size, pad_width);
    }
}

static void
strpcatd(
    char **dest, int32_t num, int32_t radix, int32_t *size,
    int32_t pad_width,
    bool left_align,    /* - */
    bool positive_sign, /* + */
    bool space_sign,    /*   */
    bool pad_zeros)     /* 0 */
{
    char utoa_buf[64];
    utoa((num < 0) ? -num : num, utoa_buf, radix);

    /* What sign to print? */
    char sign_char = '\0';
    if (num < 0) {
        sign_char = '-';
    } else if (positive_sign) {
        sign_char = '+';
    } else if (space_sign) {
        sign_char = ' ';
    }

    /* Save one additional character for sign */
    pad_width -= strlen(utoa_buf);
    if (sign_char != '\0') {
        pad_width--;
    }

    if (!left_align) {
        /*
         * If padding with zeros, print sign then padding,
         * otherwise print padding then sign.
         */
        if (pad_zeros) {
            if (sign_char != '\0') {
                strpcatc(dest, sign_char, size);
            }
            strppadc(dest, '0', size, pad_width);
        } else {
            strppadc(dest, ' ', size, pad_width);
            if (sign_char != '\0') {
                strpcatc(dest, sign_char, size);
            }
        }
        strpcats(dest, utoa_buf, size);
    } else {
        if (sign_char != '\0') {
            strpcatc(dest, sign_char, size);
        }
        strpcats(dest, utoa_buf, size);
        strppadc(dest, ' ', size, pad_width);
    }
}

int32_t
vsnprintf(char *buf, int32_t size, const char *format, va_list args)
{
    assert(buf != NULL);
    assert(size > 0);
    assert(format != NULL);

    /* Where we currently are in buf (NULL if it's full) */
    char *new_buf = buf;

    /* How much space we have left in buf */
    int32_t new_size = size;

    /* Ensure nul-terminated just in case */
    *new_buf = '\0';

    for (; *format != '\0'; format++) {
        if (*format != '%') {
            strpcatc(&new_buf, *format, &new_size);
            continue;
        }

        /* Width of numbers with padding */
        int32_t pad_width = 0;

        /* Whether we're currently reading a width format */
        bool in_width_format = false;

        /* Align numbers on the left instead of right? */
        bool left_align = false;

        /* Print the sign even if positive? */
        bool positive_sign = false;

        /* Insert space for sign if positive? */
        bool space_sign = false;

        /* Use alternate conversion format? */
        bool alternate_format = false;

        /* Pad numbers with zeros? */
        bool pad_zeros = false;

consume_format:

        /* Consume a character */
        format++;

        /* Conversion specifiers */
        switch (*format) {
        /* Align left instead of right */
        case '-':
            left_align = true;
            goto consume_format;

        /* Prepend + if positive signed number */
        case '+':
            positive_sign = true;
            goto consume_format;

        /* Prepend space if positive signed number */
        case ' ':
            space_sign = true;
            goto consume_format;

        /* Use alternate formatting */
        case '#':
            alternate_format = true;
            goto consume_format;

        /* Padding width */
        case '0':
            /* If not in a width specifier, treat as flag */
            if (!in_width_format) {
                pad_zeros = true;
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
            pad_width = pad_width * 10 + (*format - '0');
            goto consume_format;

        /* Print a literal '%' character */
        case '%':
            strpcatc(&new_buf, '%', &new_size);
            break;

        /* Print a number in hexadecimal form */
        case 'x':
            strpcatu(&new_buf, va_arg(args, uint32_t), 16, &new_size,
                pad_width, left_align, pad_zeros);
            break;

        /* Print a number in unsigned decimal form */
        case 'u':
            strpcatu(&new_buf, va_arg(args, uint32_t), 10, &new_size,
                pad_width, left_align, pad_zeros);
            break;

        /* Print a number in signed decimal form */
        case 'd':
        case 'i':
            strpcatd(&new_buf, va_arg(args, int32_t), 10, &new_size,
                pad_width, left_align, positive_sign, space_sign, pad_zeros);
            break;

        /* Print a number in octal form */
        case 'o':
            strpcatu(&new_buf, va_arg(args, uint32_t), 8, &new_size,
                pad_width, left_align, pad_zeros);
            break;

        /* Print a single character */
        case 'c':
            strpcatc(&new_buf, (char)va_arg(args, uint32_t), &new_size);
            break;

        /* Print a NULL-terminated string */
        case 's':
            strpcats(&new_buf, va_arg(args, const char *), &new_size);
            break;

        /* Fail fast on any other characters */
        default:
            abort();
            break;
        }
    }

    /* Return number of chars (excluding NUL) printed */
    if (new_buf == NULL) {
        return -1;
    } else {
        return size - new_size;
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
