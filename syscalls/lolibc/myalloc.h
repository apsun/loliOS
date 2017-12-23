#ifndef _LOLIBC_MYALLOC_H
#define _LOLIBC_MYALLOC_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t num, size_t size);
void *realloc(void *ptr, size_t size);

#endif /* _LOLIBC_MYALLOC_H */
