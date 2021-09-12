#ifndef _LOLIBC_STDINT_H
#define _LOLIBC_STDINT_H

#define SIZE_MAX 4294967295UL
#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647

typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;

#endif /* _LOLIBC_STDINT_H */
