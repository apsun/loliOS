#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <syscall.h>

#define MAX_ATEXIT 32

static void (*atexit_fns[MAX_ATEXIT])(void);
static int atexit_count = 0;

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
 * Exits the program with the specified status code.
 * This will run any functions registered with atexit().
 */
__attribute__((noreturn)) void
exit(int status)
{
    int i = atexit_count;
    while (i--) {
        atexit_fns[i]();
    }
    halt(status);
}

/*
 * Aborts the program. This does not run any functions
 * registered with atexit().
 */
__attribute__((noreturn)) void
abort(void)
{
    halt(1);
}
