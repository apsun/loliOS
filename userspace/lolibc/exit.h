#ifndef _LOLIBC_EXIT_H
#define _LOLIBC_EXIT_H

#include "types.h"

void exit(int32_t status);
int32_t atexit(void (*fn)(void));
void abort(void);

#endif /* _LOLIBC_EXIT_H */
