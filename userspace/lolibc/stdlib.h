#ifndef _LOLIBC_STDLIB_H
#define _LOLIBC_STDLIB_H

#include <myalloc.h>
#include <mt19937.h>

int atexit(void (*fn)(void));
 __attribute__((noreturn)) void exit(int status);
 __attribute__((noreturn)) void abort(void);

#endif /* _LOLIBC_STDLIB_H */
