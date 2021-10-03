#include "mt19937.h"

#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfU
#define UPPER_MASK 0x80000000U
#define LOWER_MASK 0x7fffffffU

static unsigned int state[N];
static int index = N + 1;

/*
 * Seeds the random number generator.
 */
void
srand(unsigned int seed)
{
    state[0] = seed;
    for (index = 1; index < N; ++index) {
        state[index] = (1812433253U * (state[index - 1] ^ (state[index - 1] >> 30)) + index);
    }
}

/*
 * Generates a random number in [0, 2^32).
 */
unsigned int
urand(void)
{
    if (index >= N) {
        if (index == N + 1) {
            srand(5489U);
        }

        int k;
        for (k = 0; k < N - M; ++k) {
            unsigned int y = (state[k] & UPPER_MASK) | (state[k + 1] & LOWER_MASK);
            state[k] = state[k + M] ^ (y >> 1) ^ ((y & 1) * MATRIX_A);
        }

        for (; k < N - 1; ++k) {
            unsigned int y = (state[k] & UPPER_MASK) | (state[k + 1] & LOWER_MASK);
            state[k] = state[k + (M - N)] ^ (y >> 1) ^ ((y & 1) * MATRIX_A);
        }

        unsigned int y = (state[N - 1] & UPPER_MASK) | (state[0] & LOWER_MASK);
        state[N - 1] = state[M - 1] ^ (y >> 1) ^ ((y & 1) * MATRIX_A);

        index = 0;
    }

    unsigned int y = state[index++];
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
