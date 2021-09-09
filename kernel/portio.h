#ifndef _PORTIO_H
#define _PORTIO_H

#include "types.h"

#ifndef ASM

/*
 * Reads a byte value from the specified I/O port.
 */
static inline uint8_t
inb(uint16_t port)
{
    uint8_t val;
    asm volatile(
        "inb (%w1), %b0"
        : "=a"(val)
        : "d"(port)
        : "memory");
    return val;
}

/*
 * Reads a 16-bit value from the specified I/O port.
 */
static inline uint16_t
inw(uint16_t port)
{
    uint16_t val;
    asm volatile(
        "inw (%w1), %w0"
        : "=a"(val)
        : "d"(port)
        : "memory");
    return val;
}

/*
 * Reads a 32-bit value from the specified I/O port.
 */
static inline uint32_t
inl(uint16_t port)
{
    uint32_t val;
    asm volatile(
        "inl (%w1), %0"
        : "=a"(val)
        : "d"(port)
        : "memory");
    return val;
}

/*
 * Reads n 32-bit values from the specified port.
 */
static inline void
rep_insl(uint32_t *data, int n, uint16_t port)
{
    asm volatile(
        "rep insl;"
        : 
        : "D"(data), "c"(n), "d"(port)
        : "memory");
}

/*
 * Writes a byte to the specified I/O port.
 */
static inline void
outb(uint8_t data, uint16_t port)
{
    asm volatile(
        "outb %b1, (%w0)"
        :
        : "d"(port), "a"(data)
        : "memory");
}

/*
 * Writes a 16-bit value to the specified I/O port.
 */
static inline void
outw(uint16_t data, uint16_t port)
{
    asm volatile(
        "outw %w1, (%w0)"
        :
        : "d"(port), "a"(data)
        : "memory");
}

/*
 * Writes a 32-bit value to the specified I/O port.
 */
static inline void
outl(uint32_t data, uint16_t port)
{
    asm volatile(
        "outl %1, (%w0)"
        :
        : "d"(port), "a"(data)
        : "memory");
}

/*
 * Writes n 32-bit values to the specified port.
 */
static inline void
rep_outsl(const uint32_t *data, int n, uint16_t port)
{
    asm volatile(
        "rep outsl;"
        :
        : "S"(data), "c"(n), "d"(port)
        : "memory");
}

#endif /* ASM */

#endif /* _PORTIO_H */
