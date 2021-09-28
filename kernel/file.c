#include "file.h"
#include "types.h"
#include "debug.h"
#include "myalloc.h"
#include "paging.h"
#include "filesys.h"
#include "process.h"

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
    file->nonblocking = false;
    file->inode_idx = -1;
    file->private = NULL;

    /* Call open() if necessary */
    if (open && file->ops_table->open != NULL) {
        if (file->ops_table->open(file) < 0) {
            return NULL;
        }
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
    if (file->inode_idx >= 0) {
        fs_release_inode(file->inode_idx);
    }
    if (close && file->ops_table->close != NULL) {
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
    assert(file->refcnt < INT_MAX);
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
    if (strscpy_from_user(tmp, filename, sizeof(tmp)) < 0) {
        debugf("Invalid string passed to open()\n");
        return -1;
    }

    /* Try to read or create filesystem entry */
    dentry_t *dentry;
    if (fs_dentry_by_name(tmp, &dentry) < 0) {
        if (mode & OPEN_CREATE) {
            if (fs_create_file(tmp, &dentry) < 0) {
                debugf("Failed to create file: %s\n", tmp);
                return -1;
            }
        } else {
            debugf("File not found: %s\n", tmp);
            return -1;
        }
    }

    /* Get corresponding ops table */
    const file_ops_t *ops_table = get_file_ops(dentry->type);
    if (ops_table == NULL) {
        debugf("Unhandled file type: %u\n", dentry->type);
        return -1;
    }

    /* Allocate and initialize a file object */
    file_obj_t *file = file_obj_alloc(ops_table, mode, true);
    if (file == NULL) {
        debugf("Failed to allocate file object\n");
        return -1;
    }

    /* Bind file object to a new descriptor */
    int fd = file_desc_bind(get_executing_files(), -1, file);
    if (fd < 0) {
        debugf("Failed to bind file descriptor\n");
        file_obj_free(file, true);
        return -1;
    }

    /* Increment inode refcount */
    if (dentry->type == FILE_TYPE_FILE) {
        file->inode_idx = fs_acquire_inode(dentry->inode_idx);
    }

    /* If truncate flag was specified, attempt to truncate the file */
    if ((mode & OPEN_TRUNC) && (mode & OPEN_WRITE)) {
        file_truncate(fd, 0);
    }

    return fd;
}

/*
 * open() syscall handler. This is equivalent to calling create()
 * with a mode of OPEN_RDWR (i.e. both read and write permissions).
 */
__cdecl int
file_open(const char *filename)
{
    return file_create(filename, OPEN_RDWR);
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
    if (fd == 0 || fd == 1) {
        pcb_t *pcb = get_executing_pcb();
        if (pcb->compat) {
            debugf("Compatibility mode: cannot close fd %d\n", fd);
            return -1;
        }
    }

    return file_desc_unbind(get_executing_files(), fd);
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

/*
 * Handles the FCNTL_NONBLOCK fcntl() call.
 */
static int
file_fcntl_nonblock(file_obj_t *file, int req, intptr_t arg)
{
    bool orig_nonblocking = file->nonblocking;
    file->nonblocking = !!arg;
    return orig_nonblocking;
}

/*
* fcntl() syscall handler. Similar to ioctl(), but is standardized
* for all file objects. No more accidentally sending bogus ioctl()
* calls to unknown objects.
*/
__cdecl int
file_fcntl(int fd, int req, intptr_t arg)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL) {
        return -1;
    }

    switch (req) {
    case FCNTL_NONBLOCK:
        return file_fcntl_nonblock(file, req, arg);
    default:
        return -1;
    }
}

/*
 * unlink() syscall handler. Removes the specified file from
 * the filesystem.
 */
__cdecl int
file_unlink(const char *filename)
{
    char tmp[MAX_FILENAME_LEN + 1];
    if (strscpy_from_user(tmp, filename, sizeof(tmp)) < 0) {
        debugf("Invalid string passed to unlink()\n");
        return -1;
    }

    return fs_delete_file(tmp);
}

/*
 * stat() syscall handler. Retrieves metadata about the
 * specified file and copies it into the specified buffer.
 */
__cdecl int
file_stat(const char *filename, stat_t *buf)
{
    char tmp[MAX_FILENAME_LEN + 1];
    if (strscpy_from_user(tmp, filename, sizeof(tmp)) < 0) {
        debugf("Invalid string passed to stat()\n");
        return -1;
    }

    stat_t st;
    if (fs_stat(tmp, &st) < 0) {
        return -1;
    }

    if (!copy_to_user(buf, &st, sizeof(stat_t))) {
        debugf("Failed to copy stat to userspace\n");
        return -1;
    }

    return 0;
}

/*
 * Helper macro for delegating to functions in the file ops table.
 */
#define FORWARD_FILECALL(fd, md, fn, ...) do {                    \
    file_obj_t *file = get_executing_file(fd);                    \
    if (file == NULL) {                                           \
        debugf("File: invalid file descriptor\n");                \
        return -1;                                                \
    }                                                             \
    if (file->ops_table->fn == NULL) {                            \
        debugf("File: %s() not implemented\n", #fn);              \
        return -1;                                                \
    }                                                             \
    if ((file->mode & (md)) != (md)) {                            \
        debugf("File: %s() requires %s permissions\n", #fn, #md); \
        return -1;                                                \
    }                                                             \
    return file->ops_table->fn(file, ## __VA_ARGS__);             \
} while (0)

/*
 * read() syscall handler. Reads the specified number of bytes
 * from the file into the specified userspace buffer.
 */
__cdecl int
file_read(int fd, void *buf, int nbytes)
{
    FORWARD_FILECALL(fd, OPEN_READ, read, buf, nbytes);
}

/*
 * write() syscall handler. Writes the specified number of bytes
 * from the specified userspace buffer into the file.
 */
__cdecl int
file_write(int fd, const void *buf, int nbytes)
{
    FORWARD_FILECALL(fd, OPEN_WRITE, write, buf, nbytes);
}

/*
 * ioctl() syscall handler. Performs an arbitrary action determined
 * by the file type.
 */
__cdecl int
file_ioctl(int fd, int req, intptr_t arg)
{
    FORWARD_FILECALL(fd, OPEN_NONE, ioctl, req, arg);
}

/*
 * seek() syscall handler. Modifies the current read/write offset
 * of the file.
 */
__cdecl int
file_seek(int fd, int offset, int mode)
{
    FORWARD_FILECALL(fd, OPEN_NONE, seek, offset, mode);
}

/*
 * truncate() syscall handler. Sets the file length to the specified
 * value. The file must be opened in writeable mode.
 */
__cdecl int
file_truncate(int fd, int length)
{
    FORWARD_FILECALL(fd, OPEN_WRITE, truncate, length);
}

#undef FORWARD_FILECALL
