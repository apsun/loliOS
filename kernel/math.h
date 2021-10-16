#ifndef _MATH_H
#define _MATH_H

#ifndef ASM

/* Returns the greater of a, b */
#define max(a, b) \
    ((a) > (b) ? (a) : (b))

/* Returns the lesser of a, b */
#define min(a, b) \
    ((a) > (b) ? (b) : (a))

/* Clamps x to the range [lo, hi] */
#define clamp(x, lo, hi) \
    (min(max((x), (lo)), (hi)))

/* Returns the absolute value of x. UB if x is INT_MIN. */
#define abs(x) \
    ((x) < 0 ? -(x) : (x))

/* Returns true iff x is a power of 2 */
#define is_pow2(x) \
    (((x) & ((x) - 1)) == 0)

/*
 * Rounds x to the next multiple of mul. If x is already a multiple
 * of mul, the next multiple is returned. mul must be a power of 2.
 *
 * next_multiple_of(0, 4) == 4
 * next_multiple_of(1, 4) == 4
 * next_multiple_of(4, 4) == 8
 */
#define next_multiple_of(x, mul) \
    (((x) + (mul)) & -(mul))

/*
 * Rounds x to the next multiple of mul. Does nothing if x is
 * already a multiple of mul. mul must be a power of 2.
 *
 * round_up(0, 4) == 0
 * round_up(1, 4) == 4
 * round_up(4, 4) == 4
 */
#define round_up(x, mul) \
    (((x) + (mul) - 1) & -(mul))

/*
 * Rounds x to the previous multiple of mul. Does nothing if x is
 * already a multiple of mul. mul must be a power of 2.
 *
 * round_down(8, 4) == 8
 * round_down(7, 4) == 4
 * round_down(4, 4) == 4
 */
#define round_down(x, mul) \
    ((x) & ~((mul) - 1))

/*
 * Divides num by den, rounding the result upwards.
 *
 * div_round_up(4096, 4096) == 1
 * div_round_up(4097, 4096) == 2
 * div_round_up(8192, 4096) == 2
 */
#define div_round_up(num, den) \
    (((num) + (den) - 1) / (den))

#endif /* ASM */

#endif /* _MATH_H */
