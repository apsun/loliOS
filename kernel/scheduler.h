#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "types.h"
#include "list.h"
#include "process.h"
#include "signal.h"

#ifndef ASM

/*
 * Evaluates expr in a loop, waiting for it to return a value
 * other than -EAGAIN. The loop is terminated prematurely if
 * there are pending signals. If nonblocking is true, this is
 * the same as evaluating expr directly.
 */
#define BLOCKING_WAIT(expr, queue, nonblocking) ({              \
    int __ret;                                                  \
    pcb_t *__pcb = get_executing_pcb();                         \
    while (1) {                                                 \
        __ret = (expr);                                         \
        if (__ret != -EAGAIN || (nonblocking)) {                \
            break;                                              \
        }                                                       \
        if (signal_has_pending(__pcb->signals)) {               \
            __ret = -EINTR;                                     \
            break;                                              \
        }                                                       \
        scheduler_sleep(&(queue));                              \
    }                                                           \
    __ret;                                                      \
})

/* Yields the current process's timeslice */
__cdecl int scheduler_yield(void);

/* Permanently yields the current process's timeslice */
__noreturn void scheduler_exit(void);

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
