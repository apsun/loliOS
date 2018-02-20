#include "mp1.h"
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static jmp_buf memcpy_env;

static void
segv_handler(void)
{
    sigmask(SIG_SEGFAULT, SIGMASK_UNBLOCK);
    longjmp(memcpy_env, 1);
}

ASM_VISIBLE int
mp1_copy_to_user(void *dest, const void *src, int n)
{
    sigaction(SIG_SEGFAULT, segv_handler);
    int ret;
    if (setjmp(memcpy_env) == 0) {
        memcpy(dest, src, n);
        ret = 0;
    } else {
        ret = n;
    }
    sigaction(SIG_SEGFAULT, NULL);
    return ret;
}

ASM_VISIBLE int
mp1_copy_from_user(void *dest, const void *src, int n)
{
    return mp1_copy_to_user(dest, src, n);
}

ASM_VISIBLE void *
mp1_malloc(size_t size)
{
    return malloc(size);
}

ASM_VISIBLE void
mp1_free(void *ptr)
{
    free(ptr);
}
