#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

int32_t
strlen(const char *s)
{
    assert(s != NULL);

    const char *end = s;
    while (*end != '\0') {
        end++;
    }
    return end - s;
}

int32_t
strcmp(const char *s1, const char *s2)
{
    assert(s1 != NULL);
    assert(s2 != NULL);

    while (1) {
        unsigned char c1 = (unsigned char)*s1++;
        unsigned char c2 = (unsigned char)*s2++;
        int32_t delta = c1 - c2;
        if (delta != 0) {
            return delta;
        }
        if (c1 == '\0') {
            break;
        }
    }
    return 0;
}

int32_t
strncmp(const char *s1, const char *s2, int32_t n)
{
    assert(s1 != NULL);
    assert(s2 != NULL);
    assert(n >= 0);

    while (n--) {
        unsigned char c1 = (unsigned char)*s1++;
        unsigned char c2 = (unsigned char)*s2++;
        int32_t delta = c1 - c2;
        if (delta != 0) {
            return delta;
        }
        if (c1 == '\0') {
            break;
        }
    }
    return 0;
}

char *
strcpy(char *dest, const char *src)
{
    assert(dest != NULL);
    assert(src != NULL);

    char *new_dest = dest;
    while ((*new_dest++ = *src++));
    return dest;
}

char *
strncpy(char *dest, const char *src, int32_t n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    char *new_dest = dest;
    while (n-- && (*new_dest++ = *src++));
    return dest;
}

int32_t
strscpy(char *dest, const char *src, int32_t n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n > 0);

    int32_t i = 0;
    while (i < n) {
        if ((dest[i] = src[i]) == '\0') {
            return i;
        }
        i++;
    }
    dest[i - 1] = '\0';
    return -1;
}

char *
strcat(char *dest, const char *src)
{
    assert(dest != NULL);
    assert(src != NULL);

    char *new_dest = dest;
    while (*new_dest) {
        new_dest++;
    }
    while ((*new_dest++ = *src++));
    return dest;
}

char *
strncat(char *dest, const char *src, int32_t n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n > 0);

    char *new_dest = dest;
    while (*new_dest) {
        new_dest++;
    }
    while ((*new_dest++ = *src++)) {
        if (--n == 0) {
            *new_dest = '\0';
            break;
        }
    }
    return dest;
}

char *
strrev(char *s)
{
    assert(s != NULL);

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

char *
strstr(const char *haystack, const char *needle)
{
    assert(haystack != NULL);
    assert(needle != NULL);

    int32_t len = strlen(needle);
    while (*haystack) {
        if (memcmp(haystack, needle, len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return NULL;
}

char *
utoa(uint32_t value, char *buf, int32_t radix)
{
    assert(buf != NULL);
    assert(radix >= 2 && radix <= 36);

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

char *
itoa(int32_t value, char *buf, int32_t radix)
{
    /* If it's already positive, no problem */
    if (value >= 0) {
        return utoa((uint32_t)value, buf, radix);
    }

    /* Okay, handle the sign separately */
    buf[0] = '-';
    utoa((uint32_t)-value, &buf[1], radix);
    return buf;
}

int32_t
atoi(const char *str)
{
    assert(str != NULL);

    int32_t res = 0;
    int32_t sign = 1;

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

void *
memchr(const void *s, uint8_t c, int32_t n)
{
    assert(s != NULL);
    assert(n >= 0);

    const uint8_t *p = s;
    while (n--) {
        if (*p == c) {
            return (void *)p;
        }
        p++;
    }
    return NULL;
}

int32_t
memcmp(const void *s1, const void *s2, int32_t n)
{
    assert(s1 != NULL);
    assert(s2 != NULL);
    assert(n >= 0);

    const uint8_t *a = s1;
    const uint8_t *b = s2;
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
memset(void *s, uint8_t c, int32_t n)
{
    assert(s != NULL);
    assert(n >= 0);

    uint8_t *p = s;
    while (n--) {
        *p++ = c;
    }
    return s;
}

void *
memcpy(void *dest, const void *src, int32_t n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    uint8_t *d = dest;
    const uint8_t *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *
memmove(void *dest, const void *src, int32_t n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    if (dest < src) {
        return memcpy(dest, src, n);
    } else if (dest > src) {
        uint8_t *d = dest;
        const uint8_t *s = src;
        while (n--) {
            d[n] = s[n];
        }
    }
    return dest;
}
