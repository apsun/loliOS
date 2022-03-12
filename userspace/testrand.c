#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    srand(realtime());

    int i;
    int hi_buckets[16];
    int lo_buckets[16];
    for (i = 0; i < 10000000; ++i) {
        unsigned int x = urand();
        lo_buckets[x & 0xf]++;
        hi_buckets[x >> 28]++;
    }

    for (i = 0; i < 16; ++i) {
        printf("[%x] lo=%d, hi=%d\n", i, lo_buckets[i], hi_buckets[i]);
    }

    return 0;
}
