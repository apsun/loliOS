#include "mp1.h"
#include <attrib.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

static jmp_buf memcpy_env;

__cdecl static void
sigsegv_handler(int signum)
{
    sigmask(SIGSEGV, SIGMASK_UNBLOCK);
    longjmp(memcpy_env, 1);
}

__cdecl int
mp1_copy_to_user(void *dest, const void *src, int n)
{
    sigaction(SIGSEGV, sigsegv_handler);
    int ret;
    if (setjmp(memcpy_env) == 0) {
        memcpy(dest, src, n);
        ret = 0;
    } else {
        ret = n;
    }
    sigaction(SIGSEGV, NULL);
    return ret;
}

__cdecl int
mp1_copy_from_user(void *dest, const void *src, int n)
{
    return mp1_copy_to_user(dest, src, n);
}

__cdecl void *
mp1_malloc(size_t size)
{
    return malloc(size);
}

__cdecl void
mp1_free(void *ptr)
{
    free(ptr);
}
