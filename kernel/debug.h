#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef ASM

#include "lib.h"

/* Whether to enable assertions */
#ifndef DEBUG_ASSERT
#define DEBUG_ASSERT 1
#endif

/* Whether to enable debugf printing */
#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif

/* Unreachable panic macro */
#define unreachable() panic("Reached 'unreachable' code")

/* Always-enabled panic macro */
#define panic(msg) do {                                    \
    cli();                                                 \
    printf("%s:%d: Panic: %s\n", __FILE__, __LINE__, msg); \
    loop();                                                \
} while (0)

#if DEBUG_ASSERT

#define assert(x) do {                  \
    if (!(x)) {                         \
        panic("Assertion failed: " #x); \
    }                                   \
} while(0)

#else /* DEBUG_ASSERT */

#define assert(x) ((void)0)

#endif /* DEBUG_ASSERT */

#if DEBUG_PRINT

#define debugf(...) do {                   \
    printf("%s:%u: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__);                   \
} while(0)

#else /* DEBUG_PRINT */

#define debugf(...) ((void)0)

#endif /* DEBUG_PRINT */

#endif /* ASM */

#endif /* _DEBUG_H */
