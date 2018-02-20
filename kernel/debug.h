#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef ASM

#include "lib.h"

/* Whether to enable assertions */
#define DEBUG_ASSERT 1

/* Whether to enable debugf printing */
#define DEBUG_PRINT 0

/* Always-enabled panic macro */
#define PANIC(msg) do {                                    \
    cli();                                                 \
    printf("%s:%d: PANIC: %s\n", __FILE__, __LINE__, msg); \
    loop();                                                \
} while (0)

#if DEBUG_ASSERT

#define ASSERT(x) do {                  \
    if (!(x)) {                         \
        PANIC("Assertion failed: " #x); \
    }                                   \
} while(0)

#else /* DEBUG_ASSERT */

#define ASSERT(x) ((void)0)

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
