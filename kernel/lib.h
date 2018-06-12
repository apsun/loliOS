#ifndef _LIB_H
#define _LIB_H

#include "types.h"

#ifndef ASM

/* va_arg macros - stdarg.h */
typedef char *va_list;
#define va_start(list, last) ((list) = (char *)(((uint32_t)(&(last) + 1) + 3) & ~3))
#define va_arg(list, T) ((list) += sizeof(T), *(T *)((list) - sizeof(T)))
#define va_copy(dest, src) ((dest) = (src))
#define va_end(list) ((void)0)

/* Character functions - ctype.h */
bool islower(char c);
bool isupper(char c);
bool isalpha(char c);
bool isdigit(char c);
bool isalnum(char c);
bool iscntrl(char c);
bool isblank(char c);
bool isspace(char c);
bool isprint(char c);
bool isgraph(char c);
bool ispunct(char c);
bool isxdigit(char c);
char tolower(char c);
char toupper(char c);

/* String functions - string.h */
int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int n);
int strscpy(char *dest, const char *src, int n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, int n);
char *strrev(char *s);
char *strchr(const char *s, char c);
char *strrchr(const char *s, char c);
char *strstr(const char *haystack, const char *needle);
char *utoa(unsigned int value, char *buf, int radix);
char *itoa(int value, char *buf, int radix);
int atoi(const char *str);

/* Memory functions - string.h */
int memcmp(const void *s1, const void *s2, int n);
void *memchr(const void *s, unsigned char c, int n);
void *memset(void *s, uint8_t c, int n);
void *memset_word(void *s, uint16_t c, int n);
void *memset_dword(void *s, uint32_t c, int n);
void *memcpy(void *dest, const void *src, int n);
void *memmove(void *dest, const void *src, int n);

/* Terminal functions - stdio.h */
int vsnprintf(char *buf, int size, const char *format, va_list args);
int snprintf(char *buf, int size, const char *format, ...);
int vprintf(const char *format, va_list args);
int printf(const char *format, ...);

/* Random number generation - stdlib.h */
int rand(void);
void srand(unsigned int seed);
/*
 * Reads a byte from the specified I/O port.
 */
static inline uint8_t
inb(uint16_t port)
{
    uint8_t val;
    asm volatile(
        "inb (%w1), %b0;"
        : "=a"(val)
        : "d"(port)
        : "memory");
    return val;
}

/*
 * Reads 2 bytes from 2 consecutive I/O ports.
 * The value is returned as a single 16-bit int.
 */
static inline uint16_t
inw(uint16_t port)
{
    uint16_t val;
    asm volatile(
        "inw (%w1), %w0;"
        : "=a"(val)
        : "d"(port)
        : "memory");
    return val;
}

/*
 * Reads 4 bytes from 4 consecutive I/O ports.
 * The value is returned as a single 32-bit int.
 */
static inline uint32_t
inl(uint16_t port)
{
    uint32_t val;
    asm volatile(
        "inl (%w1), %0;"
        : "=a"(val)
        : "d"(port)
        : "memory");
    return val;
}

/*
 * Swaps the endianness of a 16-bit value.
 */
static inline uint16_t
bswap16(uint16_t x)
{
    return (uint16_t)(
        (x & 0x00ff) << 8 |
        (x & 0xff00) >> 8);
}

/*
 * Swaps the endianness of a 32-bit value.
 */
static inline uint32_t
bswap32(uint32_t x)
{
    return (uint32_t)(
        (x & 0x000000ff) << 24 |
        (x & 0x0000ff00) << 8  |
        (x & 0x00ff0000) >> 8  |
        (x & 0xff000000) >> 24);
}

/*
 * Writes a byte to the specified I/O port.
 */
#define outb(data, port) do {  \
    asm volatile(              \
        "outb %b1, (%w0);"     \
        :                      \
        : "d"(port), "a"(data) \
        : "memory", "cc");     \
} while(0)

/*
 * Writes two bytes to two consecutive ports.
 */
#define outw(data, port) do {  \
    asm volatile(              \
        "outw %w1, (%w0);"     \
        :                      \
        : "d"(port), "a"(data) \
        : "memory", "cc");     \
} while(0)

/*
 * Writes four bytes to four consecutive ports
 */
#define outl(data, port) do {  \
    asm volatile(              \
        "outl %l1, (%w0);"     \
        :                      \
        : "d"(port), "a"(data) \
        : "memory", "cc");     \
} while(0)

/*
 * Clear interrupt flag - disables interrupts on this processor
 */
#define cli() do {         \
    asm volatile(          \
        "cli;"             \
        :                  \
        :                  \
        : "memory", "cc"); \
} while(0)

/*
 * Set interrupt flag - enable interrupts on this processor
 */
#define sti() do {         \
    asm volatile(          \
        "sti;"             \
        :                  \
        :                  \
        : "memory", "cc"); \
} while(0)

/*
 * Save flags and then clear interrupt flag
 * Saves the EFLAGS register into the variable "flags", and then
 * disables interrupts on this processor
 */
#define cli_and_save(flags) do { \
    asm volatile(                \
        "pushfl;"                \
        "popl %0;"               \
        "cli;"                   \
        : "=r"(flags)            \
        :                        \
        : "memory", "cc");       \
} while(0)

/*
 * Save flags and then set interrupt flag
 * Saves the EFLAGS register into the variable "flags", and then
 * enables interrupts on this processor
 */
#define sti_and_save(flags) do { \
    asm volatile(                \
        "pushfl;"                \
        "popl %0;"               \
        "sti;"                   \
        : "=r"(flags)            \
        :                        \
        : "memory", "cc");       \
} while(0)

/* Waits for an interrupt to occur */
#define hlt() do {        \
    asm volatile("hlt;"); \
} while(0)

/*
 * Puts the processor into an infinite loop. Interrupts may still
 * be received and handled (unless IF is cleared, of course).
 */
#define loop() do {    \
    asm volatile(      \
        "hlt;"         \
        "jmp . - 1;"); \
} while (0)

/*
 * Restore flags
 * Puts the value in "flags" into the EFLAGS register. Most often used
 * after a cli_and_save(flags)
 */
#define restore_flags(flags) do { \
    asm volatile(                 \
        "pushl %0;"               \
        "popfl;"                  \
        :                         \
        : "r"(flags)              \
        : "memory", "cc");        \
} while(0)

/*
 * Read the contents of the specified register
 * and stores it into dest.
 */
#define read_register(name, dest) do { \
    asm volatile(                      \
        "movl %%" name ", %0;"         \
        : "=m"(dest));                 \
} while (0)

/*
 * Returns the length of an array. Only works on actual
 * arrays, not pointers to arrays. Never use on function
 * arguments.
 */
#define array_len(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

/*
 * Returns the offset of a field inside a structure.
 */
#define offsetof(type, member) \
    ((size_t)&(((type *)NULL)->member))

/*
 * Returns a pointer to the parent structure of the
 * specified structure pointer.
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)ptr - offsetof(type, member)))

#endif /* ASM */

#endif /* _LIB_H */
