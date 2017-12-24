#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <syscall.h>

#define SMALL_SIZE_MIN 0
#define SMALL_SIZE_MAX 64
#define LARGE_SIZE_MIN 512
#define LARGE_SIZE_MAX 8192
#define ITERATION_COUNT 10000
#define RAND_RANGE(a, b) ((a) + rand() % ((b) - (a)))
#define RAND_SIZE() \
    ((rand() & 1) \
       ? RAND_RANGE(SMALL_SIZE_MIN, SMALL_SIZE_MAX) \
       : RAND_RANGE(LARGE_SIZE_MIN, LARGE_SIZE_MAX))

int
main(void)
{
    int i;

    /* sbrk correctness checks */
    assert(sbrk(-2147483647) < 0);
    assert(sbrk(-2147483647 - 1) < 0);
    assert(sbrk(2147483647) < 0);

    /* 0-sized allocation checks */
    (void)malloc(0);
    void *_ = realloc(NULL, 0);
    (void)_;
    (void)calloc(1, 0);

    /* Overflow checks */
    assert(malloc(SIZE_MAX) == NULL);
    assert(realloc(NULL, SIZE_MAX) == NULL);
    assert(calloc(1, SIZE_MAX) == NULL);
    assert(calloc(SIZE_MAX, SIZE_MAX) == NULL);

    /* Stuff to store our checks */
    char *ptrs[ITERATION_COUNT];
    int sizes[ITERATION_COUNT];
    char chrs[ITERATION_COUNT];

    /* I can haz randomness? */
    srand(time());

    /* malloc some randomly sized blocks */
    for (i = 0; i < ITERATION_COUNT; ++i) {
        size_t sz = RAND_SIZE();
        sizes[i] = 0;
        ptrs[i] = malloc(sz);
        if (ptrs[i] != NULL) {
            sizes[i] = sz;
            chrs[i] = (char)RAND_RANGE(0, 256);
            size_t j;
            for (j = 0; j < sz; ++j) {
                ptrs[i][j] = chrs[i];
            }
        }
    }

    /* free some of the pointers */
    for (i = 0; i < ITERATION_COUNT / 2; ++i) {
        int index = rand() % ITERATION_COUNT;
        free(ptrs[index]);
        ptrs[index] = NULL;
        sizes[index] = 0;
        chrs[index] = '\0';
    }

    /* realloc some of the pointers */
    for (i = 0; i < ITERATION_COUNT / 2; ++i) {
        int index = rand() % ITERATION_COUNT;
        size_t sz = RAND_SIZE();
        void *x = realloc(ptrs[index], sz);

        if (sz == 0 || x != NULL) {
            ptrs[index] = x;
            sizes[index] = sz;
            chrs[index] = (char)RAND_RANGE(0, 256);
            size_t j;
            for (j = 0; j < sz; ++j) {
                ptrs[index][j] = chrs[index];
            }
        }
    }

    /* Make sure our data is still intact */
    for (i = 0; i < ITERATION_COUNT; ++i) {
        int j;
        for (j = 0; j < sizes[i]; ++j) {
            assert(ptrs[i][j] == chrs[i]);
        }
    }

    /* Clean up our mess */
    for (i = 0; i < ITERATION_COUNT; ++i) {
        free(ptrs[i]);
        ptrs[i] = NULL;
        sizes[i] = 0;
        chrs[i] = '\0';
    }

    printf("All tests passed!\n");
    return 0;
}
