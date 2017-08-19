#ifndef _LIB_H
#define _LIB_H

#include "types.h"

/* String functions */
int32_t strlen(const char *s);
int32_t strcmp(const char *s1, const char *s2);
int32_t strncmp(const char *s1, const char *s2, int32_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int32_t n);
char *strrev(char *s);
char *itoa(uint32_t value, char *buf, int32_t radix);
bool atoi(const char *value, int32_t *result);

/* Memory functions */
void *memset(void *s, uint8_t c, int32_t n);
void *memset_word(void *s, uint16_t c, int32_t n);
void *memset_dword(void *s, uint32_t c, int32_t n);
void *memcpy(void *dest, const void *src, int32_t n);
void *memmove(void *dest, const void *src, int32_t n);

/* Terminal functions */
void printf(const char *format, ...);
void putc(char c);
void puts(const char *s);
void clear(void);

/* User-kernel copy functions */
bool is_user_readable_string(const char *str);
bool is_user_readable(const void *user_buf, int32_t n);
bool is_user_writable(const void *user_buf, int32_t n);
bool strncpy_from_user(char *dest, const char *src, int32_t n);
bool copy_from_user(void *dest, const void *src, int32_t n);
bool copy_to_user(void *dest, const void *src, int32_t n);

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

/* Restore flags
 * Puts the value in "flags" into the EFLAGS register.  Most often used
 * after a cli_and_save_flags(flags) */
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

#endif /* _LIB_H */
