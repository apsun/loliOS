#include "mt19937.h"

#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfU
#define UPPER_MASK 0x80000000U
#define LOWER_MASK 0x7fffffffU

static unsigned int mt19937_state[N];
static int mt19937_index = N + 1;

/*
 * Seeds the random number generator.
 */
void
srand(unsigned int seed)
{
    unsigned int *s = mt19937_state;
    s[0] = seed;

    int i;
    for (i = 1; i < N; ++i) {
        s[i] = (1812433253U * (s[i - 1] ^ (s[i - 1] >> 30)) + i);
    }

    mt19937_index = i;
}

/*
 * Generates a random number in [0, 2^32).
 */
unsigned int
urand(void)
{
    unsigned int *s = mt19937_state;

    if (mt19937_index >= N) {
        if (mt19937_index == N + 1) {
            srand(5489U);
        }

        int k;
        for (k = 0; k < N - M; ++k) {
            unsigned int y = (s[k] & UPPER_MASK) | (s[k + 1] & LOWER_MASK);
            s[k] = s[k + M] ^ (y >> 1) ^ ((y & 1) * MATRIX_A);
        }

        for (; k < N - 1; ++k) {
            unsigned int y = (s[k] & UPPER_MASK) | (s[k + 1] & LOWER_MASK);
            s[k] = s[k + (M - N)] ^ (y >> 1) ^ ((y & 1) * MATRIX_A);
        }

        unsigned int y = (s[N - 1] & UPPER_MASK) | (s[0] & LOWER_MASK);
        s[N - 1] = s[M - 1] ^ (y >> 1) ^ ((y & 1) * MATRIX_A);

        mt19937_index = 0;
    }

    unsigned int y = s[mt19937_index++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680U;
    y ^= (y << 15) & 0xefc60000U;
    y ^= (y >> 18);
    return y;
}

/*
 * Generates a random number in [0, 2^31).
 */
int
rand(void)
{
    return (int)(urand() >> 1);
}
