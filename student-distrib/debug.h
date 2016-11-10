#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef ASM

#include "lib.h"

/* Set to 1 for debug mode */
#define DEBUG 1

#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG

#define ASSERT(EXP)            \
do {                           \
    if(!(EXP)) {               \
        printf(__FILE__ ":%u: Assertion `" #EXP "\' failed.\n", __LINE__);  \
        loop();                \
    }                          \
} while(0)

#define debugf(...)            \
do {                           \
    printf(__FILE__ ":%u: ", __LINE__);    \
    printf(__VA_ARGS__);       \
} while(0)

#else

#define ASSERT(EXP)                  \
do {                                 \
    if (!(EXP)) {                    \
        *(volatile int *)0xdeaddead; \
    }                                \
} while(0)

#define debugf(...)            \
    while(0)

#endif

#endif
#endif /* _DEBUG_H */
