#include "file.h"
#include "lib.h"
#include "debug.h"
#include "rtc.h"
#include "filesys.h"
#include "terminal.h"
#include "process.h"
#include "taux.h"
#include "sb16.h"
#include "ne2k.h"

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

static const file_ops_t fops_ne2k = {
    .open = ne2k_open,
    .read = ne2k_read,
    .write = ne2k_write,
    .close = ne2k_close,
    .ioctl = ne2k_ioctl,
};

/* Initializes the file object from the given dentry */
static bool
init_file_obj(file_obj_t *file, dentry_t *dentry)
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
    case FTYPE_NET:
        file->ops_table = &fops_ne2k;
        break;
    default:
        debugf("Unknown file type: %d\n", dentry->type);
        return false;
    }

    file->offset = 0;
    file->inode_idx = dentry->inode_idx;
    file->valid = true;
    return true;
}

/* Gets the file object array for the executing process */
static file_obj_t *
get_executing_file_objs(void)
{
    pcb_t *pcb = get_executing_pcb();
    ASSERT(pcb != NULL);
    return pcb->files;
}

/* Gets the file object corresponding to the given descriptor */
static file_obj_t *
get_executing_file_obj(int fd)
{
    file_obj_t *file;

    /* Ensure descriptor is in bounds */
    if (fd < 0 || fd >= MAX_FILES) {
        return NULL;
    }

    /* Get file object, check that it's open */
    file = &get_executing_file_objs()[fd];
    if (!file->valid) {
        return NULL;
    }

    return file;
}

/*
 * Initializes the specified file object array.
 */
void
file_init(file_obj_t *files)
{
    /* Initialize stdin as fd = 0 */
    files[0].valid = true;
    files[0].ops_table = &fops_stdin;

    /* Initialize stdout as fd = 1 */
    files[1].valid = true;
    files[1].ops_table = &fops_stdout;

    /* Clear the remaining files */
    int i;
    for (i = 2; i < MAX_FILES; ++i) {
        files[i].valid = false;
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

    file_obj_t *files = get_executing_file_objs();

    /* Skip fd = 0 (stdin) and fd = 1 (stdout) */
    int i;
    for (i = 2; i < MAX_FILES; ++i) {
        if (!files[i].valid) {
            /* Try to read filesystem entry */
            dentry_t dentry;
            if (read_dentry_by_name(tmp, &dentry) != 0) {
                return -1;
            }

            /* Initialize file object */
            if (!init_file_obj(&files[i], &dentry)) {
                return -1;
            }

            /* Perform post-initialization setup */
            if (files[i].ops_table->open(tmp, &files[i]) != 0) {
                files[i].valid = false;
                return -1;
            }

            /* Index becomes our file descriptor */
            return i;
        }
    }

    /* Too many files open */
    return -1;
}

/* read() syscall handler */
__cdecl int
file_read(int fd, void *buf, int nbytes)
{
    file_obj_t *file = get_executing_file_obj(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->read(file, buf, nbytes);
}

/* write() syscall handler */
__cdecl int
file_write(int fd, const void *buf, int nbytes)
{
    file_obj_t *file = get_executing_file_obj(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->write(file, buf, nbytes);
}

/* close() syscall handler */
__cdecl int
file_close(int fd)
{
    file_obj_t *file = get_executing_file_obj(fd);
    if (file == NULL) {
        return -1;
    }
    if (file->ops_table->close(file) != 0) {
        return -1;
    }
    file->valid = false;
    return 0;
}

/* ioctl() syscall handler */
__cdecl int
file_ioctl(int fd, int req, int arg)
{
    file_obj_t *file = get_executing_file_obj(fd);
    if (file == NULL) {
        return -1;
    }
    return file->ops_table->ioctl(file, req, arg);
}
