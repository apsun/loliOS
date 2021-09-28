#ifndef _LOLIBC_STDDEF_H
#define _LOLIBC_STDDEF_H

#define NULL 0

typedef long ssize_t;
typedef unsigned long size_t;
typedef long ptrdiff_t;

#define offsetof(type, member) \
    __builtin_offsetof(type, member)

#endif /* _LOLIBC_STDDEF_H */
