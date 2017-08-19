#include "lib.h"
#include "paging.h"
#include "terminal.h"

/*
 * Returns the length of the specified string.
 */
int32_t
strlen(const char *s)
{
    int32_t i = 0;
    while (s[i]) {
        i++;
    }
    return i;
}

/*
 * Compares two strings. Returns 0 if the two
 * strings are equal, and non-zero otherwise.
 */
int32_t
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
int32_t
strncmp(const char *s1, const char *s2, int32_t n)
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
strncpy(char *dest, const char *src, int32_t n)
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
    int32_t end = strlen(s) - 1;
    int32_t start = 0;
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
itoa(uint32_t value, char *buf, int32_t radix)
{
    const char *lookup = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *newbuf = buf;
    uint32_t newval = value;

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
atoi(const char *str, int32_t *result)
{
    int32_t res = 0;
    int32_t sign = 1;

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
memset(void *s, uint8_t c, int32_t n)
{
    asm volatile("              \n\
        .memset_top:            \n\
        testl   %%ecx, %%ecx    \n\
        jz      .memset_done    \n\
        testl   $0x3, %%edi     \n\
        jz      .memset_aligned \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        subl    $1, %%ecx       \n\
        jmp     .memset_top     \n\
        .memset_aligned:        \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        movl    %%ecx, %%edx    \n\
        shrl    $2, %%ecx       \n\
        andl    $0x3, %%edx     \n\
        cld                     \n\
        rep     stosl           \n\
        .memset_bottom:         \n\
        testl   %%edx, %%edx    \n\
        jz      .memset_done    \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        subl    $1, %%edx       \n\
        jmp     .memset_bottom  \n\
        .memset_done:           \n"
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
memset_word(void *s, uint16_t c, int32_t n)
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
memset_dword(void *s, uint32_t c, int32_t n)
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
memcpy(void *dest, const void *src, int32_t n)
{
    asm volatile("              \n\
        .memcpy_top:            \n\
        testl   %%ecx, %%ecx    \n\
        jz      .memcpy_done    \n\
        testl   $0x3, %%edi     \n\
        jz      .memcpy_aligned \n\
        movb    (%%esi), %%al   \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        addl    $1, %%esi       \n\
        subl    $1, %%ecx       \n\
        jmp     .memcpy_top     \n\
        .memcpy_aligned:        \n\
        movw    %%ds, %%dx      \n\
        movw    %%dx, %%es      \n\
        movl    %%ecx, %%edx    \n\
        shrl    $2, %%ecx       \n\
        andl    $0x3, %%edx     \n\
        cld                     \n\
        rep     movsl           \n\
        .memcpy_bottom:         \n\
        testl   %%edx, %%edx    \n\
        jz      .memcpy_done    \n\
        movb    (%%esi), %%al   \n\
        movb    %%al, (%%edi)   \n\
        addl    $1, %%edi       \n\
        addl    $1, %%esi       \n\
        subl    $1, %%edx       \n\
        jmp     .memcpy_bottom  \n\
        .memcpy_done:           \n"
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
memmove(void *dest, const void *src, int32_t n)
{
    asm volatile("                         \n\
        movw    %%ds, %%dx                 \n\
        movw    %%dx, %%es                 \n\
        cld                                \n\
        cmp     %%edi, %%esi               \n\
        jae     .memmove_go                \n\
        leal    -1(%%esi, %%ecx), %%esi    \n\
        leal    -1(%%edi, %%ecx), %%edi    \n\
        std                                \n\
        .memmove_go:                       \n\
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

        /* Print a number in hexadecimal form */
        case 'x':
            if (!alternate) {
                itoa(*esp, conv_buf, 16);
                puts(conv_buf);
            } else {
                itoa(*esp, &conv_buf[8], 16);
                int32_t starting_index;
                int32_t i;
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
            if (*(int32_t *)esp < 0) {
                conv_buf[0] = '-';
                itoa(-*(int32_t *)esp, &conv_buf[1], 10);
            } else {
                itoa(*(int32_t *)esp, conv_buf, 10);
            }
            puts(conv_buf);
            esp++;
            break;

        /* Print a single character */
        case 'c':
            putc((char)*esp);
            esp++;
            break;

        /* Print a NULL-terminated string */
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

/*
 * Checks whether a userspace string is readable, that
 * is, the entire string lies within user memory.
 */
bool
is_user_readable_string(const char *str)
{
    /* Ensure string starts inside the user page */
    if ((uint32_t)str < USER_PAGE_START) {
        return false;
    }

    int32_t i;
    for (i = 0; (uint32_t)str + i < USER_PAGE_END; ++i) {
        if (str[i] == '\0') {
            return true;
        }
    }

    /* Hit the end of the page w/o seeing a NUL terminator */
    return false;
}

/*
 * Checks whether a userspace buffer is readable, that
 * is, the address is valid and the entire buffer lies
 * within user memory.
 */
bool
is_user_readable(const void *user_buf, int32_t n)
{
    /* Buffer size must be non-negative */
    if (n < 0) {
        return false;
    }

    uint32_t start = (uint32_t)user_buf;
    uint32_t end = start + (uint32_t)n;

    /* Check for integer overflow */
    if (end < start) {
        return false;
    }

    /*
     * Buffer must start and end inside the user page.
     * This is kind of a hacky way to determine whether the
     * buffer is valid, but the only other alternative is
     * EAFP which is much worse.
     */
    if (start < USER_PAGE_START || end >= USER_PAGE_END) {
        return false;
    }

    return true;
}
/*
 * Checks whether a userspace buffer is writable, that
 * is, the address is valid and the entire buffer lies
 * within user memory, and the buffer is user-writable.
 */
bool
is_user_writable(const void *user_buf, int32_t n)
{
    /*
     * Everything is in one massive R/W/X page, so we don't
     * distingush readable and writable memory blocks
     */
    return is_user_readable(user_buf, n);
}

/*
 * Copies a string from userspace, with page boundary checking.
 * Returns true if the buffer was big enough and the source string
 * could be fully copied to the buffer. Returns false otherwise.
 * This does NOT pad excess chars in dest with zeros!
 */
bool
strncpy_from_user(char *dest, const char *src, int32_t n)
{
    /*
     * Make sure we start in the user page
     * (The upper bound check is in the loop)
     */
    if ((uint32_t)src < USER_PAGE_START) {
        return false;
    }

    int32_t i;
    for (i = 0; i < n; ++i) {
        /* Stop at the end of the user page */
        if ((uint32_t)(src + i) >= USER_PAGE_END) {
            return false;
        }

        /* Copy character, stop after reaching NUL terminator */
        if ((dest[i] = src[i]) == '\0') {
            return true;
        }
    }

    /* Didn't reach the terminator before n characters */
    return false;
}

/*
 * Copies a buffer from userspace to kernelspace, checking
 * that the source buffer is a valid userspace buffer. Returns
 * true if the entire buffer could be copied, and false otherwise.
 */
bool
copy_from_user(void *dest, const void *src, int32_t n)
{
    if (!is_user_readable(src, n)) {
        return false;
    }

    memcpy(dest, src, n);
    return true;
}

/*
 * Copies a buffer from kernelspace to userspace, checking
 * that the destination buffer is a valid userspace buffer. Returns
 * true if the entire buffer could be copied, and false otherwise.
 */
bool
copy_to_user(void *dest, const void *src, int32_t n)
{
    if (!is_user_writable(dest, n)) {
        return false;
    }

    memcpy(dest, src, n);
    return true;
}
