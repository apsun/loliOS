#include "exit.h"
#include "types.h"
#include "sys.h"
#include "assert.h"

static void (*atexit_fns[32])(void);
static int32_t atexit_num = 0;

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
