#include "mp1.h"
#include "mp1-vga.h"
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define MAX_BLINKS (SCREEN_WIDTH * SCREEN_HEIGHT)

static jmp_buf memcpy_env;
static blink_t malloc_buf[MAX_BLINKS];

static void
segv_handler(void)
{
    sigmask(SIG_SEGFAULT, SIGMASK_UNBLOCK);
    longjmp(memcpy_env, 1);
}

ASM_VISIBLE int32_t
mp1_copy_to_user(void *dest, const void *src, int32_t n)
{
    sigaction(SIG_SEGFAULT, segv_handler);
    int32_t ret;
    if (setjmp(memcpy_env) == 0) {
        memcpy(dest, src, n);
        ret = 0;
    } else {
        ret = n;
    }
    sigaction(SIG_SEGFAULT, NULL);
    return ret;
}

ASM_VISIBLE int32_t
mp1_copy_from_user(void *dest, const void *src, int32_t n)
{
    return mp1_copy_to_user(dest, src, n);
}

ASM_VISIBLE void *
mp1_malloc(int32_t size)
{
    assert(size == sizeof(blink_t));
    int32_t i;
    for (i = 0; i < MAX_BLINKS; ++i) {
        if (malloc_buf[i].location == 0) {
            return &malloc_buf[i];
        }
    }
    return NULL;
}

ASM_VISIBLE void
mp1_free(void *ptr)
{
    blink_t *m = ptr;
    m->location = 0;
}
