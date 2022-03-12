/*
 * xoshiro128++ 1.0 based on the reference implementation
 * https://prng.di.unimi.it/xoshiro128plusplus.c
 */

/*
 * PRNG internal state, initialized as if srand(1) were called.
 */
static unsigned int xoshiro128pp_state[4] = {
    0x96a0f96bU,
    0x12bc8390U,
    0x971e9964U,
    0x79adc7e7U,
};

/*
 * Rotates x left by k bits.
 */
static unsigned int
rotl(unsigned int x, int k)
{
    return (x << k) | (x >> (32 - k));
}

/*
 * SplitMix generator based on fmix32 function from MurmurHash3.
 * Used to generate randomly distributed bits from the original seed.
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 */
static unsigned int
splitmix32(unsigned int x)
{
    x ^= x >> 16;
    x *= 0x85ebca6bU;
    x ^= x >> 13;
    x *= 0xc2b2ae35U;
    x ^= x >> 16;
    return x;
}

/*
 * Seeds the random number generator.
 */
void
srand(unsigned int seed)
{
    unsigned int *s = xoshiro128pp_state;
    s[0] = splitmix32(seed += 0x9e3779b9U);
    s[1] = splitmix32(seed += 0x9e3779b9U);
    s[2] = splitmix32(seed += 0x9e3779b9U);
    s[3] = splitmix32(seed += 0x9e3779b9U);
}

/*
 * Generates a random number in [0, 2^32).
 */
unsigned int
urand(void)
{
    unsigned int *s = xoshiro128pp_state;
    unsigned int r = rotl(s[0] + s[3], 7) + s[0];
    unsigned int t = s[1] << 9;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 11);
    return r;
}

/*
 * Generates a random number in [0, 2^31).
 */
int
rand(void)
{
    return (int)(urand() >> 1);
}
