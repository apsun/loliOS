#include "lib.h"
#include "debug.h"
#include "terminal.h"

/* Whether to force rand() to always return zero */
#define DETERMINISTIC_RAND 0

/* PRNG state for rand() */
static unsigned int rand_state = 1;

/*
 * Checks whether the input is a lowercase
 * alphabetical character.
 */
bool
islower(char c)
{
    return c >= 'a' && c <= 'z';
}

/*
 * Checks whether the input is an uppercase
 * alphabetical character.
 */
bool
isupper(char c)
{
    return c >= 'A' && c <= 'Z';
}

/*
 * Checks whether the input is an alphabetical
 * character.
 */
bool
isalpha(char c)
{
    return islower(c) || isupper(c);
}

/*
 * Checks whether the input is a numerical
 * character.
 */
bool
isdigit(char c)
{
    return c >= '0' && c <= '9';
}

/*
 * Checks whether the input is either an alphabetical
 * or a numerical character.
 */
bool
isalnum(char c)
{
    return isalpha(c) || isdigit(c);
}

/*
 * Checks whether the input is a control character.
 */
bool
iscntrl(char c)
{
    return (c >= 0 && c <= 31) || c == 127;
}

/*
 * Checks whether the input is space or tab.
 */
bool
isblank(char c)
{
    return c == ' ' || c == '\t';
}

/*
 * Checks whether the input is whitespace.
 */
bool
isspace(char c)
{
    return isblank(c) || (c >= 10 && c <= 13);
}

/*
 * Checks whether the input is a printable (non-control)
 * character.
 */
bool
isprint(char c)
{
    return !iscntrl(c);
}

/*
 * Checks whether the input is a printable non-space
 * character.
 */
bool
isgraph(char c)
{
    return isprint(c) && c != ' ';
}

/*
 * Checks whether the input is a punctuation character.
 */
bool
ispunct(char c)
{
    return isgraph(c) && !isalnum(c);
}

/*
 * Checks whether the input is a hexadecimal character.
 */
bool
isxdigit(char c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/*
 * Converts a character to lowercase.
 */
char
tolower(char c)
{
    if (isupper(c)) {
        return c - 'A' + 'a';
    } else {
        return c;
    }
}

/*
 * Converts a character to uppercase.
 */
char
toupper(char c)
{
    if (islower(c)) {
        return c - 'a' + 'A';
    } else {
        return c;
    }
}

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
    typedef unsigned long long word_t;

    /*
     * Pack c into a word for fast fill.
     */
    word_t word = 0;
    if (c != 0) {
        word = \
            ((word_t)c << 56) | \
            ((word_t)c << 48) | \
            ((word_t)c << 40) | \
            ((word_t)c << 32) | \
            ((word_t)c << 24) | \
            ((word_t)c << 16) | \
            ((word_t)c << 8)  | \
            ((word_t)c << 0);
    }

    /*
     * Align dest ptr to word boundary. The switch must
     * handle up to alignof(word_t) - 1.
     */
    unsigned char *sb = s;
    int nalign = -(size_t)sb & (__alignof__(word_t) - 1);
    if (n >= nalign) {
        n -= nalign;
        switch (nalign) {
        case 7: *sb++ = c; __attribute__((fallthrough));
        case 6: *sb++ = c; __attribute__((fallthrough));
        case 5: *sb++ = c; __attribute__((fallthrough));
        case 4: *sb++ = c; __attribute__((fallthrough));
        case 3: *sb++ = c; __attribute__((fallthrough));
        case 2: *sb++ = c; __attribute__((fallthrough));
        case 1: *sb++ = c; __attribute__((fallthrough));
        case 0: break;
        default: panic("Unhandled memset alignment");
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
    switch (ntrailing) {
    case 7: *sb++ = c; __attribute__((fallthrough));
    case 6: *sb++ = c; __attribute__((fallthrough));
    case 5: *sb++ = c; __attribute__((fallthrough));
    case 4: *sb++ = c; __attribute__((fallthrough));
    case 3: *sb++ = c; __attribute__((fallthrough));
    case 2: *sb++ = c; __attribute__((fallthrough));
    case 1: *sb++ = c; __attribute__((fallthrough));
    case 0: break;
    default: panic("Unhandled memset size");
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

    asm volatile("              \n\
        rep     stosw           \n"
        : "+D"(s), "+c"(n)
        : "a"(c)
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
    assert(s != NULL);
    assert(n >= 0);

    asm volatile("              \n\
        rep     stosl           \n"
        : "+D"(s), "+c"(n)
        : "a"(c)
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
    assert(dest != NULL);
    assert(src != NULL);
    assert(n >= 0);

    /*
     * Empirical testing suggests 8B is fastest on QEMU, about
     * twice the speed of REP MOVSL.
     */
    typedef unsigned long long word_t;

    /*
     * Align dest ptr to word boundary. The switch must
     * handle up to alignof(word_t) - 1.
     */
    unsigned char *db = dest;
    const unsigned char *sb = src;
    int nalign = -(size_t)db & (__alignof__(word_t) - 1);
    if (n >= nalign) {
        n -= nalign;
        switch (nalign) {
        case 7: *db++ = *sb++; __attribute__((fallthrough));
        case 6: *db++ = *sb++; __attribute__((fallthrough));
        case 5: *db++ = *sb++; __attribute__((fallthrough));
        case 4: *db++ = *sb++; __attribute__((fallthrough));
        case 3: *db++ = *sb++; __attribute__((fallthrough));
        case 2: *db++ = *sb++; __attribute__((fallthrough));
        case 1: *db++ = *sb++; __attribute__((fallthrough));
        case 0: break;
        default: panic("Unhandled memcpy alignment");
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
     * The switch must handle up to sizeof(word_t) - 1.
     */
    db = (unsigned char *)dw;
    sb = (const unsigned char *)sw;
    int ntrailing = n & (sizeof(word_t) - 1);
    switch (ntrailing) {
    case 7: *db++ = *sb++; __attribute__((fallthrough));
    case 6: *db++ = *sb++; __attribute__((fallthrough));
    case 5: *db++ = *sb++; __attribute__((fallthrough));
    case 4: *db++ = *sb++; __attribute__((fallthrough));
    case 3: *db++ = *sb++; __attribute__((fallthrough));
    case 2: *db++ = *sb++; __attribute__((fallthrough));
    case 1: *db++ = *sb++; __attribute__((fallthrough));
    case 0: break;
    default: panic("Unhandled memcpy size");
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
    if (d < s || s + n <= d) {
        return memcpy(dest, src, n);
    } else {
        while (n--) {
            d[n] = s[n];
        }
    }

    return dest;
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
 * Kernel printf flush function: just dumps it to the
 * terminal.
 */
static bool
printf_write(printf_arg_t *a, const char *s, int len)
{
    terminal_puts(s);
    return true;
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

            /* Fallthrough */
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
            panic("Invalid printf() format specifier");
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
    return printf_impl(buf, size, NULL, format, args);
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
 * Prints a string to the terminal, va_list version.
 */
int
vprintf(const char *format, va_list args)
{
    char buf[256];
    return printf_impl(buf, sizeof(buf), printf_write, format, args);
}

/*
 * Prints a string to the terminal.
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

/*
 * Generates a random number.
 */
int
rand(void)
{
#if DETERMINISTIC_RAND
    return 0;
#else
    rand_state = rand_state * 1103515245 + 12345;
    return (rand_state >> 16) & 0x7fff;
#endif
}

/*
 * Seeds the random number generator.
 */
void
srand(unsigned int seed)
{
    rand_state = seed;
}
