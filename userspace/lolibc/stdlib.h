#ifndef _LOLIBC_STDLIB_H
#define _LOLIBC_STDLIB_H

#include <myalloc.h>

#define __noreturn __attribute__((noreturn))

__noreturn void exit(int status);
int atexit(void (*fn)(void));
__noreturn void abort(void);
int rand(void);
void srand(unsigned int seed);

#endif /* _LOLIBC_STDLIB_H */
