#include "io.h"
#include "types.h"
#include "sys.h"
#include "string.h"
#include "assert.h"

static void
writes(const char *s)
{
    assert(s != NULL);
    write(1, s, strlen(s));
}

void
putc(char c)
{
    write(1, &c, 1);
}

void
puts(const char *s)
{
    writes(s);
    putc('\n');
}

char
getc(void)
{
    char c;
    read(1, &c, 1);
    return c;
}

char *
gets(char *buf, int32_t n)
{
    assert(buf != NULL);
    assert(n > 0);

    int32_t cnt = read(1, buf, n - 1);
    if (cnt > 0 && buf[cnt - 1] == '\n') {
        cnt--;
    }
    buf[cnt] = '\0';
    return buf;
}

void
printf(const char *format, ...)
{
    assert(format != NULL);

    /* itoa() buffer */
    char conv_buf[64];

    /* Stack pointer for the other parameters */
    uint32_t *esp = (void *)&format;
    esp++;

    for (; *format != '\0'; format++) {
        if (*format != '%') {
            putc(*format);
            continue;
        }

        /* Use alternate %x format? */
        bool alternate = false;

        /* Consume the % character */
        format++;

format_char_switch:
        /* Conversion specifiers */
        switch (*format) {

        /* Print a literal '%' character */
        case '%':
            putc('%');
            break;

        /* Use alternate formatting */
        case '#':
            alternate = true;
            format++;
            goto format_char_switch;

        /* Print a number in hexadecimal form */
        case 'x':
            if (!alternate) {
                utoa(*(uint32_t *)esp, conv_buf, 16);
                writes(conv_buf);
            } else {
                utoa(*(uint32_t *)esp, &conv_buf[8], 16);
                int32_t starting_index;
                int32_t i;
                i = starting_index = strlen(&conv_buf[8]);
                while (i < 8) {
                    conv_buf[i] = '0';
                    i++;
                }
                writes(&conv_buf[starting_index]);
            }
            esp++;
            break;

        /* Print a number in unsigned int form */
        case 'u':
            utoa(*(uint32_t *)esp, conv_buf, 10);
            writes(conv_buf);
            esp++;
            break;

        /* Print a number in signed int form */
        case 'd':
        case 'i':
            itoa(*(int32_t *)esp, conv_buf, 10);
            writes(conv_buf);
            esp++;
            break;

        /* Print a number in octal form */
        case 'o':
            utoa(*(uint32_t *)esp, conv_buf, 8);
            writes(conv_buf);
            esp++;
            break;

        /* Print a single character */
        case 'c':
            putc(*(char *)esp);
            esp++;
            break;

        /* Print a NULL-terminated string */
        case 's':
            writes(*(char **)esp);
            esp++;
            break;

        /* Prevent reading past end if string ends with % */
        case '\0':
            format--;
            break;
        }
    }
}
