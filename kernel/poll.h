#ifndef _POLL_H
#define _POLL_H

#include "types.h"
#include "file.h"
#include "wait.h"

#ifndef ASM

/*
 * Implementation for POLL_READ and POLL_WRITE. Use one of those
 * helpers instead of calling this directly.
 */
#define POLL_IMPL(expr, queue, node, bit) ({ \
    int __ret = 0;                           \
    if ((node) != NULL) {                    \
        if (!wait_node_in_queue(node)) {     \
            wait_queue_add((node), (queue)); \
        }                                    \
        if ((expr) != -EAGAIN) {             \
            __ret |= (bit);                  \
        }                                    \
    }                                        \
    __ret;                                   \
})

/*
 * If expr evaluates to anything other than -EAGAIN, returns revents
 * with the read bit set. Also registers the node in the given wait
 * queue.
 */
#define POLL_READ(expr, queue, node) \
    POLL_IMPL(expr, queue, node, OPEN_READ)

/*
 * If expr evaluates to anything other than -EAGAIN, returns revents
 * with the write bit set. Also registers the node in the given wait
 * queue.
 */
#define POLL_WRITE(expr, queue, node) \
    POLL_IMPL(expr, queue, node, OPEN_WRITE)

/* Structure for poll() syscall */
typedef struct {
    int fd;
    short events;
    short revents;
} pollfd_t;

/* poll() syscall handler */
__cdecl int poll_poll(pollfd_t *pfd, int nfd);

#endif /* ASM */

#endif /* _POLL_H */
