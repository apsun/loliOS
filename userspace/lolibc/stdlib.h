#ifndef _LOLIBC_STDLIB_H
#define _LOLIBC_STDLIB_H

#include <attrib.h>
#include <myalloc.h>
#include <mt19937.h>

int atexit(void (*fn)(void));
__noreturn void exit(int status);
__noreturn void abort(void);

#endif /* _LOLIBC_STDLIB_H */
