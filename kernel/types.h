#ifndef _TYPES_H
#define _TYPES_H

#define NULL 0
#define SIZE_MAX (~(size_t)0)

#ifndef ASM

/* Commonly used attributes */
#define __cdecl __attribute__((cdecl))
#define __used __attribute__((used))
#define __unused __attribute__((unused))
#define __noinline __attribute__((noinline))
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))

typedef int int32_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef unsigned long size_t;
typedef enum { false, true } bool;

#endif /* ASM */

#endif /* _TYPES_H */
