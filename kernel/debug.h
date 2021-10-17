#ifndef _DEBUG_H
#define _DEBUG_H

#include "printf.h"
#include "idt.h"

#ifndef ASM

/* Triggers a kernel panic */
#define panic(fmt, ...) \
    idt_panic("%s:%d: Panic: " fmt, __FILE__, __LINE__, ## __VA_ARGS__)

/* Whether to enable assertions */
#ifndef DEBUG_ASSERT
    #define DEBUG_ASSERT 1
#endif
#if DEBUG_ASSERT
    #define assert(x) do {                       \
        if (!(x)) {                              \
            panic("Assertion failed: %s\n", #x); \
        }                                        \
    } while(0)
#else
    #define assert(x) ((void)0)
#endif

/* Whether to enable debugf printing */
#ifndef DEBUG_PRINT
    #define DEBUG_PRINT 0
#endif
#if DEBUG_PRINT
    #define debugf(fmt, ...) \
        printf("%s:%d: " fmt, __FILE__, __LINE__, ## __VA_ARGS__)
#else
    #define debugf(...) ((void)0)
#endif

#endif /* ASM */

#endif /* _DEBUG_H */
