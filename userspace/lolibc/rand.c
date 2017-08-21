#include "rand.h"
#include "types.h"

static int32_t rand_state;

int32_t
rand(void)
{
    return rand_state = ((rand_state * 1103515245) + 12345) & 0x7fffffff;
}

void
srand(int32_t seed)
{
    rand_state = seed;
}
