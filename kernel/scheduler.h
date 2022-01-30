#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "types.h"
#include "process.h"

#ifndef ASM

/* Yields the current process's timeslice */
__cdecl int scheduler_yield(void);

/* Permanently yields the current process's timeslice */
__noreturn void scheduler_exit(void);

/* Adds/removes a process from the scheduler */
void scheduler_add(pcb_t *pcb);
void scheduler_remove(pcb_t *pcb);

/* Puts the currently executing process to sleep */
void scheduler_sleep(void);

/* Puts the currently executing process to sleep until a timeout */
void scheduler_sleep_with_timeout(int timeout);

/* Wakes the specified process */
void scheduler_wake(pcb_t *pcb);

/* Initializes the scheduler */
void scheduler_init(void);

#endif /* ASM */

#endif /* _SCHEDULER_H */
