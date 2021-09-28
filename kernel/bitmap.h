#ifndef _BITMAP_H
#define _BITMAP_H

#include "types.h"
#include "string.h"
#include "myalloc.h"

#ifndef ASM

/*
 * Bitmap unit type.
 */
typedef unsigned int bitmap_t;

/*
 * Returns the size of the parameter in bits.
 */
#define bitmap_bitsizeof(x) \
    (8 * sizeof(x))

/*
 * Returns the major (unit) component of a bit index.
 */
#define bitmap_index(i) \
    ((i) / bitmap_bitsizeof(bitmap_t))

/*
 * Returns the minor (bit) component of a bit index.
 */
#define bitmap_subindex(i) \
    ((i) % bitmap_bitsizeof(bitmap_t))

/*
 * Returns the number of units needed to hold a n-bit bitmap.
 */
#define bitmap_nunits(nbits) \
    ((int)(((nbits) + bitmap_bitsizeof(bitmap_t) - 1) / bitmap_bitsizeof(bitmap_t)))

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
    (!!((map)[bitmap_index(i)] & (1U << bitmap_subindex(i))))

/*
 * Sets the specified bit in the bitmap.
 */
#define bitmap_set(map, i) \
    (map)[bitmap_index(i)] |= (1U << bitmap_subindex(i))

/*
 * Clears the specified bit in the bitmap.
 */
#define bitmap_clear(map, i) \
    (map)[bitmap_index(i)] &= ~(1U << bitmap_subindex(i))

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
            return i * bitmap_bitsizeof(bitmap_t) + ctz(map[i]);
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
            return i * bitmap_bitsizeof(bitmap_t) + ctz(~map[i]);
        }
    }
    return nbits;
}

#endif /* ASM */

#endif /* _BITMAP_H */
