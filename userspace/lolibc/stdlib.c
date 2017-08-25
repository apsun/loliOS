#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>

static void (*atexit_fns[32])(void);
static int32_t atexit_num = 0;
static int32_t rand_state = 0;

void
exit(int32_t status)
{
    int32_t i = atexit_num;
    while (i--) {
        atexit_fns[i]();
    }
    halt(status);
}

int32_t
atexit(void (*fn)(void))
{
    assert(fn != NULL);

    if (atexit_num < 32) {
        atexit_fns[atexit_num++] = fn;
        return 0;
    } else {
        return -1;
    }
}

void
abort(void)
{
    halt(1);
}

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
