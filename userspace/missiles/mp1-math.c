#include "mp1-math.h"
#include <assert.h>
#include <stddef.h>

int
abs(int x)
{
    return x < 0 ? -x : x;
}

int
sqrt(int x)
{
    /* Algorithm from linux/lib/int_sqrt.c */
    assert(x >= 0);
    if (x <= 1) {
        return x;
    }

    int y = 0;
    int m = 1 << 30;
    while (m != 0) {
        int b = y + m;
        y >>= 1;
        if (x >= b) {
            x -= b;
            y += m;
        }
        m >>= 2;
    }

    return y;
}

int
clamp(int x, int min, int max)
{
    return (x < min) ? min : (x > max) ? max : x;
}
