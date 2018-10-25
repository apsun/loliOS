#include "random.h"
#include "lib.h"
#include "file.h"
#include "paging.h"

/*
 * open() syscall handler for the random file. Always succeeds.
 */
static int
random_open(file_obj_t *file)
{
    return 0;
}

/*
 * read() syscall handler for the random file. Fills the buffer
 * with random bytes.
 */
static int
random_read(file_obj_t *file, void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    int copied = 0;
    uint8_t block[256];
    uint8_t *bufp = buf;
    while (copied < nbytes) {
        int to_copy = sizeof(block);
        if (to_copy > nbytes - copied) {
            to_copy = nbytes - copied;
        }

        int i;
        for (i = 0; i < to_copy; ++i) {
            block[i] = rand() & 0xff;
        }

        if (!copy_to_user(&bufp[copied], block, to_copy)) {
            break;
        }
        copied += to_copy;
    }

    if (copied == 0) {
        return -1;
    } else {
        return copied;
    }
}

/*
 * write() syscall handler for the random file. Always fails.
 */
static int
random_write(file_obj_t *file, const void *buf, int nbytes)
{
    return -1;
}

/*
 * close() syscall handler for the random file. Always succeeds.
 */
static int
random_close(file_obj_t *file)
{
    return 0;
}

/*
 * ioctl() syscall handler for the random file. Always fails.
 */
static int
random_ioctl(file_obj_t *file, int req, int arg)
{
    return -1;
}

/* Random file type operations table */
static const file_ops_t random_fops = {
    .open = random_open,
    .read = random_read,
    .write = random_write,
    .close = random_close,
    .ioctl = random_ioctl,
};

/*
 * Initializes the random file driver.
 */
void
random_init(void)
{
    file_register_type(FILE_TYPE_RANDOM, &random_fops);
}
