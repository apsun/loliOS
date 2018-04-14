#include <string.h>
#include <assert.h>
#include <stddef.h>

/*
 * Returns the length of the specified string.
 */
int
strlen(const char *s)
{
    assert(s != NULL);

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
    assert(s1 != NULL);
    assert(s2 != NULL);

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
    assert(s1 != NULL);
    assert(s2 != NULL);
    assert(n >= 0);

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
strcpy(char *dest, const char *src)
{
    assert(dest != NULL);
    assert(src != NULL);

    char *new_dest = dest;
    while ((*new_dest++ = *src++));
    return dest;
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
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    char *new_dest = dest;
    while (n-- && (*new_dest++ = *src++));
    return dest;
}

/*
 * Copies a string, up to n characters, from
 * src to dest. If n is reached before the NUL
 * terminator, dest is NUL-terminated and -1
 * is returned. Otherwise, the length of the
 * string is returned.
 */
int
strscpy(char *dest, const char *src, int n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n > 0);

    int i;
    for (i = 0; i < n; ++i) {
        if (!(dest[i] = src[i])) {
            return i;
        }
    }

    dest[i - 1] = '\0';
    return -1;
}

/*
 * Appends src to dest. Returns dest.
 */
char *
strcat(char *dest, const char *src)
{
    assert(dest != NULL);
    assert(src != NULL);

    char *new_dest = dest;
    while (*new_dest) new_dest++;
    while ((*new_dest++ = *src++));
    return dest;
}

/*
 * Appends up to n characters from src to dest.
 * The destination string is always NUL-terminated.
 */
char *
strncat(char *dest, const char *src, int n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n > 0);

    char *new_dest = dest;
    while (*new_dest) new_dest++;
    while ((*new_dest++ = *src++)) {
        if (--n == 0) {
            *new_dest = '\0';
            break;
        }
    }
    return dest;
}

/*
 * Reverses a string in-place. Returns the string.
 */
char *
strrev(char *s)
{
    assert(s != NULL);

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
 * Finds the first occurrence of the specified
 * character in the string. Returns null if the
 * character was not found.
 */
char *
strchr(const char *s, char c)
{
    assert(s != NULL);

    const char *ret = NULL;
    do {
        if (*s == c) {
            ret = s;
            break;
        }
    } while (*s++);
    return (char *)ret;
}

/*
 * Finds the last occurrence of the specified
 * character in the string. Returns null if the
 * character was not found.
 */
char *
strrchr(const char *s, char c)
{
    assert(s != NULL);

    const char *ret = NULL;
    do {
        if (*s == c) {
            ret = s;
        }
    } while (*s++);
    return (char *)ret;
}

/*
 * Finds the first occurrence of the specified
 * substring (needle) in the string (haystack).
 * Returns null if the substring was not found.
 */
char *
strstr(const char *haystack, const char *needle)
{
    assert(haystack != NULL);
    assert(needle != NULL);

    int len = strlen(needle);
    while (*haystack) {
        if (memcmp(haystack, needle, len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return NULL;
}

/*
 * Converts an unsigned integer to a string. The buffer
 * must be large enough to hold all the characters. The
 * radix can be any value between 2 and 36.
 */
char *
utoa(unsigned int value, char *buf, int radix)
{
    assert(buf != NULL);
    assert(radix >= 2 && radix <= 36);

    const char *lookup = "0123456789abcdefghijklmnopqrstuvwxyz";
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
 * Converts a signed integer to a string. The buffer
 * must be large enough to hold all the characters. The
 * radix can be any value between 2 and 36.
 */
char *
itoa(int value, char *buf, int radix)
{
    assert(buf != NULL);
    assert(radix >= 2 && radix <= 36);

    /* If it's already positive, no problem */
    if (value >= 0) {
        return utoa((unsigned int)value, buf, radix);
    }

    /* Okay, handle the sign separately */
    buf[0] = '-';
    utoa((unsigned int)-value, &buf[1], radix);
    return buf;
}

/*
 * Converts a string to an integer. If the string
 * is not a valid integer, returns 0. Only decimal
 * integers are recognized.
 */
int
atoi(const char *str)
{
    assert(str != NULL);

    int res = 0;
    int sign = 1;

    /* Negative sign check */
    if (*str == '-') {
        sign = -1;
        str++;
    }

    char c;
    while ((c = *str++)) {
        if (c < '0' || c > '9') {
            return 0;
        }
        res = res * 10 + (c - '0');
    }
    return res * sign;
}

/*
 * Compares two regions of memory. Returns 0 if
 * they are equal, and non-0 otherwise.
 */
int
memcmp(const void *s1, const void *s2, int n)
{
    assert(s1 != NULL);
    assert(s2 != NULL);

    const unsigned char *a = s1;
    const unsigned char *b = s2;
    while (n && (*a == *b)) {
        a++;
        b++;
        n--;
    }

    if (n == 0) {
        return 0;
    } else {
        return *a - *b;
    }
}

void *
memchr(const void *s, unsigned char c, int n)
{
    assert(s != NULL);
    assert(n >= 0);

    const unsigned char *p = s;
    while (n--) {
        if (*p == c) {
            return (void *)p;
        }
        p++;
    }
    return NULL;
}

void *
memset(void *s, unsigned char c, int n)
{
    assert(s != NULL);
    assert(n >= 0);

    unsigned char *p = s;
    while (n--) {
        *p++ = c;
    }
    return s;
}

void *
memcpy(void *dest, const void *src, int n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *
memmove(void *dest, const void *src, int n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    if (dest < src) {
        return memcpy(dest, src, n);
    }

    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        d[n] = s[n];
    }
    return dest;
}
