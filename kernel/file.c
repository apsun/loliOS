#include "file.h"
#include "lib.h"
#include "debug.h"
#include "myalloc.h"
#include "rtc.h"
#include "filesys.h"
#include "terminal.h"
#include "process.h"
#include "taux.h"
#include "sb16.h"

/* File type to ops table mapping */
static const file_ops_t *file_ops_tables[FILE_TYPE_COUNT];

/*
 * Returns the file ops table corresponding to the specified
 * file type.
 */
static const file_ops_t *
get_file_ops(int file_type)
{
    if (file_type < 0 || file_type >= FILE_TYPE_COUNT) {
        return NULL;
    }
    return file_ops_tables[file_type];
}

/*
 * Registers a file ops table with its type to be used by open().
 */
void
file_register_type(int file_type, const file_ops_t *ops_table)
{
    assert(file_type >= 0 && file_type < FILE_TYPE_COUNT);
    file_ops_tables[file_type] = ops_table;
}

/*
 * Gets the file object array for the executing process.
 */
file_obj_t **
get_executing_files(void)
{
    pcb_t *pcb = get_executing_pcb();
    return pcb->files;
}

/*
 * Gets the file object corresponding to the given file
 * descriptor for the executing process.
 */
file_obj_t *
get_executing_file(int fd)
{
    /* Ensure descriptor is in bounds */
    if (fd < 0 || fd >= MAX_FILES) {
        return NULL;
    }

    /* Get corresponding file object */
    file_obj_t **files = get_executing_files();
    return files[fd];
}

/*
 * Allocates a new file object. Optionally calls open()
 * on the file object. Note that the new file object
 * starts with reference count ZERO, not one!
 */
file_obj_t *
file_obj_alloc(const file_ops_t *ops_table, int mode, bool open)
{
    /* Allocate file */
    file_obj_t *file = malloc(sizeof(file_obj_t));
    if (file == NULL) {
        return NULL;
    }

    /* Initialize fields */
    file->ops_table = ops_table;
    file->refcnt = 0;
    file->mode = mode;
    file->inode_idx = 0;
    file->private = NULL;

    /* Call open() if necessary */
    if (open && file->ops_table->open(file) < 0) {
        free(file);
        return NULL;
    }

    return file;
}

/*
 * Frees a file object. The file reference count must be zero.
 * Optionally calls the close() function. This should only be
 * directly called if the file cannot be correctly initialized,
 * before it is associated with a file descriptor.
 */
void
file_obj_free(file_obj_t *file, bool close)
{
    assert(file->refcnt == 0);
    if (close) {
        file->ops_table->close(file);
    }
    free(file);
}

/*
 * Increments the reference count of a file.
 */
file_obj_t *
file_obj_retain(file_obj_t *file)
{
    file->refcnt++;
    return file;
}

/*
 * Decrements the reference count of a file. If the refcount
 * reaches zero, close() is called and the file object is freed.
 */
void
file_obj_release(file_obj_t *file)
{
    assert(file->refcnt > 0);
    if (--file->refcnt == 0) {
        file_obj_free(file, true);
    }
}

/*
 * Allocates a file descriptor and binds it to the specified
 * file object, incrementing the reference count of the file.
 * Returns the file descriptor, or -1 if no free file descriptors
 * are available. If fd >= 0, will force the file to bind to
 * that specific descriptor.
 */
int
file_desc_bind(file_obj_t **files, int fd, file_obj_t *file)
{
    if (fd >= 0) {
        /* Just check that the descriptor is valid and not in use */
        if (fd >= MAX_FILES || files[fd] != NULL) {
            return -1;
        }
    } else {
        /* Find a free descriptor */
        for (fd = 0; fd < MAX_FILES; ++fd) {
            if (files[fd] == NULL) {
                break;
            }
        }
        if (fd == MAX_FILES) {
            return -1;
        }
    }

    /* Grab reference to object */
    files[fd] = file_obj_retain(file);
    return fd;
}

/*
 * Frees a file descriptor and decrements the reference count
 * of the corresponding file (may call close() if the refcount
 * reaches zero). Returns -1 if the fd does not refer to a
 * valid open file descriptor.
 */
int
file_desc_unbind(file_obj_t **files, int fd)
{
    if (fd < 0 || fd >= MAX_FILES) {
        return -1;
    }

    /* Check for unbinding an unused file */
    if (files[fd] == NULL) {
        return -1;
    }

    /* Decrement refcount of the file object and mark fd as free */
    file_obj_release(files[fd]);
    files[fd] = NULL;
    return 0;
}

/*
 * Replaces the file object that a file descriptor points to
 * with a new file object. This will decrement the refcount
 * of the original file object (if it was open) and increment
 * the refcount of the new file object.
 */
int
file_desc_rebind(file_obj_t **files, int fd, file_obj_t *new_file)
{
    if (fd < 0 || fd >= MAX_FILES) {
        return -1;
    }

    /* If the two refer to the same file, do nothing */
    file_obj_t *old_file = files[fd];
    if (old_file == new_file) {
        return fd;
    }

    /* Release old file, if present */
    if (old_file != NULL) {
        file_obj_release(old_file);
    }

    /* Replace it with the new file */
    files[fd] = file_obj_retain(new_file);
    return fd;
}

/*
 * Initializes the specified file object array.
 */
void
file_init(file_obj_t **files)
{
    int i;
    for (i = 0; i < MAX_FILES; ++i) {
        files[i] = NULL;
    }
}

/*
 * Clones the file object array of an existing process into
 * that of a new process. This will update reference counts
 * accordingly.
 */
void
file_clone(file_obj_t **new_files, file_obj_t **old_files)
{
    int i;
    for (i = 0; i < MAX_FILES; ++i) {
        file_obj_t *file = old_files[i];
        if (file != NULL) {
            new_files[i] = file_obj_retain(file);
        } else {
            new_files[i] = NULL;
        }
    }
}

/*
 * Closes all files in the specified file object array.
 */
void
file_deinit(file_obj_t **files)
{
    int i;
    for (i = 0; i < MAX_FILES; ++i) {
        file_desc_unbind(files, i);
    }
}

/*
 * create() syscall handler. Creates a new file object that
 * can be used to access the specified file. Returns the
 * file descriptor on success, or -1 on error.
 */
__cdecl int
file_create(const char *filename, int mode)
{
    /* Copy filename into kernel memory */
    char tmp[MAX_FILENAME_LEN + 1];
    if (!strscpy_from_user(tmp, filename, sizeof(tmp))) {
        debugf("Invalid string passed to open()\n");
        return -1;
    }

    /* Try to read filesystem entry */
    dentry_t dentry;
    if (read_dentry_by_name(tmp, &dentry) != 0) {
        debugf("File not found\n");
        return -1;
    }

    /* Get corresponding ops table */
    const file_ops_t *ops_table = get_file_ops(dentry.type);
    if (ops_table == NULL) {
        debugf("Unhandled file type: %u\n", dentry.type);
        return -1;
    }

    /* Allocate and initialize a file object */
    file_obj_t *file = file_obj_alloc(ops_table, mode, true);
    if (file == NULL) {
        debugf("Failed to allocate file\n");
        return -1;
    }

    /* Bind file object to a new descriptor */
    int fd = file_desc_bind(get_executing_files(), -1, file);
    if (fd < 0) {
        debugf("Failed to bind file descriptor\n");
        file_obj_free(file, true);
        return -1;
    }

    file->inode_idx = dentry.inode_idx;
    return fd;
}

/*
 * open() syscall handler. This is equivalent to calling create()
 * with a mode of OPEN_ALL (i.e. both read and write permissions).
 */
__cdecl int
file_open(const char *filename)
{
    return file_create(filename, OPEN_ALL);
}

/*
 * read() syscall handler. Reads the specified number of bytes
 * from the file into the specified userspace buffer. The
 * implementation is determined by the file type.
 */
__cdecl int
file_read(int fd, void *buf, int nbytes)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL || !(file->mode & OPEN_READ)) {
        debugf("Invalid fd or reading without permissions\n");
        return -1;
    }
    return file->ops_table->read(file, buf, nbytes);
}

/*
 * write() syscall handler. Writes the specified number of bytes
 * from the specified userspace buffer into the file. The
 * implementation is determined by the file type.
 */
__cdecl int
file_write(int fd, const void *buf, int nbytes)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL || !(file->mode & OPEN_WRITE)) {
        debugf("Invalid fd or writing without permissions\n");
        return -1;
    }
    return file->ops_table->write(file, buf, nbytes);
}

/*
 * close() syscall handler. Releases the specified file descriptor,
 * and if it was the last descriptor referring to a file object,
 * that file object is also released. Always returns 0 unless the
 * file descriptor is invalid.
 */
__cdecl int
file_close(int fd)
{
    /* Used to pass the dumb test that tries to close stdin and stdout */
    if (fd >= 0 && fd <= 1) {
        pcb_t *pcb = get_executing_pcb();
        if (pcb->compat) {
            debugf("Compatibility mode: cannot close fd %d\n", fd);
            return -1;
        }
    }

    return file_desc_unbind(get_executing_files(), fd);
}

/*
 * ioctl() syscall handler. Performs an arbitrary action determined
 * by the file type.
 */
__cdecl int
file_ioctl(int fd, int req, int arg)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->ioctl(file, req, arg);
}

/*
 * dup() syscall handler. If destfd == -1, this performs the
 * Linux equivalent of dup(srcfd). Otherwise, this performs the
 * Linux equivalent of dup2(srcfd, destfd). Upon return, destfd
 * points to the same file object as srcfd, and the original
 * destfd is closed (if it was originally open). On success,
 * destfd is returned.
 */
__cdecl int
file_dup(int srcfd, int destfd)
{
    file_obj_t *new_file = get_executing_file(srcfd);
    if (new_file == NULL) {
        return -1;
    }

    /*
     * If destfd is -1, pick a new descriptor, otherwise use the
     * one that they specified. Note that this is a bit different
     * from how Linux does it - Linux uses two separate syscalls,
     * dup() and dup2().
     */
    file_obj_t **files = get_executing_files();
    if (destfd == -1) {
        return file_desc_bind(files, -1, new_file);
    } else {
        return file_desc_rebind(files, destfd, new_file);
    }
}
