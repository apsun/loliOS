#include "filesys.h"
#include "lib.h"
#include "debug.h"
#include "file.h"
#include "paging.h"

/* Macros to access inode/data blocks */
#define fs_inode(idx) ((inode_t *)(fs_boot_block + 1 + (idx)))
#define fs_data(idx) ((uint8_t *)(fs_boot_block + 1 + fs_boot_block->stat.inode_count + (idx)))

/* Helpers for casting to/from file private data */
#define get_off(f) ((uint32_t)(f)->private)
#define set_off(f, x) ((f)->private = (void *)(uint32_t)(x))

/* Holds the address of the boot block */
static boot_block_t *fs_boot_block = NULL;

/*
 * Compares a search (NUL-terminated) file name with a
 * potentially non-NUL-terminated raw file name. Essentially
 * the same as strcmp(), but limited to at most 32 chars.
 */
static int
fs_namecmp(const char *search_name, const char file_name[MAX_FILENAME_LEN])
{
    int i;
    for (i = 0; i < MAX_FILENAME_LEN; i++) {
        if (search_name[i] != file_name[i] || file_name[i] == '\0') {
            return search_name[i] - file_name[i];
        }
    }

    /*
     * We checked all 32 chars, now check if the search filename
     * is also 32 chars long (meaning a \0 at the 33rd byte),
     * otherwise the filenames don't actually match.
     */
    return search_name[i] - '\0';
}

/*
 * Returns the length of a file name. This is like
 * strlen, but will return 32 if no NUL terminator is hit.
 */
static int
fs_namelen(const char file_name[MAX_FILENAME_LEN])
{
    int i;
    for (i = 0; i < MAX_FILENAME_LEN; ++i) {
        if (file_name[i] == '\0') {
            break;
        }
    }
    return i;
}

/*
 * Finds a directory entry by name. If the entry is found,
 * it is copied to dentry and 0 is returned; otherwise,
 * -1 is returned.
 */
int
fs_dentry_by_name(const char *fname, dentry_t *dentry)
{
    uint32_t i;
    for (i = 0; i < fs_boot_block->stat.dentry_count; ++i) {
        dentry_t *curr = &fs_boot_block->dir_entries[i];
        if (fs_namecmp(fname, curr->name) == 0) {
            *dentry = *curr;
            return 0;
        }
    }
    return -1;
}

/*
 * Gets a directory entry by its index. If the entry exists,
 * it is copied to dentry and 0 is returned; otherwise,
 * -1 is returned.
 */
int
fs_dentry_by_index(uint32_t index, dentry_t *dentry)
{
    if (index >= fs_boot_block->stat.dentry_count) {
        return -1;
    }

    *dentry = fs_boot_block->dir_entries[index];
    return 0;
}

/*
 * Copies the data from the specified file at the given offset
 * into a buffer. If offset + length extends past the end of the
 * file, it is clamped to the end of the file. Returns the number
 * of bytes read, or -1 on error.
 */
int
fs_read_data(
    uint32_t inode, uint32_t offset,
    void *buf, uint32_t length,
    void *(*copy)(void *, const void *, int))
{
    /*
     * Ensure we don't read more than INT_MAX bytes, or else
     * we won't be able to represent it in the return value.
     */
    if ((int)length < 0) {
        return -1;
    }

    /* Check inode index bounds */
    if (inode >= fs_boot_block->stat.inode_count) {
        return -1;
    }

    inode_t *inode_p = fs_inode(inode);

    /* Reading past EOF is an error */
    if (offset > inode_p->size) {
        return -1;
    }

    /* Clamp read length to end of file */
    if (length > inode_p->size - offset) {
        length = inode_p->size - offset;
    }

    /* If nothing left to read, we're done */
    if (length == 0) {
        return 0;
    }

    /* Compute intra-block offsets */
    uint32_t first_block = offset / FS_BLOCK_SIZE;
    uint32_t first_offset = offset % FS_BLOCK_SIZE;
    uint32_t last_block = (offset + length) / FS_BLOCK_SIZE;
    uint32_t last_offset = (offset + length) % FS_BLOCK_SIZE;

    /* Now copy the data! */
    uint8_t *bufp = buf;
    uint32_t total_read = 0;
    uint32_t i;
    for (i = first_block; i <= last_block; ++i) {
        /* Adjust start offset */
        uint32_t start_offset = 0;
        if (i == first_block) {
            start_offset = first_offset;
        }

        /* Adjust end offset */
        uint32_t end_offset = FS_BLOCK_SIZE;
        if (i == last_block) {
            end_offset = last_offset;
        }

        /*
         * Check 0-sized copy to avoid out-of-bounds read on
         * data_blocks[i].
         */
        uint32_t copy_len = end_offset - start_offset;
        if (copy_len > 0) {
            uint8_t *data = fs_data(inode_p->data_blocks[i]);
            if (!copy(&bufp[total_read], data + start_offset, copy_len)) {
                break;
            }
            total_read += copy_len;
        }
    }

    /* We should have read *something*; it not, copy must have failed */
    if (total_read == 0) {
        return -1;
    } else {
        return (int)total_read;
    }
}

/*
 * open() syscall handler for files/directories. Always succeeds.
 */
static int
fs_open(file_obj_t *file)
{
    set_off(file, 0);
    return 0;
}

/*
 * read() syscall handler for directories. Writes the name of the
 * next entry in the directory to the buffer, NOT including the
 * NUL terminator. Returns the number of characters read.
 */
static int
fs_dir_read(file_obj_t *file, void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    }

    /* Read next file dentry, return 0 if no more entries */
    dentry_t dentry;
    if (fs_dentry_by_index(get_off(file), &dentry) != 0) {
        return 0;
    }

    /* Calculate length so we can copy in one go */
    int length = fs_namelen(dentry.name);
    if (nbytes > length) {
        nbytes = length;
    }

    /* Perform copy */
    if (!copy_to_user(buf, dentry.name, nbytes)) {
        return -1;
    }

    /* Increment offset for next read */
    set_off(file, get_off(file) + 1);

    /* Return number of chars read, excluding NUL terminator */
    return nbytes;
}

/*
 * read() syscall handler for files. Writes the contents of the
 * file to the buffer, starting from where the previous call to
 * read left off. Returns the number of bytes read.
 */
static int
fs_file_read(file_obj_t *file, void *buf, int nbytes)
{
    /* Read bytes into userspace buffer */
    int count = fs_read_data(file->inode_idx, get_off(file), buf, nbytes, copy_to_user);

    /* Increment byte offset for next read */
    if (count > 0) {
        set_off(file, get_off(file) + count);
    }

    /* Return how many bytes we read */
    return count;
}

/*
 * write() syscall handler for files/directories. Always fails.
 */
static int
fs_write(file_obj_t *file, const void *buf, int nbytes)
{
    return -1;
}

/*
 * close() syscall handler for files/directories. Always succeeds.
 */
static int
fs_close(file_obj_t *file)
{
    return 0;
}

/*
 * ioctl() syscall handler for files/directories. Always fails.
 */
static int
fs_ioctl(file_obj_t *file, int req, int arg)
{
    return -1;
}

/* Directory file ops */
static const file_ops_t fs_dir_fops = {
    .open = fs_open,
    .read = fs_dir_read,
    .write = fs_write,
    .close = fs_close,
    .ioctl = fs_ioctl,
};

/* File (the real kind) file ops */
static const file_ops_t fs_file_fops = {
    .open = fs_open,
    .read = fs_file_read,
    .write = fs_write,
    .close = fs_close,
    .ioctl = fs_ioctl,
};

/* Initializes the filesystem */
void
fs_init(uint32_t fs_start)
{
    /* Some basic sanity checks */
    assert(sizeof(dentry_t) == 64);
    assert(sizeof(stat_entry_t) == 64);
    assert(sizeof(boot_block_t) == 4096);
    assert(sizeof(inode_t) == 4096);

    /* Save address of boot block for future use */
    fs_boot_block = (boot_block_t *)fs_start;

    /* Register file ops table */
    file_register_type(FILE_TYPE_DIR, &fs_dir_fops);
    file_register_type(FILE_TYPE_FILE, &fs_file_fops);
}
