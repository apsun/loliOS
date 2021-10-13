#ifndef _LOLIBC_STDDEF_H
#define _LOLIBC_STDDEF_H

#define NULL 0

typedef int ssize_t;
typedef unsigned int size_t;
typedef int ptrdiff_t;

#define offsetof(type, member) \
    __builtin_offsetof(type, member)

#endif /* _LOLIBC_STDDEF_H */
