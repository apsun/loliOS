#include "string.h"
#include "types.h"
#include "debug.h"

/*
 * Returns the length of the specified string.
 */
int
strlen(const char *s)
{
    assert(s != NULL);

    int len;
    for (len = 0; s[len]; ++len);
    return len;
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

    char *destp = dest;
    while ((*destp++ = *src++));
    return dest;
}

/*
 * Copies a string from src to dest. Returns a pointer
 * to the NUL terminator in dest.
 */
char *
stpcpy(char *dest, const char *src)
{
    assert(dest != NULL);
    assert(src != NULL);

    while ((*dest = *src)) {
        dest++;
        src++;
    }
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

    char *destp = dest;
    while (n-- && (*destp++ = *src++));
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

    char *destp = dest + strlen(dest);
    while ((*destp++ = *src++));
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

    char *destp = dest + strlen(dest);
    while (n && (*destp = *src)) {
        destp++;
        src++;
        n--;
    }
    *destp = '\0';
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
    for (; *haystack; ++haystack) {
        if (memcmp(haystack, needle, len) == 0) {
            return (char *)haystack;
        }
    }
    return NULL;
}

/*
 * Returns the number of characters in s before
 * the first occurrence of any character not in needle.
 */
int
strspn(const char *s, const char *needle)
{
    assert(s != NULL);
    assert(needle != NULL);

    int i, j;
    for (i = 0; s[i]; ++i) {
        for (j = 0; needle[j]; ++j) {
            if (s[i] == needle[j]) {
                goto next;
            }
        }
        break;
next:;
    }
    return i;
}

/*
 * Returns the number of characters in s before
 * the first occurrence of any character in needle.
 */
int
strcspn(const char *s, const char *needle)
{
    assert(s != NULL);
    assert(needle != NULL);

    int i, j;
    for (i = 0; s[i]; ++i) {
        for (j = 0; needle[j]; ++j) {
            if (s[i] == needle[j]) {
                goto done;
            }
        }
    }
done:
    return i;
}

/*
 * Finds the first occurrence of any characters
 * from the second string (needle) in the first (s).
 * Returns null if no characters were found.
 */
char *
strpbrk(const char *s, const char *needle)
{
    assert(s != NULL);
    assert(needle != NULL);

    s += strcspn(s, needle);
    return *s ? (char *)s : NULL;
}

/*
 * Finds the next occurrence of any character from delim
 * in s, and replaces it with a NUL character. Subsequent
 * calls to strtok() with s == NULL will start after the
 * the end of the string from the previous call. This will
 * skip consecutive delimiters.
 */
char *
strtok(char *s, const char *delim)
{
    assert(delim != NULL);

    static char *end;
    if (s == NULL) {
        s = end;
    }

    s += strspn(s, delim);
    if (!*s) {
        return NULL;
    }

    end = s + strcspn(s, delim);
    if (*end) {
        *end++ = '\0';
    }

    return s;
}

/*
 * Finds the next occurrence of any character from delim
 * in *sp, replaces it with a NUL character, and sets
 * *sp to the next character to search from. Unlike strok(),
 * this does not skip consecutive delimiters.
 */
char *
strsep(char **sp, const char *delim)
{
    assert(sp != NULL);
    assert(delim != NULL);

    char *s = *sp;
    if (s == NULL) {
        return NULL;
    }

    char *end = s + strcspn(s, delim);
    if (*end) {
        *end++ = '\0';
        *sp = end;
    } else {
        *sp = NULL;
    }

    return s;
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

    static const char *lookup = "0123456789abcdefghijklmnopqrstuvwxyz";
    char *bufp = buf;
    do {
        *bufp++ = lookup[value % radix];
        value /= radix;
    } while (value != 0);
    *bufp = '\0';
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

    if (value >= 0) {
        return utoa((unsigned int)value, buf, radix);
    }

    if (value != INT_MIN) {
        value = -value;
    }

    buf[0] = '-';
    utoa((unsigned int)value, &buf[1], radix);
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

    if (*str == '-') {
        sign = -1;
        str++;
    }

    for (; *str; ++str) {
        if (*str < '0' || *str > '9') {
            return 0;
        }
        res = res * 10 + (*str - '0');
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
    assert(n >= 0);

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

/*
 * Finds the first occurrence of the specified byte
 * within the given memory region. Returns null if
 * the byte was not found.
 */
void *
memchr(const void *s, unsigned char c, int n)
{
    assert(s != NULL);
    assert(n >= 0);

    const unsigned char *p = s;
    for (; n--; ++p) {
        if (*p == c) {
            return (void *)p;
        }
    }
    return NULL;
}

/*
 * Sets all bytes in the specified memory region to
 * the value of c. Returns s.
 */
void *
memset(void *s, unsigned char c, int n)
{
    assert(s != NULL);
    assert(n >= 0);

    /*
     * Empirical testing suggests 8B is fastest on QEMU, about
     * 50% faster than REP STOSL.
     */
    typedef struct {
        char bytes[8];
    } __packed word_t;

    /*
     * Pack c into a word for fast fill.
     */
    word_t word;
    size_t i;
    for (i = 0; i < sizeof(word.bytes); ++i) {
        word.bytes[i] = c;
    }

    /*
     * Align dest ptr to word boundary.
     */
    unsigned char *sb = s;
    int nalign = -(uintptr_t)sb & (sizeof(word_t) - 1);
    if (n >= nalign) {
        n -= nalign;
        while (nalign--) {
            *sb++ = c;
        }
    }

    /*
     * Do a fast word-by-word copy.
     */
    word_t *sw = (word_t *)sb;
    int nword = n / sizeof(word_t);
    while (nword--) {
        *sw++ = word;
    }

    /*
     * Handle trailing bytes at the end with a byte-by-byte copy.
     * The switch must handle up to sizeof(word_t) - 1.
     */
    sb = (unsigned char *)sw;
    int ntrailing = n & (sizeof(word_t) - 1);
    while (ntrailing--) {
        *sb++ = c;
    }

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
    assert(s != NULL);
    assert(n >= 0);

    asm volatile(
        "rep stosw"
        : "+D"(s), "+c"(n)
        : "a"(c)
        : "memory", "cc");

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
    assert(s != NULL);
    assert(n >= 0);

    asm volatile(
        "rep stosl"
        : "+D"(s), "+c"(n)
        : "a"(c)
        : "memory", "cc");

    return s;
}

/*
 * Copies a non-overlapping memory region from src to dest.
 * Returns dest.
 */
void *
memcpy(void *dest, const void *src, int n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    /*
     * Empirical testing suggests 8B is fastest on QEMU, about
     * twice the speed of REP MOVSL.
     */
    typedef struct {
        char bytes[8];
    } __packed word_t;

    /*
     * Align dest ptr to word boundary.
     */
    unsigned char *db = dest;
    const unsigned char *sb = src;
    int nalign = -(uintptr_t)db & (sizeof(word_t) - 1);
    if (n >= nalign) {
        n -= nalign;
        while (nalign--) {
            *db++ = *sb++;
        }
    }

    /*
     * Do a fast word-by-word copy.
     */
    word_t *dw = (word_t *)db;
    const word_t *sw = (const word_t *)sb;
    int nword = n / sizeof(word_t);
    while (nword--) {
        *dw++ = *sw++;
    }

    /*
     * Handle trailing bytes at the end with a byte-by-byte copy.
     */
    db = (unsigned char *)dw;
    sb = (const unsigned char *)sw;
    int ntrailing = n & (sizeof(word_t) - 1);
    while (ntrailing--) {
        *db++ = *sb++;
    }

    return dest;
}

/*
 * Copies a potentially overlapping memory region from
 * src to dest. Returns dest.
 */
void *
memmove(void *dest, const void *src, int n)
{
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d <= s || s + n <= d) {
        return memcpy(dest, src, n);
    } else {
        while (n--) {
            d[n] = s[n];
        }
    }

    return dest;
}

/*
 * Returns the number of trailing zeros in x.
 * x must not be zero.
 */
int
ctz(unsigned int x)
{
    assert(x != 0);

    int i;
    asm("bsfl %1, %0" : "=r"(i) : "g"(x) : "cc");
    return i;
}
