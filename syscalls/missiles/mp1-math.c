#include "mp1-math.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

int32_t
abs(int32_t x)
{
    return x < 0 ? -x : x;
}

int32_t
sqrt(int32_t x)
{
    /* Algorithm from linux/lib/int_sqrt.c */
    assert(x >= 0);
    if (x <= 1) {
        return x;
    }

    int32_t y = 0;
    int32_t m = 1 << 30;
    while (m != 0) {
        int32_t b = y + m;
        y >>= 1;
        if (x >= b) {
            x -= b;
            y += m;
        }
        m >>= 2;
    }

    return y;
}

int32_t
clamp(int32_t x, int32_t min, int32_t max)
{
    return (x < min) ? min : (x > max) ? max : x;
}
