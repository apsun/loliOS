#ifndef _MYALLOC_H
#define _MYALLOC_H

#include "types.h"

#ifndef ASM

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t num, size_t size);
void *realloc(void *ptr, size_t size);

/* Prints a summary of the current heap state */
void mya_dump_state(void);

#endif /* ASM */

#endif /* _MYALLOC_H */
