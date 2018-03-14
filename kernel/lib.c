#include "lib.h"
#include "terminal.h"

/*
 * Returns the length of the specified string.
 */
int
strlen(const char *s)
{
    const char *end = s;
    while (*end) end++;
    return end - s;
}

/*
 * Compares two strings. Returns 0 if the two
 * strings are equal, and non-zero otherwise.
 */
int
strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/*
 * Compares two strings, up to the specified
 * number of characters. Returns 0 if the two
 * strings are equal up to the specified number,
 * and non-zero otherwise.
 */
int
strncmp(const char *s1, const char *s2, int n)
{
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0) {
        return 0;
    } else {
        return *(unsigned char *)s1 - *(unsigned char *)s2;
    }
}

/*
 * Copies a string from src to dest. Returns dest.
 */
char *
strcpy(char* dest, const char *src)
{
    char *dest_orig = dest;
    while ((*dest++ = *src++));
    return dest_orig;
}

/*
 * Copies a string, up to n characters, from
 * src to dest. If n is reached before the NUL
 * terminator, dest is NOT NUL-terminated! Returns
 * dest.
 */
char *
strncpy(char *dest, const char *src, int n)
{
    char *dest_orig = dest;
    while (n-- && (*dest++ = *src++));
    return dest_orig;
}

/*
 * Reverses a string in-place. Returns the string.
 */
char *
strrev(char *s)
{
    int end = strlen(s) - 1;
    int start = 0;
    while (start < end) {
        char tmp = s[end];
        s[end] = s[start];
        s[start] = tmp;
        start++;
        end--;
    }
    return s;
}

/*
 * Converts an unsigned integer to a string using
 * the speicifed radix. buf must be large enough
 * to hold the entire string. Returns buf.
 */
char *
itoa(unsigned int value, char *buf, int radix)
{
    const char *lookup = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *newbuf = buf;
    unsigned int newval = value;

    /* Special case for zero */
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    /*
     * Go through the number one place value at a time, and add the
     * correct digit to "newbuf".  We actually add characters to the
     * ASCII string from lowest place value to highest, which is the
     * opposite of how the number should be printed.  We'll reverse the
     * characters later.
     */
    while (newval > 0) {
        *newbuf = lookup[newval % radix];
        newbuf++;
        newval /= radix;
    }

    /* Add a terminating NULL */
    *newbuf = '\0';

    /* Reverse the string and return */
    return strrev(buf);
}

/*
 * Converts a string to an integer. If the string
 * contains invalid characters, returns false.
 * Otherwise, *result is set to the integer value
 * and true is returned.
 */
bool
atoi(const char *str, int *result)
{
    int res = 0;
    int sign = 1;

    /* Check empty string (not a number) */
    if (*str == '\0') {
        return false;
    }

    /* Negative sign check */
    if (*str == '-') {
        sign = -1;
        str++;
    }

    while (*str != '\0') {
        char c = *str;
        if (c < '0' || c > '9') {
            return false;
        }

        res *= 10;
        res += (c - '0');
        str++;
    }

    *result = res * sign;
    return true;
}

/*
 * Sets all bytes in the specified memory region to
 * the value of c. Returns s.
 */
void *
memset(void *s, uint8_t c, int n)
{
    asm volatile("              \n\
        1:                      \n\
        testl   %%ecx, %%ecx    \n\
        jz      4f              \n\
        testl   $0x3, %%edi     \n\
        jz      2f              \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        subl    $1, %%ecx       \n\
        jmp     1b              \n\
        2:                      \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        movl    %%ecx, %%edx    \n\
        shrl    $2, %%ecx       \n\
        andl    $0x3, %%edx     \n\
        cld                     \n\
        rep     stosl           \n\
        3:                      \n\
        testl   %%edx, %%edx    \n\
        jz      4f              \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        subl    $1, %%edx       \n\
        jmp     3b              \n\
        4:                      \n"
        :
        : "a"(c << 24 | c << 16 | c << 8 | c), "D"(s), "c"(n)
        : "edx", "memory", "cc");

    return s;
}

/*
 * Sets all words in the specified memory region to
 * the value of c. The address must be word-aligned.
 * Returns s.
 */
void *
memset_word(void *s, uint16_t c, int n)
{
    asm volatile("              \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        cld                     \n\
        rep     stosw           \n"
        :
        : "a"(c), "D"(s), "c"(n)
        : "edx", "memory", "cc");

    return s;
}

/*
 * Sets all dwords in the specified memory region to
 * the value of c. The address must be dword-aligned.
 * Returns s.
 */
void *
memset_dword(void *s, uint32_t c, int n)
{
    asm volatile("              \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        cld                     \n\
        rep     stosl           \n"
        :
        : "a"(c), "D"(s), "c"(n)
        : "edx", "memory", "cc");

    return s;
}

/*
 * Copies a non-overlapping memory region from src to dest.
 * Returns dest.
 */
void *
memcpy(void *dest, const void *src, int n)
{
    asm volatile("              \n\
        1:                      \n\
        testl   %%ecx, %%ecx    \n\
        jz      4f              \n\
        testl   $0x3, %%edi     \n\
        jz      2f              \n\
        movb    (%%esi), %%al   \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        addl    $1, %%esi       \n\
        subl    $1, %%ecx       \n\
        jmp     1b              \n\
        2:                      \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        movl    %%ecx, %%edx    \n\
        shrl    $2, %%ecx       \n\
        andl    $0x3, %%edx     \n\
        cld                     \n\
        rep     movsl           \n\
        3:                      \n\
        testl   %%edx, %%edx    \n\
        jz      4f              \n\
        movb    (%%esi), %%al   \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        addl    $1, %%esi       \n\
        subl    $1, %%edx       \n\
        jmp     3b              \n\
        4:                      \n"
        :
        : "S"(src), "D"(dest), "c"(n)
        : "eax", "edx", "memory", "cc");

    return dest;
}

/*
 * Copies a potentially overlapping memory region from
 * src to dest. Returns dest.
 */
void *
memmove(void *dest, const void *src, int n)
{
    asm volatile("                         \n\
        movw    %%ds, %%dx                 \n\
        movw    %%dx, %%es                 \n\
        cld                                \n\
        cmp     %%edi, %%esi               \n\
        jae     1f                         \n\
        leal    -1(%%esi, %%ecx), %%esi    \n\
        leal    -1(%%edi, %%ecx), %%edi    \n\
        std                                \n\
        1:                                 \n\
        rep     movsb                      \n"
        :
        : "D"(dest), "S"(src), "c"(n)
        : "edx", "memory", "cc");

    return dest;
}

/*
 * Clears the terminal screen.
 */
void
clear(void)
{
    terminal_clear();
}

/*
 * Prints a character to the terminal.
 */
void
putc(char c)
{
    terminal_putc(c);
}

/*
 * Prints a string to the terminal.
 */
void
puts(const char *s)
{
    while (*s) {
        putc(*s++);
    }
}

/* Standard printf().
 * Only supports the following format strings:
 * %%  - print a literal '%' character
 * %x  - print a number in hexadecimal
 * %u  - print a number as an unsigned integer
 * %d  - print a number as a signed integer
 * %c  - print a character
 * %s  - print a string
 * %#x - print a number in 32-bit aligned hexadecimal, i.e.
 *       print 8 hexadecimal digits, zero-padded on the left.
 *       For example, the hex number "E" would be printed as
 *       "0000000E".
 *       Note: This is slightly different than the libc specification
 *       for the "#" modifier (this implementation doesn't add a "0x" at
 *       the beginning), but I think it's more flexible this way.
 *       Also note: %x is the only conversion specifier that can use
 *       the "#" modifier to alter output.
 */
void
printf(const char *format, ...)
{
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
        bool altbin = false;

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
            /* Yes, I know gotos are bad.  This is the
             * most elegant and general way to do this,
             * IMHO. */
            goto format_char_switch;

        case '*':
            altbin = true;
            format++;
            goto format_char_switch;

        /* Print a number in hexadecimal form */
        case 'x':
            if (!alternate && !altbin) {
                itoa(*esp, conv_buf, 16);
                puts(conv_buf);
            } else if (altbin) {
                itoa(*esp, &conv_buf[2], 16);
                int starting_index;
                int i;
                i = starting_index = strlen(&conv_buf[2]);
                while (i < 2) {
                    conv_buf[i] = '0';
                    i++;
                }
                puts(&conv_buf[starting_index]);
            } else {
                itoa(*esp, &conv_buf[8], 16);
                int starting_index;
                int i;
                i = starting_index = strlen(&conv_buf[8]);
                while (i < 8) {
                    conv_buf[i] = '0';
                    i++;
                }
                puts(&conv_buf[starting_index]);
            }
            esp++;
            break;

        /* Print a number in unsigned int form */
        case 'u':
            itoa(*esp, conv_buf, 10);
            puts(conv_buf);
            esp++;
            break;

        /* Print a number in signed int form */
        case 'd':
            if (*(int *)esp < 0) {
                conv_buf[0] = '-';
                itoa(-*(int *)esp, &conv_buf[1], 10);
            } else {
                itoa(*(int *)esp, conv_buf, 10);
            }
            puts(conv_buf);
            esp++;
            break;

        /* Print a single character */
        case 'c':
            putc((char)*esp);
            esp++;
            break;

        /* Print a NUL-terminated string */
        case 's':
            puts(*(char **)esp);
            esp++;
            break;

        /* Prevent reading past end if string ends with % */
        case '\0':
            format--;
            break;
        }
    }
}
