#include "pipe.h"
#include "lib.h"
#include "debug.h"
#include "myalloc.h"
#include "paging.h"
#include "file.h"

/* How much storage to allocate for the kernel buffer */
#define PIPE_SIZE 65536

/* Underlying pipe state */
typedef struct {
    int head;
    int tail;
    uint8_t buf[PIPE_SIZE];
    bool half_closed;
} pipe_state_t;

/*
 * read() syscall handler for pipe read endpoint. Drains
 * data from the pipe.
 */
static int
pipe_read(file_obj_t *file, void *buf, int nbytes)
{
    pipe_state_t *pipe = file->private;

    /* If the head comes before the tail, it must wrap around */
    int head = pipe->head;
    if (head < pipe->tail) {
        head += PIPE_SIZE;
    }

    /* Compute number of bytes left in the buffer */
    int to_read = nbytes;
    if (to_read > head - pipe->tail) {
        to_read = head - pipe->tail;
    }

    /* No bytes to read. If write end is closed, treat as EOF. */
    if (to_read == 0) {
        if (pipe->half_closed) {
            return 0;
        } else {
            return -EAGAIN;
        }
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
        int this_read = to_read;
        if (this_read > PIPE_SIZE - pipe->tail) {
            this_read = PIPE_SIZE - pipe->tail;
        }

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

    /* Return number of bytes read (unless no copies succeeded) */
    if (total_read == 0) {
        return -1;
    } else {
        return total_read;
    }
}

/*
 * write() syscall handler for pipe write endpoint. Appends
 * data to the pipe.
 */
static int
pipe_write(file_obj_t *file, const void *buf, int nbytes)
{
    pipe_state_t *pipe = file->private;

    /* If the reader is gone, writes should fail */
    if (pipe->half_closed) {
        return -1;
    }

    /* If the head comes before the tail, it must wrap around */
    int tail = pipe->tail;
    if (tail <= pipe->head) {
        tail += PIPE_SIZE;
    }

    /* Compute number of free bytes left in the buffer */
    int to_write = nbytes;
    if (to_write > tail - 1 - pipe->head) {
        to_write = tail - 1 - pipe->head;
    }

    /* If no more room in the buffer, fail fast */
    if (to_write == 0) {
        return -EAGAIN;
    }

    int total_write = 0;
    const char *bufp = buf;
    do {
        /* Read until the end of the buffer at most */
        int this_write = to_write;
        if (this_write > PIPE_SIZE - pipe->head) {
            this_write = PIPE_SIZE - pipe->head;
        }

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

    if (total_write == 0) {
        return -1;
    } else {
        return total_write;
    }
}

/*
 * read() syscall handler for pipe write endpoint. Always fails.
 */
static int
pipe_read_fail(file_obj_t *file, void *buf, int nbytes)
{
    return -1;
}

/*
 * write() syscall handler for pipe read endpoint. Always fails.
 */
static int
pipe_write_fail(file_obj_t *file, const void *buf, int nbytes)
{
    return -1;
}

/*
 * close() syscall handler for pipes. If the file refers to
 * the read end of the pipe, all further writes to the pipe
 * will fail. If it refers to the write end of the pipe, future
 * reads will return buffered data, then EOF when the buffer
 * is empty.
 */
static int
pipe_close(file_obj_t *file)
{
    pipe_state_t *pipe = file->private;

    /*
     * If both ends are closed, release the underlying pipe.
     * Otherwise, just mark the pipe as half duplex so the
     * other end knows when to give up.
     */
    if (pipe->half_closed) {
        free(pipe);
    } else {
        pipe->half_closed = true;
    }
    return 0;
}

/*
 * ioctl() syscall handler for pipes. Always fails.
 */
static int
pipe_ioctl(file_obj_t *file, int req, int arg)
{
    return -1;
}

/* File ops for the read end of the pipe */
static const file_ops_t pipe_read_fops = {
    .open = NULL,
    .read = pipe_read,
    .write = pipe_write_fail,
    .close = pipe_close,
    .ioctl = pipe_ioctl,
};

/* File ops for the write end of the pipe */
static const file_ops_t pipe_write_fops = {
    .open = NULL,
    .read = pipe_read_fail,
    .write = pipe_write,
    .close = pipe_close,
    .ioctl = pipe_ioctl,
};

/*
 * pipe() syscall handler. Creates a new pipe, and writes the
 * descriptor of the read end to readfd, and the write end to
 * writefd.
 */
__cdecl int
pipe_pipe(int *readfd, int *writefd)
{
    /* Allocate pipe data */
    pipe_state_t *pipe = malloc(sizeof(pipe_state_t));
    if (pipe == NULL) {
        debugf("Cannot allocate space for pipe\n");
        return -1;
    }

    /* Create read endpoint */
    file_obj_t *read_file = file_obj_alloc(&pipe_read_fops, OPEN_READ, false);
    if (read_file == NULL) {
        debugf("Cannot allocate pipe read endpoint\n");
        free(pipe);
        return -1;
    }

    /* Create write endpoint */
    file_obj_t *write_file = file_obj_alloc(&pipe_write_fops, OPEN_WRITE, false);
    if (write_file == NULL) {
        debugf("Cannot allocate pipe write endpoint\n");
        file_obj_free(read_file, false);
        free(pipe);
        return -1;
    }

    /* Starting from this point, close() can be called */
    pipe->head = 0;
    pipe->tail = 0;
    pipe->half_closed = false;
    read_file->private = pipe;
    write_file->private = pipe;

    /* Bind read descriptor */
    file_obj_t **files = get_executing_files();
    int kreadfd = file_desc_bind(files, -1, read_file);
    if (kreadfd < 0) {
        debugf("Cannot bind read descriptor\n");
        file_obj_free(read_file, true);
        file_obj_free(write_file, true);
        /* close() will free the pipe */
        return -1;
    }

    /* Bind write descriptor */
    int kwritefd = file_desc_bind(files, -1, write_file);
    if (kwritefd < 0) {
        debugf("Cannot bind write descriptor\n");
        file_desc_unbind(files, kreadfd);
        file_obj_free(write_file, true);
        /* close() will free the pipe */
        return -1;
    }

    /* Copy descriptors to userspace */
    if (!copy_to_user(readfd, &kreadfd, sizeof(int)) ||
        !copy_to_user(writefd, &kwritefd, sizeof(int)))
    {
        debugf("Failed to copy descriptors to userspace\n");
        file_desc_unbind(files, kreadfd);
        file_desc_unbind(files, kwritefd);
        /* close() will free the pipe */
        return -1;
    }

    return 0;
}