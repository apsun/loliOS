#ifndef _LOLIBC_STDLIB_H
#define _LOLIBC_STDLIB_H

#include <myalloc.h>

void exit(int status);
int atexit(void (*fn)(void));
void abort(void);
int rand(void);
void srand(unsigned int seed);

#endif /* _LOLIBC_STDLIB_H */
