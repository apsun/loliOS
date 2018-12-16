#include "filesys.h"
#include "lib.h"
#include "debug.h"
#include "bitmap.h"
#include "file.h"
#include "paging.h"

/*
 * Since the maximum size of a file is 4096 * 1023 bytes,
 * we have 10 free upper bits in each inode's length field.
 * We use that to store the inode refcount and a pending deletion
 * flag, for a maximum of 512 open copies of a single file.
 * Also note that dup'd file descriptors do not count toward
 * this limit, since the refcount is per file object.
 *
 * This obviously isn't a good idea for disk-based filesystems,
 * but since our fs is loaded into memory this is a reasonable
 * alternative to maintaining a list of open inodes.
 */

/* Macros to access inode/data blocks */
#define fs_dentry(idx) (&fs_boot_block->dir_entries[idx])
#define fs_inode(idx) ((inode_t *)(fs_boot_block + 1 + (idx)))
#define fs_data(idx) ((uint8_t *)(fs_boot_block + 1 + fs_boot_block->inode_count + (idx)))

/* Helpers for casting to/from file private data */
#define get_off(f) ((uint32_t)(f)->private)
#define set_off(f, x) ((f)->private = (void *)(uint32_t)(x))

/* Holds the address of the boot block */
static boot_block_t *fs_boot_block;

/* Bitmap of allocated dentries/inodes/data blocks */
static bitmap_t *fs_dentry_map;
static bitmap_t *fs_inode_map;
static bitmap_t *fs_data_block_map;

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
 * Populates the filesystem bitmaps with the initial state.
 * Since our filesystem is not persistent, we need to regenerate
 * this every boot.
 */
static void
fs_generate_bitmaps(void)
{
    fs_dentry_map = bitmap_alloc(MAX_DENTRIES);
    fs_inode_map = bitmap_alloc(fs_boot_block->inode_count);
    fs_data_block_map = bitmap_alloc(fs_boot_block->data_block_count);

    int i;
    for (i = 0; i < (int)fs_boot_block->dentry_count; ++i) {
        dentry_t *dentry = fs_dentry(i);
        inode_t *inode = fs_inode(dentry->inode_idx);

        /* These should always be zero on bootup... */
        assert(inode->refcnt == 0);
        assert(inode->delet == 0);

        /* Set as allocated: dentry, inode, all data blocks */
        bitmap_set(fs_dentry_map, i);
        bitmap_set(fs_inode_map, dentry->inode_idx);
        int d;
        for (d = 0; d < (inode->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE; ++d) {
            bitmap_set(fs_data_block_map, inode->data_blocks[d]);
        }
    }
}

/*
 * Adds a new empty file to the filesystem. Currently
 * this is only able to create normal files.
 */
int
fs_create_file(const char *filename, dentry_t **dentry_out)
{
    /* Check that filename will fit */
    if (strlen(filename) > MAX_FILENAME_LEN) {
        return -1;
    }

    /* Find a free dentry */
    int dentry_idx = bitmap_find_zero(fs_dentry_map, MAX_DENTRIES);
    if (dentry_idx >= MAX_DENTRIES) {
        debugf("Reached maximum number of dentries\n");
        return -1;
    }

    /* Find a free inode */
    int inode_idx = bitmap_find_zero(fs_inode_map, fs_boot_block->inode_count);
    if ((uint32_t)inode_idx >= fs_boot_block->inode_count) {
        debugf("Reached maximum number of inodes\n");
        return -1;
    }

    /* Mark dentry and inode as allocated */
    bitmap_set(fs_dentry_map, dentry_idx);
    bitmap_set(fs_inode_map, inode_idx);

    /* Initialize dentry values */
    dentry_t *dentry = fs_dentry(dentry_idx);
    strncpy(dentry->name, filename, MAX_FILENAME_LEN);
    dentry->type = FILE_TYPE_FILE;
    dentry->inode_idx = inode_idx;
    *dentry_out = dentry;

    /* Initialize inode values */
    inode_t *inode = fs_inode(inode_idx);
    inode->size = 0;
    inode->refcnt = 0;
    inode->delet = 0;
    return 0;
}

/*
 * Deletes a file from the filesystem.
 */
int
fs_delete_file(const char *filename)
{
    dentry_t *dentry;
    if (fs_dentry_by_name(filename, &dentry) < 0) {
        return -1;
    }

    /* Free up dentry */
    int dentry_idx = dentry - fs_boot_block->dir_entries;
    bitmap_clear(fs_dentry_map, dentry_idx);

    /*
     * Mark inode as pending deletion. If nobody had the inode open,
     * this will immediately delete it; otherwise it will be deleted
     * once the last file referencing it is closed.
     */
    if (dentry->type == FILE_TYPE_FILE) {
        fs_acquire_inode(dentry->inode_idx);
        inode_t *inode = fs_inode(dentry->inode_idx);
        inode->delet = true;
        fs_release_inode(dentry->inode_idx);
    }
    return 0;
}

/*
 * Gets metadata about the specified file.
 */
int
fs_stat(const char *filename, stat_t *st)
{
    dentry_t *dentry;
    if (fs_dentry_by_name(filename, &dentry) < 0) {
        debugf("File not found for stat\n");
        return -1;
    }

    st->type = dentry->type;
    st->size = 0;
    if (dentry->type == FILE_TYPE_FILE) {
        inode_t *inode = fs_inode(dentry->inode_idx);
        st->size = inode->size;
    }
    return 0;
}

/*
 * Increments the reference count of the specified inode,
 * preventing it from being deleted on unlink.
 */
int
fs_acquire_inode(int inode_idx)
{
    assert((uint32_t)inode_idx < fs_boot_block->inode_count);
    assert(bitmap_get(fs_inode_map, inode_idx));
    fs_inode(inode_idx)->refcnt++;
    return inode_idx;
}

/*
 * Decrements the reference count of the specified inode.
 * If the refcount reaches zero and the inode has been marked
 * for deletion, it is freed.
 */
void
fs_release_inode(int inode_idx)
{
    assert((uint32_t)inode_idx < fs_boot_block->inode_count);
    assert(bitmap_get(fs_inode_map, inode_idx));
    inode_t *inode = fs_inode(inode_idx);
    assert(inode->refcnt > 0);
    if (--inode->refcnt == 0 && inode->delet) {
        debugf("File inode refcount zero, deleting file w/ inode = %d\n", inode_idx);
        bitmap_clear(fs_inode_map, inode_idx);
    }
}

/*
 * Finds a directory entry by name. If the entry is found,
 * it is copied to dentry and 0 is returned; otherwise,
 * -1 is returned.
 */
int
fs_dentry_by_name(const char *fname, dentry_t **dentry)
{
    int i;
    for (i = 0; i < MAX_DENTRIES; ++i) {
        if (bitmap_get(fs_dentry_map, i)) {
            dentry_t *curr = &fs_boot_block->dir_entries[i];
            if (fs_namecmp(fname, curr->name) == 0) {
                *dentry = curr;
                return 0;
            }
        }
    }
    return -1;
}

/*
 * Copies the data from the specified file at the given offset
 * into a buffer. If offset + length extends past the end of the
 * file, it is clamped to the end of the file. Returns the number
 * of bytes read, or -1 on error.
 */
int
fs_read_data(
    int inode, uint32_t offset,
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

    /* Reading past EOF is an error */
    inode_t *inode_p = fs_inode(inode);
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

    int i;
    for (i = get_off(file); i < MAX_DENTRIES; ++i) {
        if (bitmap_get(fs_dentry_map, i)) {
            dentry_t *dentry = fs_dentry(i);

            /* Calculate length so we can copy in one go */
            int length = fs_namelen(dentry->name);
            if (nbytes > length) {
                nbytes = length;
            }

            /* Perform copy */
            if (!copy_to_user(buf, dentry->name, nbytes)) {
                return -1;
            }

            /* Increment offset for next read */
            set_off(file, i + 1);

            /* Return number of chars read, excluding NUL terminator */
            return nbytes;
        }
    }

    /* No more files to read */
    set_off(file, i);
    return 0;
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

/* Directory file ops */
static const file_ops_t fs_dir_fops = {
    .open = fs_open,
    .read = fs_dir_read,
    .write = fs_write,
    .close = fs_close,
};

/* File (the real kind) file ops */
static const file_ops_t fs_file_fops = {
    .open = fs_open,
    .read = fs_file_read,
    .write = fs_write,
    .close = fs_close,
};

/* Initializes the filesystem */
void
fs_init(uint32_t fs_start)
{
    /* Some basic sanity checks */
    assert(sizeof(dentry_t) == 64);
    assert(sizeof(boot_block_t) == 4096);
    assert(sizeof(inode_t) == 4096);

    /* Save address of boot block for future use */
    fs_boot_block = (boot_block_t *)fs_start;

    /* Generate the initial bitmap state */
    fs_generate_bitmaps();

    /* Register file ops table */
    file_register_type(FILE_TYPE_DIR, &fs_dir_fops);
    file_register_type(FILE_TYPE_FILE, &fs_file_fops);
}
