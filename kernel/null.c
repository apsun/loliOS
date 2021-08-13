#include "null.h"
#include "file.h"

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

/* Null file type operations table */
static const file_ops_t null_fops = {
    .read = null_read,
    .write = null_write,
};

/*
 * Initializes the null file driver.
 */
void
null_init(void)
{
    file_register_type(FILE_TYPE_NULL, &null_fops);
}
