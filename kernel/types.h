#ifndef _TYPES_H
#define _TYPES_H

#define NULL 0
#define SIZE_MAX (~(size_t)0)

#ifndef ASM

#define __cdecl __attribute__((cdecl))
#define __used __attribute__((used))
#define __unused __attribute__((unused))
#define __noinline __attribute__((noinline))
#define __always_inline __attribute__((always_inline))
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))
#define __fallthrough __attribute__((fallthrough))

typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef long ssize_t;
typedef unsigned long size_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;
typedef long ptrdiff_t;
typedef enum { false, true } bool;

typedef char *va_list;
#define va_start(list, last) ((list) = (char *)(&(last) + 1))
#define va_arg(list, T) ((list) += sizeof(T), *(T *)((list) - sizeof(T)))
#define va_copy(dest, src) ((dest) = (src))
#define va_end(list) ((void)0)

/*
 * Returns the length of an array. Only works on actual
 * arrays, not pointers to arrays. Never use on function
 * arguments.
 */
#define array_len(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

/*
 * Returns the offset of a field inside a structure.
 */
#define offsetof(type, member) \
    ((size_t)&(((type *)NULL)->member))

/*
 * Returns a pointer to the parent structure of the
 * specified structure pointer.
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* ASM */

#endif /* _TYPES_H */
