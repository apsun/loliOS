#include <assert.h>
#include <stddef.h>
#include <syscall.h>

#define MAX_ATEXIT 32

static void (*atexit_fns[MAX_ATEXIT])(void);
static int atexit_count = 0;
static unsigned int rand_state = 1;

/*
 * Exits the program with the specified status code.
 * This will run any functions registered with atexit().
 */
__noreturn void
exit(int status)
{
    int i = atexit_count;
    while (i--) {
        atexit_fns[i]();
    }
    halt(status);
}

/*
 * Registers a function to be called when the
 * program exits. Returns 0 on success, -1 on
 * failure.
 */
int
atexit(void (*fn)(void))
{
    assert(fn != NULL);

    if (atexit_count < MAX_ATEXIT) {
        atexit_fns[atexit_count++] = fn;
        return 0;
    } else {
        return -1;
    }
}

/*
 * Aborts the program. This does not run any functions
 * registered with atexit().
 */
__noreturn void
abort(void)
{
    halt(1);
}

/*
 * Generates a random number.
 */
int
rand(void)
{
    unsigned int tmp = rand_state;
    tmp *= 1103515245;
    tmp += 12345;
    tmp &= 0x7fffffff;
    return (int)(rand_state = tmp);
}

/*
 * Seeds the random number generator.
 */
void
srand(unsigned int seed)
{
    rand_state = seed;
}
