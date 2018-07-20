#include "null.h"
#include "lib.h"
#include "file.h"

/*
 * open() syscall handler for the null file. Always succeeds.
 */
static int
null_open(file_obj_t *file)
{
    return 0;
}

/*
 * read() syscall handler for the null file. Always returns 0.
 */
static int
null_read(file_obj_t *file, void *buf, int nbytes)
{
    return 0;
}

/*
 * write() syscall handler for the null file. Always returns nbytes.
 */
static int
null_write(file_obj_t *file, const void *buf, int nbytes)
{
    return nbytes;
}

/*
 * close() syscall handler for the null file. Always succeeds.
 */
static int
null_close(file_obj_t *file)
{
    return 0;
}

/*
 * ioctl() syscall handler for the null file. Always fails.
 */
static int
null_ioctl(file_obj_t *file, int req, int arg)
{
    return -1;
}

/* Null file type operations table */
static const file_ops_t null_fops = {
    .open = null_open,
    .read = null_read,
    .write = null_write,
    .close = null_close,
    .ioctl = null_ioctl,
};

/*
 * Initializes the null file driver.
 */
void
null_init(void)
{
    file_register_type(FILE_TYPE_NULL, &null_fops);
}
