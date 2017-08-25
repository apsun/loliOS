#ifndef _LOLIBC_STDLIB_H
#define _LOLIBC_STDLIB_H

#include <stdint.h>

void exit(int32_t status);
int32_t atexit(void (*fn)(void));
void abort(void);
int32_t rand(void);
void srand(int32_t seed);

#endif /* _LOLIBC_STDLIB_H */
