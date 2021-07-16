#ifndef _TIME_H
#define _TIME_H

#include "types.h"

#ifndef ASM

/* Seconds since some point in time */
typedef int64_t time_t;

/* Nanoseconds since some point in time */
typedef int64_t nanotime_t;

/* Helpers to convert to nanotime_t */
#define SECONDS(s) ((s) * 1000000000LL)
#define MILLISECONDS(ms) ((ms) * 1000000LL)
#define MICROSECONDS(us) ((us) * 1000LL)
#define NANOSECONDS(ns) ((ns) * 1LL)

/* Current time getters */
time_t realtime_now(void);
nanotime_t monotime_now(void);

/* Syscall handlers */
__cdecl int time_realtime(time_t *tp);
__cdecl int time_monotime(nanotime_t *tp);
__cdecl int time_monosleep(const nanotime_t *tp);

#endif /* ASM */

#endif /* _TIME_H */
