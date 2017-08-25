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
        if (ret < 0) {
            *dest = NULL;
        } else {
            *dest += ret;
            *size -= ret;
        }
    }
}

static void
strpcatc(char **dest, char c, int32_t *size)
{
    if (*dest != NULL && *size > 1) {
        **dest = c;
        (*dest)++;
        **dest = '\0';
        (*size)--;
    } else {
        *dest = NULL;
    }
}

int32_t
vsnprintf(char *buf, int32_t size, const char *format, va_list args)
{
    assert(buf != NULL);
    assert(size > 0);
    assert(format != NULL);

    /* itoa() buffer */
    char itoa_buf[64];

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

        /* Use alternate %x format? */
        bool alternate = false;

        /* Consume the % character */
        format++;

print_format:
        /* Conversion specifiers */
        switch (*format) {

        /* Print a literal '%' character */
        case '%':
            strpcatc(&new_buf, '%', &new_size);
            break;

        /* Use alternate formatting */
        case '#':
            alternate = true;
            format++;
            goto print_format;

        /* Print a number in hexadecimal form */
        case 'x':
            if (!alternate) {
                utoa(va_arg(args, uint32_t), itoa_buf, 16);
                strpcats(&new_buf, itoa_buf, &new_size);
            } else {
                utoa(va_arg(args, uint32_t), &itoa_buf[8], 16);
                int32_t starting_index;
                int32_t i;
                i = starting_index = strlen(&itoa_buf[8]);
                while (i < 8) {
                    itoa_buf[i] = '0';
                    i++;
                }
                strpcats(&new_buf, &itoa_buf[starting_index], &new_size);
            }
            break;

        /* Print a number in unsigned int form */
        case 'u':
            utoa(va_arg(args, uint32_t), itoa_buf, 10);
            strpcats(&new_buf, itoa_buf, &new_size);
            break;

        /* Print a number in signed int form */
        case 'd':
        case 'i':
            itoa(va_arg(args, int32_t), itoa_buf, 10);
            strpcats(&new_buf, itoa_buf, &new_size);
            break;

        /* Print a number in octal form */
        case 'o':
            utoa(va_arg(args, uint32_t), itoa_buf, 8);
            strpcats(&new_buf, itoa_buf, &new_size);
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
