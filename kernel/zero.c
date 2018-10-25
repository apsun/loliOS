#include "zero.h"
#include "lib.h"
#include "file.h"
#include "paging.h"

/*
 * open() syscall handler for the zero file. Always succeeds.
 */
static int
zero_open(file_obj_t *file)
{
    return 0;
}

/*
 * read() syscall handler for the zero file. Fills the buffer
 * with zero bytes.
 */
static int
zero_read(file_obj_t *file, void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    int copied = 0;
    uint8_t block[256] = {0};
    uint8_t *bufp = buf;
    while (copied < nbytes) {
        int to_copy = sizeof(block);
        if (to_copy > nbytes - copied) {
            to_copy = nbytes - copied;
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
 * write() syscall handler for the zero file. Always returns nbytes.
 */
static int
zero_write(file_obj_t *file, const void *buf, int nbytes)
{
    return nbytes;
}

/*
 * close() syscall handler for the zero file. Always succeeds.
 */
static int
zero_close(file_obj_t *file)
{
    return 0;
}

/*
 * ioctl() syscall handler for the zero file. Always fails.
 */
static int
zero_ioctl(file_obj_t *file, int req, int arg)
{
    return -1;
}

/* Zero file type operations table */
static const file_ops_t zero_fops = {
    .open = zero_open,
    .read = zero_read,
    .write = zero_write,
    .close = zero_close,
    .ioctl = zero_ioctl,
};

/*
 * Initializes the zero file driver.
 */
void
zero_init(void)
{
    file_register_type(FILE_TYPE_ZERO, &zero_fops);
}
