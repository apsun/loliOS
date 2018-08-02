#ifndef _DEBUG_H
#define _DEBUG_H

#include "types.h"
#include "lib.h"

#ifndef ASM

/* Whether to enable assertions */
#ifndef DEBUG_ASSERT
#define DEBUG_ASSERT 1
#endif

/* Whether to enable debugf printing */
#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif

/* Whether to BSOD on a panic */
#ifndef DEBUG_PANIC_BSOD
#define DEBUG_PANIC_BSOD 1
#endif

#if DEBUG_PANIC_BSOD

#define panic(msg) do {  \
    asm volatile("ud2"); \
} while (0)

#else /* DEBUG_PANIC_BSOD */

#define panic(msg) do {                                    \
    cli();                                                 \
    printf("%s:%d: Panic: %s\n", __FILE__, __LINE__, msg); \
    loop();                                                \
} while (0)

#endif /* DEBUG_PANIC_BSOD */

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
