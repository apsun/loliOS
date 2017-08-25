#ifndef _LOLIBC_ASSERT_H
#define _LOLIBC_ASSERT_H

#ifndef NDEBUG

#include <stdlib.h>
#include <stdio.h>

#define assert(x) do {                                                   \
    if (!(x)) {                                                          \
        printf("%s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #x); \
        abort();                                                         \
    }                                                                    \
} while (0)

#else /* NDEBUG */

#define assert(x) ((void)0)

#endif /* NDEBUG */

#endif /* _LOLIBC_ASSERT_H */
