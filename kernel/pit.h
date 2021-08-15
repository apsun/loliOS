#ifndef _PIT_H
#define _PIT_H

#include "types.h"

#ifndef ASM

/* Returns the current monotonic clock time in milliseconds */
__cdecl int pit_monotime(void);

/* Initializes the PIT */
void pit_init(void);

#endif /* ASM */

#endif /* _PIT_H */
