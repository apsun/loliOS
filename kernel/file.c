#include "file.h"
#include "lib.h"
#include "debug.h"
#include "rtc.h"
#include "filesys.h"
#include "terminal.h"
#include "process.h"
#include "taux.h"
#include "sb16.h"

/* Terminal stdin file ops */
static const file_ops_t fops_stdin = {
    .open = terminal_kbd_open,
    .read = terminal_stdin_read,
    .write = terminal_stdin_write,
    .close = terminal_kbd_close,
    .ioctl = terminal_kbd_ioctl,
};

/* Terminal stdout file ops */
static const file_ops_t fops_stdout = {
    .open = terminal_kbd_open,
    .read = terminal_stdout_read,
    .write = terminal_stdout_write,
    .close = terminal_kbd_close,
    .ioctl = terminal_kbd_ioctl,
};

/* File (the real kind) file ops */
static const file_ops_t fops_file = {
    .open = fs_open,
    .read = fs_file_read,
    .write = fs_write,
    .close = fs_close,
    .ioctl = fs_ioctl,
};

/* Directory file ops */
static const file_ops_t fops_dir = {
    .open = fs_open,
    .read = fs_dir_read,
    .write = fs_write,
    .close = fs_close,
    .ioctl = fs_ioctl,
};

/* RTC file ops */
static const file_ops_t fops_rtc = {
    .open = rtc_open,
    .read = rtc_read,
    .write = rtc_write,
    .close = rtc_close,
    .ioctl = rtc_ioctl,
};

/* Mouse file ops */
static const file_ops_t fops_mouse = {
    .open = terminal_mouse_open,
    .read = terminal_mouse_read,
    .write = terminal_mouse_write,
    .close = terminal_mouse_close,
    .ioctl = terminal_mouse_ioctl,
};

/* Taux controller file ops */
static const file_ops_t fops_taux = {
    .open = taux_open,
    .read = taux_read,
    .write = taux_write,
    .close = taux_close,
    .ioctl = taux_ioctl,
};

/* Sound Blaster 16 file ops */
static const file_ops_t fops_sb16 = {
    .open = sb16_open,
    .read = sb16_read,
    .write = sb16_write,
    .close = sb16_close,
    .ioctl = sb16_ioctl,
};

/*
 * Gets the file object array for the executing process.
 */
file_obj_t *
get_executing_files(void)
{
    pcb_t *pcb = get_executing_pcb();
    ASSERT(pcb != NULL);
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

    /* Get file object, check that it's open */
    file_obj_t *file = &get_executing_files()[fd];
    if (file->fd < 0) {
        return NULL;
    }

    return file;
}

/*
 * Allocates a new file object for the executing process.
 * Returns NULL if the executing process has reached the
 * open file limit.
 */
file_obj_t *
file_obj_alloc(void)
{
    file_obj_t *files = get_executing_files();
    int i;
    for (i = 2; i < MAX_FILES; ++i) {
        file_obj_t *file = &files[i];
        if (file->fd < 0) {
            file->fd = i;
            file->private = 0;
            return file;
        }
    }
    return NULL;
}

/*
 * Initializes the file object from the given dentry.
 */
static int
file_obj_init(file_obj_t *file, dentry_t *dentry)
{
    switch (dentry->type) {
    case FTYPE_RTC:
        file->ops_table = &fops_rtc;
        break;
    case FTYPE_DIR:
        file->ops_table = &fops_dir;
        break;
    case FTYPE_FILE:
        file->ops_table = &fops_file;
        break;
    case FTYPE_MOUSE:
        file->ops_table = &fops_mouse;
        break;
    case FTYPE_TAUX:
        file->ops_table = &fops_taux;
        break;
    case FTYPE_SOUND:
        file->ops_table = &fops_sb16;
        break;
    default:
        debugf("Unknown file type: %d\n", dentry->type);
        return -1;
    }

    file->inode_idx = dentry->inode_idx;
    return 0;
}

/*
 * Initializes the specified file object array.
 */
void
file_init(file_obj_t *files)
{
    /* Initialize stdin as fd = 0 */
    files[0].fd = 0;
    files[0].ops_table = &fops_stdin;

    /* Initialize stdout as fd = 1 */
    files[1].fd = 1;
    files[1].ops_table = &fops_stdout;

    /* Clear the remaining files */
    int i;
    for (i = 2; i < MAX_FILES; ++i) {
        files[i].fd = -1;
    }
}

/* open() syscall handler */
__cdecl int
file_open(const char *filename)
{
    /* Copy filename into kernel memory */
    char tmp[MAX_FILENAME_LEN + 1];
    if (!strscpy_from_user(tmp, filename, sizeof(tmp))) {
        return -1;
    }

    /* Try to read filesystem entry */
    dentry_t dentry;
    if (read_dentry_by_name(tmp, &dentry) != 0) {
        return -1;
    }

    /* Allocate a file object to use */
    file_obj_t *file = file_obj_alloc();
    if (file == NULL) {
        return -1;
    }

    /* Initialize file object */
    if (file_obj_init(file, &dentry) < 0) {
        file->fd = -1;
        return -1;
    }

    /* Perform post-initialization setup */
    if (file->ops_table->open(tmp, file) < 0) {
        file->fd = -1;
        return -1;
    }

    /* Index becomes our file descriptor */
    return file->fd;
}

/* read() syscall handler */
__cdecl int
file_read(int fd, void *buf, int nbytes)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->read(file, buf, nbytes);
}

/* write() syscall handler */
__cdecl int
file_write(int fd, const void *buf, int nbytes)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->write(file, buf, nbytes);
}

/* close() syscall handler */
__cdecl int
file_close(int fd)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL) {
        return -1;
    }
    if (file->ops_table->close(file) < 0) {
        return -1;
    }
    file->fd = -1;
    return 0;
}

/* ioctl() syscall handler */
__cdecl int
file_ioctl(int fd, int req, int arg)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->ioctl(file, req, arg);
}
