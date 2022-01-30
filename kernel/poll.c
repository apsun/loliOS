#include "poll.h"
#include "types.h"
#include "process.h"
#include "wait.h"
#include "file.h"
#include "paging.h"
#include "scheduler.h"
#include "signal.h"
#include "pit.h"

/*
 * poll() syscall implementation.
 *
 * High level overview: we create two wait queue nodes for each file,
 * one for reads and one for writes. These nodes are passed to the
 * poll() handler for the file type, which will do two things:
 *
 * 1. Check if the file is ready to read/write
 * 2. Register the wait queue nodes with its internal wait queues
 *
 * If no files are ready, then we go to sleep and wait for either a
 * signal or a wait queue wakeup. When at least one file is ready,
 * we unregister all wait queue nodes and return.
 */
static int
poll_impl(pollfd_t *kpfds, int nfds, int timeout)
{
    int ret = 0;
    int i;
    pcb_t *pcb = get_executing_pcb();

    /* Allocate read and write wait queue nodes for each file */
    struct {
        wait_node_t read;
        wait_node_t write;
    } wait_nodes[MAX_FILES];
    for (i = 0; i < nfds; ++i) {
        wait_node_init(&wait_nodes[i].read, pcb);
        wait_node_init(&wait_nodes[i].write, pcb);
    }

    while (1) {
        for (i = 0; i < nfds; ++i) {
            pollfd_t *pfd = &kpfds[i];

            file_obj_t *file = get_executing_file(pfd->fd);
            if (file == NULL) {
                debugf("Attempting to poll invalid fd %d\n", pfd->fd);
                ret = -1;
                goto exit;
            }

            if (file->ops_table->poll == NULL) {
                debugf("Poll is not implemented for fd %d\n", pfd->fd);
                ret = -1;
                goto exit;
            }

            int events = pfd->events & OPEN_RDWR;
            if (events != pfd->events) {
                debugf("Invalid poll event bits set: %016b\n", pfd->events);
                ret = -1;
                goto exit;
            }

            /* Skip operations that we don't have permission to perform */
            events &= file->mode;

            /* Check for events and register in wait queues */
            wait_node_t *read_node = (events & OPEN_READ) ? &wait_nodes[i].read : NULL;
            wait_node_t *write_node = (events & OPEN_WRITE) ? &wait_nodes[i].write : NULL;
            pfd->revents = file->ops_table->poll(file, read_node, write_node) & file->mode;

            /* Return value = number of files with any event bits set */
            if (pfd->revents != 0) {
                ret++;
            }
        }

        /* Stop polling if any files have events or we've hit the timeout */
        if (ret > 0 || (timeout >= 0 && pit_monotime() >= timeout)) {
            break;
        }

        /* Bail out if we have a pending signal */
        if (signal_has_pending(pcb->signals)) {
            ret = -EINTR;
            goto exit;
        }

        /* Wait for one of the files or timeout to wake us */
        if (timeout >= 0) {
            scheduler_sleep_with_timeout(timeout);
        } else {
            scheduler_sleep();
        }
    }

exit:
    /* Remove all nodes from their wait queues */
    for (i = 0; i < nfds; ++i) {
        wait_queue_remove(&wait_nodes[i].read);
        wait_queue_remove(&wait_nodes[i].write);
    }

    return ret;
}

/*
 * poll() syscall handler. Waits for any of the input files
 * to be readable/writable, or until the given timeout (absolute
 * monotonic time, or < 0 for infinite). Returns the number of
 * files with events, or 0 if the poll timed out.
 *
 * If a file does not support a given operation, or the file
 * is opened without permissions to perform that operation,
 * it will be treated as if the operation was not specified.
 * Be warned, this may lead to deadlock!
 */
__cdecl int
poll_poll(pollfd_t *pfds, int nfds, int timeout)
{
    if (nfds <= 0 || nfds > MAX_FILES) {
        debugf("Invalid value for nfds: %d\n", nfds);
        return -1;
    }

    /* Copy pfds from userspace */
    pollfd_t kpfds[MAX_FILES];
    if (!copy_from_user(kpfds, pfds, nfds * sizeof(pollfd_t))) {
        return -1;
    }

    /* Do the actual poll logic with the kernel copy of pfds */
    int ret = poll_impl(kpfds, nfds, timeout);

    /* Copy pfds with revent fields set back to userspace */
    if (ret >= 0 && !copy_to_user(pfds, kpfds, nfds * sizeof(pollfd_t))) {
        return -1;
    }

    return ret;
}

/*
 * Generic poll() file op handler that always returns ready
 * for reads. Does not register for any wakeups.
 */
int
poll_generic_rdonly(file_obj_t *file, wait_node_t *readq, wait_node_t *writeq)
{
    return OPEN_READ;
}

/*
 * Generic poll() file op handler that always returns ready
 * for reads and writes. Does not register for any wakeups.
 */
int
poll_generic_rdwr(file_obj_t *file, wait_node_t *readq, wait_node_t *writeq)
{
    return OPEN_RDWR;
}
