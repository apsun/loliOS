#ifndef _LOLIBC_STDLIB_H
#define _LOLIBC_STDLIB_H

#include <myalloc.h>
#include <mt19937.h>

#define __noreturn __attribute__((noreturn))

__noreturn void exit(int status);
int atexit(void (*fn)(void));
__noreturn void abort(void);

#endif /* _LOLIBC_STDLIB_H */
