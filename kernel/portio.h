#ifndef _PORTIO_H
#define _PORTIO_H

#include "types.h"

#ifndef ASM

/*
 * Reads a byte from the specified I/O port.
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
 * Reads 2 bytes from 2 consecutive I/O ports.
 * The value is returned as a single 16-bit int.
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
 * Reads 4 bytes from 4 consecutive I/O ports.
 * The value is returned as a single 32-bit int.
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
 * Writes two bytes to two consecutive ports.
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
 * Writes four bytes to four consecutive ports.
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

#endif /* ASM */

#endif /* _PORTIO_H */
