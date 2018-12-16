#include "zero.h"
#include "lib.h"
#include "file.h"
#include "paging.h"

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

/* Zero file type operations table */
static const file_ops_t zero_fops = {
    .read = zero_read,
    .write = zero_write,
};

/*
 * Initializes the zero file driver.
 */
void
zero_init(void)
{
    file_register_type(FILE_TYPE_ZERO, &zero_fops);
}
