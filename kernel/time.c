#include "time.h"
#include "debug.h"
#include "rtc.h"
#include "pit.h"
#include "paging.h"
#include "process.h"

/*
 * Returns the number of seconds since the Unix epoch (UTC).
 */
time_t
realtime_now(void)
{
    return rtc_now();
}

/*
 * Returns the current time in nanoseconds of the system monotonic
 * clock.
 */
nanotime_t
monotime_now(void)
{
    return pit_now();
}

/*
 * Writes the current time in seconds of the system real time clock
 * to tp. This is the number of seconds since the Unix epoch (UTC).
 */
__cdecl int
time_realtime(time_t *tp)
{
    time_t now = realtime_now();
    if (!copy_to_user(tp, &now, sizeof(time_t))) {
        debugf("Invalid pointer passed to realtime()\n");
        return -1;
    }
    return 0;
}

/*
 * Writes the current time in nanoseconds of the system monotonic clock
 * to tp. This is the number of nanoseconds from an arbitrary point in
 * time, and thus the result is only valid when compared with the result
 * of another call to monotime(), or as an input to monosleep().
 */
__cdecl int
time_monotime(nanotime_t *tp)
{
    nanotime_t now = monotime_now();
    if (!copy_to_user(tp, &now, sizeof(nanotime_t))) {
        debugf("Invalid pointer passed to monotime()\n");
        return -1;
    }
    return 0;
}

/*
 * Sleeps until the specified monotonic clock time (in nanoseconds).
 * If tp is earlier than the current time, the call will immediately
 * return 0. The sleep may be interrupted, in which case -EINTR will
 * be returned and this can be called again with the same argument.
 * Otherwise, 0 will be returned to indicate a successful sleep.
 */
__cdecl int
time_monosleep(const nanotime_t *tp)
{
    nanotime_t when;
    if (!copy_from_user(&when, tp, sizeof(nanotime_t))) {
        debugf("Invalid pointer passed to monosleep()\n");
        return -1;
    }
    return process_sleep(when);
}
