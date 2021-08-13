#ifndef _BITMAP_H
#define _BITMAP_H

#include "types.h"
#include "myalloc.h"

#ifndef ASM

/*
 * Bitmap unit type.
 */
typedef uint32_t bitmap_t;

/*
 * Returns the size of the parameter in bits.
 */
#define bitsizeof(x) \
    (8 * sizeof(x))

/*
 * Returns the major (unit) component of a bit index.
 */
#define bitmap_index(i) \
    ((i) / bitsizeof(bitmap_t))

/*
 * Returns the minor (bit) component of a bit index.
 */
#define bitmap_subindex(i) \
    ((i) % bitsizeof(bitmap_t))

/*
 * Returns the number of units needed to hold a n-bit bitmap.
 */
#define bitmap_nunits(nbits) \
    ((int)(((nbits) + bitsizeof(bitmap_t) - 1) / bitsizeof(bitmap_t)))

/*
 * Declares a new bitmap with the specified name and number of bits.
 */
#define bitmap_declare(name, nbits) \
    bitmap_t name[bitmap_nunits(nbits)]

/*
 * Dynamically allocates a new bitmap with the specified number of bits.
 */
#define bitmap_alloc(nbits) \
    calloc(bitmap_nunits(nbits), sizeof(bitmap_t))

/*
 * Reads the specified bit in the bitmap.
 */
#define bitmap_get(map, i) \
    (!!((map)[bitmap_index(i)] & (1 << bitmap_subindex(i))))

/*
 * Sets the specified bit in the bitmap.
 */
#define bitmap_set(map, i) \
    (map)[bitmap_index(i)] |= (1 << bitmap_subindex(i))

/*
 * Clears the specified bit in the bitmap.
 */
#define bitmap_clear(map, i) \
    (map)[bitmap_index(i)] &= ~(1 << bitmap_subindex(i))

/*
 * Finds the index of the first set bit in value.
 * Behavior is undefined if value is all zero bits.
 */
static inline int
bsfl(uint32_t value)
{
    int32_t i;
    asm volatile(
        "bsfl %1, %0"
        : "=r"(i)
        : "g"(value)
        : "cc");
    return i;
}

/*
 * Finds the index of the first '1' bit in the bitmap.
 * If there are no '1' bits, this will return an index
 * greater than or equal to the actual number of bits
 * in the bitmap.
 */
static inline int
bitmap_find_one(bitmap_t *map, int nbits)
{
    int i;
    for (i = 0; i < bitmap_nunits(nbits); ++i) {
        if (map[i] != 0) {
            return i * bitsizeof(bitmap_t) + bsfl(map[i]);
        }
    }
    return nbits;
}

/*
 * Finds the index of the first '0' bit in the bitmap.
 * If there are no '0' bits, this will return an index
 * greater than or equal to the actual number of bits
 * in the bitmap.
 */
static inline int
bitmap_find_zero(bitmap_t *map, int nbits)
{
    int i;
    for (i = 0; i < bitmap_nunits(nbits); ++i) {
        if (~map[i] != 0) {
            return i * bitsizeof(bitmap_t) + bsfl(~map[i]);
        }
    }
    return nbits;
}

#endif /* ASM */

#endif /* _BITMAP_H */
