#include "pipe.h"
#include "types.h"
#include "debug.h"
#include "math.h"
#include "list.h"
#include "myalloc.h"
#include "paging.h"
#include "file.h"
#include "wait.h"
#include "signal.h"

/*
 * How much storage to allocate for the kernel buffer.
 * This should be incremented by 1 to account for the
 * fact that one byte cannot be used in the circular queue.
 */
#define PIPE_SIZE 8193

/* Underlying pipe state */
typedef struct {
    int head;
    int tail;
    uint8_t buf[PIPE_SIZE];
    bool half_closed : 1;
    list_t read_queue;
    list_t write_queue;
} pipe_state_t;

/*
 * Determines the number of bytes that can be read from
 * this pipe. Returns 0 if the pipe write end is closed.
 * Returns -EAGAIN if there are no bytes, but the write
 * end is still open.
 */
static int
pipe_get_readable_count(pipe_state_t *pipe, int nbytes)
{
    /* If the head comes before the tail, it must wrap around */
    int head = pipe->head;
    if (head < pipe->tail) {
        head += PIPE_SIZE;
    }

    /* Have something to read immediately? */
    int to_read = min(nbytes, head - pipe->tail);
    if (to_read > 0) {
        return to_read;
    }

    /* If write end is closed, treat as EOF */
    if (pipe->half_closed) {
        return 0;
    }

    return -EAGAIN;
}

/*
 * read() syscall handler for pipe read endpoint. Drains
 * data from the pipe.
 */
static int
pipe_read(file_obj_t *file, void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    pipe_state_t *pipe = (pipe_state_t *)file->private;
    int to_read = WAIT_INTERRUPTIBLE(
        pipe_get_readable_count(pipe, nbytes),
        &pipe->read_queue,
        file->nonblocking);
    if (to_read <= 0) {
        return to_read;
    }

    /*
     * This loop will iterate at most two times: once from
     * the tail to the end of the buffer, and once from the
     * start of the buffer to the head. If the head and tail
     * are on the "same side", this will iterate only once.
     */
    int total_read = 0;
    char *bufp = buf;
    do {
        /* Read until the end of the buffer at most */
        int this_read = min(to_read, PIPE_SIZE - pipe->tail);

        /* Copy this chunk to userspace */
        if (!copy_to_user(&bufp[total_read], &pipe->buf[pipe->tail], this_read)) {
            debugf("Failed to copy data to userspace\n");
            break;
        }

        /* Advance counters */
        total_read += this_read;
        to_read -= this_read;
        pipe->tail = (pipe->tail + this_read) % PIPE_SIZE;
    } while (to_read > 0);

    /* Buffer should have some space now, wake writers */
    wait_queue_wake(&pipe->write_queue);

    /* Return number of bytes read (unless no copies succeeded) */
    if (total_read == 0) {
        return -1;
    } else {
        return total_read;
    }
}

/*
 * Returns the number of bytes that can be written to the
 * pipe. Returns -1 if the read half is closed. Returns
 * -EAGAIN if the pipe is full.
 */
static int
pipe_get_writable_count(pipe_state_t *pipe, int nbytes)
{
    /* If the reader is gone, writes should fail */
    if (pipe->half_closed) {
        debugf("Writing to half-duplex pipe\n");
        return -EPIPE;
    }

    /* If the head comes before the tail, it must wrap around */
    int tail = pipe->tail;
    if (tail <= pipe->head) {
        tail += PIPE_SIZE;
    }

    /* Have some space to write? */
    int to_write = min(nbytes, tail - 1 - pipe->head);
    if (to_write > 0) {
        return to_write;
    }

    return -EAGAIN;
}

/*
 * write() syscall handler for pipe write endpoint. Appends
 * data to the pipe.
 */
static int
pipe_write(file_obj_t *file, const void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    pipe_state_t *pipe = (pipe_state_t *)file->private;
    int to_write = WAIT_INTERRUPTIBLE(
        pipe_get_writable_count(pipe, nbytes),
        &pipe->write_queue,
        file->nonblocking);
    if (to_write < 0) {
        if (to_write == -EPIPE) {
            signal_raise_executing(SIGPIPE);
        }
        return to_write;
    }

    int total_write = 0;
    const char *bufp = buf;
    do {
        /* Read until the end of the buffer at most */
        int this_write = min(to_write, PIPE_SIZE - pipe->head);

        /* Copy this chunk to kernelspace */
        if (!copy_from_user(&pipe->buf[pipe->head], &bufp[total_write], this_write)) {
            debugf("Failed to copy data from userspace\n");
            break;
        }

        /* Advance counters */
        total_write += this_write;
        to_write -= this_write;
        pipe->head = (pipe->head + this_write) % PIPE_SIZE;
    } while (to_write > 0);

    /* Now that we have some data in the pipe, wake up readers */
    wait_queue_wake(&pipe->read_queue);

    if (total_write == 0) {
        return -1;
    } else {
        return total_write;
    }
}

/*
 * close() syscall handler for pipes. If the file refers to
 * the read end of the pipe, all further writes to the pipe
 * will fail. If it refers to the write end of the pipe, future
 * reads will return buffered data, then EOF when the buffer
 * is empty.
 */
static void
pipe_close(file_obj_t *file)
{
    if (file->private == 0) {
        return;
    }

    pipe_state_t *pipe = (pipe_state_t *)file->private;

    /*
     * If both ends are closed, release the underlying pipe.
     * Otherwise, just mark the pipe as half duplex so the
     * other end knows when to give up.
     */
    if (pipe->half_closed) {
        free(pipe);
    } else {
        pipe->half_closed = true;
        wait_queue_wake(&pipe->read_queue);
        wait_queue_wake(&pipe->write_queue);
    }
}

/* Combined read/write file ops for pipe files */
static const file_ops_t pipe_fops = {
    .read = pipe_read,
    .write = pipe_write,
    .close = pipe_close,
};

/*
 * pipe() syscall handler. Creates a new pipe, and writes the
 * descriptor of the read end to readfd, and the write end to
 * writefd.
 */
__cdecl int
pipe_pipe(int *readfd, int *writefd)
{
    int ret;
    pipe_state_t *pipe = NULL;
    file_obj_t *read_file = NULL;
    file_obj_t *write_file = NULL;
    int kreadfd = -1;
    int kwritefd = -1;
    file_obj_t **files = get_executing_files();

    /* Allocate pipe data */
    pipe = malloc(sizeof(pipe_state_t));
    if (pipe == NULL) {
        debugf("Cannot allocate space for pipe\n");
        ret = -1;
        goto error;
    }

    /* Initialize pipe */
    pipe->head = 0;
    pipe->tail = 0;
    pipe->half_closed = false;
    list_init(&pipe->read_queue);
    list_init(&pipe->write_queue);

    /* Create read endpoint */
    read_file = file_obj_alloc(&pipe_fops, OPEN_READ);
    if (read_file == NULL) {
        debugf("Cannot allocate pipe read endpoint\n");
        ret = -1;
        goto error;
    }

    /* Create write endpoint */
    write_file = file_obj_alloc(&pipe_fops, OPEN_WRITE);
    if (write_file == NULL) {
        debugf("Cannot allocate pipe write endpoint\n");
        ret = -1;
        goto error;
    }

    /* Bind read descriptor */
    kreadfd = file_desc_bind(files, -1, read_file);
    if (kreadfd < 0) {
        debugf("Cannot bind read descriptor\n");
        ret = -1;
        goto error;
    }

    /* Bind write descriptor */
    kwritefd = file_desc_bind(files, -1, write_file);
    if (kwritefd < 0) {
        debugf("Cannot bind write descriptor\n");
        ret = -1;
        goto error;
    }

    /* Copy descriptors to userspace */
    if (!copy_to_user(readfd, &kreadfd, sizeof(int)) ||
        !copy_to_user(writefd, &kwritefd, sizeof(int)))
    {
        debugf("Failed to copy descriptors to userspace\n");
        ret = -1;
        goto error;
    }

    read_file->private = (intptr_t)pipe;
    write_file->private = (intptr_t)pipe;
    ret = 0;

exit:
    if (write_file != NULL) {
        file_obj_release(write_file);
    }
    if (read_file != NULL) {
        file_obj_release(read_file);
    }
    return ret;

error:
    if (kwritefd >= 0) {
        file_desc_unbind(files, kwritefd);
    }
    if (kreadfd >= 0) {
        file_desc_unbind(files, kreadfd);
    }
    if (pipe != NULL) {
        free(pipe);
    }
    goto exit;
}
