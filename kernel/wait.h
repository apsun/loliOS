#ifndef _WAIT_H
#define _WAIT_H

#include "types.h"
#include "debug.h"
#include "list.h"
#include "process.h"
#include "scheduler.h"
#include "signal.h"

#ifndef ASM

/*
 * Evaluates expr in a loop, waiting for it to return a value
 * other than -EAGAIN. The loop is terminated prematurely if
 * interruptible is true and there are pending signals. If
 * nonblocking is true, this is the same as evaluating expr
 * directly.
*/
#define WAIT_IMPL(expr, queue, nonblocking, interruptible) ({             \
    int __ret;                                                            \
    wait_node_t __wait;                                                   \
    wait_node_init(&__wait, get_executing_pcb());                         \
    wait_queue_add(&__wait, (queue));                                     \
    while (1) {                                                           \
        __ret = (expr);                                                   \
        if (__ret != -EAGAIN || (nonblocking)) {                          \
            break;                                                        \
        }                                                                 \
        if ((interruptible) && signal_has_pending(__wait.pcb->signals)) { \
            __ret = -EINTR;                                               \
            break;                                                        \
        }                                                                 \
        scheduler_sleep();                                                \
    }                                                                     \
    wait_queue_remove(&__wait);                                           \
    __ret;                                                                \
})

/*
 * Evaluates expr in a loop, waiting for it to return a value
 * other than -EAGAIN. The loop is terminated prematurely if
 * there are pending signals.
 */
#define WAIT_INTERRUPTIBLE(expr, queue, nonblocking) \
    WAIT_IMPL(expr, queue, nonblocking, true)

/*
 * Evaluates expr in a loop, waiting for it to return a value
 * other than -EAGAIN. The loop is not terminated prematurely
 * even if there are pending signals.
 */
#define WAIT_UNINTERRUPTIBLE(expr, queue, nonblocking) \
    WAIT_IMPL(expr, queue, nonblocking, false)

/*
 * Wait queue node. Contains a pointer to the process to be
 * woken up when the queue is notified.
 */
typedef struct wait_node {
    list_t list;
    pcb_t *pcb;
} wait_node_t;

/*
 * Initializes a wait queue node.
 */
static inline void
wait_node_init(wait_node_t *node, pcb_t *pcb)
{
    list_init(&node->list);
    node->pcb = pcb;
}

/*
 * Returns true iff the wait node is currently in a queue.
 */
static inline bool
wait_node_in_queue(wait_node_t *node)
{
    return !list_empty(&node->list);
}

/*
 * Adds a node to the specified wait queue. The node must
 * not already be in a wait queue.
 */
static inline void
wait_queue_add(wait_node_t *node, list_t *queue)
{
    assert(!wait_node_in_queue(node));
    list_add(&node->list, queue);
}

/*
 * Removes a node from its wait queue. No-op if the node
 * is not in a queue.
 */
static inline void
wait_queue_remove(wait_node_t *node)
{
    list_del(&node->list);
}

/*
 * Wakes all nodes in the specified wait queue. This does
 * NOT remove them from the queue.
 */
static inline void
wait_queue_wake(list_t *queue)
{
    list_t *pos, *next;
    list_for_each_safe(pos, next, queue) {
        wait_node_t *node = list_entry(pos, wait_node_t, list);
        scheduler_wake(node->pcb);
    }
}

#endif /* ASM */

#endif /* _WAIT_H */
