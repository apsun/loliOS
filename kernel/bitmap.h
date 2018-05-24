#ifndef _BITMAP_H
#define _BITMAP_H

#ifndef ASM

/*
 * Returns the size of the parameter, in bits.
 */
#define bitsizeof(x) \
    (8 * sizeof(x))

/*
 * Declares a new bitmap with the specified name, element
 * type, and number of bits.
 */
#define bitmap_declare(name, type, n) \
    type name[(n + bitsizeof(type) - 1) / bitsizeof(type)]

/*
 * Reads the specified bit in the bitmap.
 */
#define bitmap_get(map, n) \
    (!!(map[n / bitsizeof(*map)] & (1 << (n % bitsizeof(*map)))))

/*
 * Sets the specified bit in the bitmap.
 */
#define bitmap_set(map, n) \
    map[n / bitsizeof(*map)] |= (1 << (n % bitsizeof(*map)))

/*
 * Clears the specified bit in the bitmap.
 */
#define bitmap_clear(map, n) \
    map[n / bitsizeof(*map)] &= ~(1 << (n % bitsizeof(*map)))

/*
 * Finds the index of the first '1' bit in the bitmap.
 * If there are no '1' bits, this will return an index
 * greater than or equal to the actual number of bits
 * in the bitmap.
 */
#define bitmap_find_one(map, result)     \
    asm volatile(                        \
        "xorl %%ecx, %%ecx;"             \
        "1:"                             \
        "movl (%1, %%ecx, 1), %%eax;"    \
        "testl %%eax, %%eax;"            \
        "jz 2f;"                         \
        "bsfl %%eax, %%eax;"             \
        "leal (%%eax, %%ecx, 8), %%eax;" \
        "jmp 3f;"                        \
        "2:"                             \
        "addl $4, %%ecx;"                \
        "cmpl %2, %%ecx;"                \
        "jl 1b;"                         \
        "movl %2 * 8, %%eax;"            \
        "3:"                             \
        : "=a"(result)                   \
        : "d"(map), "i"(sizeof(map))     \
        : "ecx")

/*
 * Finds the index of the first '0' bit in the bitmap.
 * If there are no '0' bits, this will return an index
 * greater than or equal to the actual number of bits
 * in the bitmap.
 */
#define bitmap_find_zero(map, result)    \
    asm volatile(                        \
        "xorl %%ecx, %%ecx;"             \
        "1:"                             \
        "movl (%1, %%ecx, 1), %%eax;"    \
        "notl %%eax;"                    \
        "testl %%eax, %%eax;"            \
        "jz 2f;"                         \
        "bsfl %%eax, %%eax;"             \
        "leal (%%eax, %%ecx, 8), %%eax;" \
        "jmp 3f;"                        \
        "2:"                             \
        "addl $4, %%ecx;"                \
        "cmpl %2, %%ecx;"                \
        "jl 1b;"                         \
        "movl %2 * 8, %%eax;"            \
        "3:"                             \
        : "=a"(result)                   \
        : "d"(map), "i"(sizeof(map))     \
        : "ecx")

#endif /* ASM */

#endif /* _BITMAP_H */
