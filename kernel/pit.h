#ifndef _PIT_H
#define _PIT_H

#include "types.h"

#ifndef ASM

/* Returns the current monotonic timestamp in nanoseconds */
int64_t pit_now(void);

/* Initializes the PIT */
void pit_init(void);

#endif /* ASM */

#endif /* _PIT_H */
