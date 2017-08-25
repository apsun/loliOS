#include "mp1.h"
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define MAX_MISSILES 64

static jmp_buf memcpy_env;
static missile_t malloc_buf[MAX_MISSILES];

static void
segv_handler(void)
{
    sigmask(SIG_SEGFAULT, SIGMASK_UNBLOCK);
    longjmp(memcpy_env, 1);
}

__attribute__((cdecl)) int32_t
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

__attribute__((cdecl)) int32_t
mp1_copy_from_user(void *dest, const void *src, int32_t n)
{
    return mp1_copy_to_user(dest, src, n);
}

__attribute__((cdecl)) void *
mp1_malloc(int32_t size)
{
    assert(size == sizeof(missile_t));
    int32_t i;
    for (i = 0; i < MAX_MISSILES; ++i) {
        if (malloc_buf[i].c == '\0') {
            return &malloc_buf[i];
        }
    }
    return NULL;
}

__attribute__((cdecl)) void
mp1_free(void *ptr)
{
    missile_t *m = ptr;
    m->c = '\0';
}
