#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "types.h"
#include "list.h"
#include "process.h"

#ifndef ASM

/* Yields the current process's timeslice */
__cdecl void scheduler_yield(void);

/* Permanently yields the current process's timeslice */
void scheduler_exit(void);

/* Adds/removes a process from the scheduler */
void scheduler_add(pcb_t *pcb);
void scheduler_remove(pcb_t *pcb);

/* Puts the currently executing process to sleep */
void scheduler_sleep(list_t *queue);

/* Wakes the specified process */
void scheduler_wake(pcb_t *pcb);

/* Wakes all processes in the specified queue */
void scheduler_wake_all(list_t *queue);

/* Initializes the scheduler */
void scheduler_init(void);

#endif /* ASM */

#endif /* _SCHEDULER_H */
