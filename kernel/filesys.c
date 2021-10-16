#include "filesys.h"
#include "types.h"
#include "debug.h"
#include "math.h"
#include "string.h"
#include "bitmap.h"
#include "file.h"
#include "paging.h"

/* Macros to access inode/data blocks */
#define fs_dentry(idx) (&fs_boot_block->dir_entries[idx])
#define fs_inode(idx) ((inode_t *)(fs_boot_block + 1 + (idx)))
#define fs_data(idx) ((uint8_t *)(fs_boot_block + 1 + fs_boot_block->inode_count + (idx)))
#define fs_nblocks(nbytes) div_round_up((nbytes), FS_BLOCK_SIZE)

/* Helpers for casting to/from file private data */
#define get_off(f) ((int)(f)->private)
#define set_off(f, x) ((f)->private = (x))

/* Size of a single filesystem block, in bytes */
#define FS_BLOCK_SIZE 4096

/* Maximum size in bytes of a file */
#define FS_MAX_FILE_SIZE (FS_BLOCK_SIZE * MAX_DATA_BLOCKS)

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
    for (i = 0; i < MAX_FILENAME_LEN; ++i) {
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
 * Allocates a data block and returns its index. Returns
 * -1 if there are no free data blocks remaining.
 */
static int
fs_alloc_data_block(void)
{
    int data_idx = bitmap_find_zero(fs_data_block_map, fs_boot_block->data_block_count);
    if (data_idx >= (int)fs_boot_block->data_block_count) {
        return -1;
    }
    bitmap_set(fs_data_block_map, data_idx);
    return data_idx;
}

/*
 * Frees a data block previously allocated by fs_alloc_data_block().
 */
static void
fs_free_data_block(int data_idx)
{
    assert((uint32_t)data_idx < fs_boot_block->data_block_count);
    bitmap_clear(fs_data_block_map, data_idx);
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
    if (inode_idx >= (int)fs_boot_block->inode_count) {
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
    int dentry_idx = dentry - &fs_boot_block->dir_entries[0];
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
    assert(fs_inode(inode_idx)->refcnt < (1 << 9) - 1);
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
        int i;
        for (i = 0; i < fs_nblocks(inode->size); ++i) {
            fs_free_data_block(inode->data_blocks[i]);
        }
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
        if (!bitmap_get(fs_dentry_map, i)) {
            continue;
        }

        dentry_t *curr = fs_dentry(i);
        if (fs_namecmp(fname, curr->name) == 0) {
            *dentry = curr;
            return 0;
        }
    }

    return -1;
}

/*
 * Iterator for an inode's data blocks. Yields a view of
 * file's data, one block at a time, by calling the provided
 * callback function. Returns the number of bytes that were
 * successfully iterated (note that this is zero, not -1 even
 * if no bytes were copied). The caller must clamp offset and
 * length to valid values within the file.
 */
static int
fs_iterate_data(
    inode_t *inode,
    int offset,
    int length,
    int (*callback)(void *data, int nbytes, void *private),
    void *private)
{
    assert(offset >= 0);
    assert(length >= 0);

    /* Compute intra-block offsets */
    int first_block = offset / FS_BLOCK_SIZE;
    int first_offset = offset % FS_BLOCK_SIZE;
    int last_block = (offset + length) / FS_BLOCK_SIZE;
    int last_offset = (offset + length) % FS_BLOCK_SIZE;

    /* Now copy the data! */
    int total_read = 0;
    int i;
    for (i = first_block; i <= last_block; ++i) {
        /* Adjust start offset */
        int start_offset = 0;
        if (i == first_block) {
            start_offset = first_offset;
        }

        /* Adjust end offset */
        int end_offset = FS_BLOCK_SIZE;
        if (i == last_block) {
            end_offset = last_offset;
        }

        /*
         * Check for 0-sized chunk to avoid out-of-bounds access on
         * the last data block. This can happen if the ending offset
         * is exactly a multiple of FS_BLOCK_SIZE.
         */
        int nbytes = end_offset - start_offset;
        if (nbytes > 0) {
            uint8_t *data = fs_data(inode->data_blocks[i]);
            if (callback(data + start_offset, nbytes, private) < 0) {
                break;
            }
            total_read += nbytes;
        }
    }

    return total_read;
}

/*
 * Private extra data to pass to fs_read_data_cb().
 */
typedef struct {
    uint8_t *buf;
    void *(*copy)(void *dest, const void *src, int nbytes);
} fs_read_data_private;

/*
 * fs_iterate_data() callback for fs_read_data().
 */
static int
fs_read_data_cb(void *data, int nbytes, void *private)
{
    fs_read_data_private *p = private;
    if (!p->copy(p->buf, data, nbytes)) {
        return -1;
    }
    p->buf += nbytes;
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
    int inode_idx,
    int offset,
    void *buf,
    int length,
    void *(*copy)(void *dest, const void *src, int nbytes))
{
    assert((uint32_t)inode_idx < fs_boot_block->inode_count);
    assert(offset >= 0);
    assert(length >= 0);

    /* If nothing left to read, we're done */
    inode_t *inode = fs_inode(inode_idx);
    if (offset >= inode->size) {
        return 0;
    }

    /* Clamp read length to end of file */
    length = min(length, inode->size - offset);

    /* Iterate data blocks, copying output to buf as we go */
    fs_read_data_private p;
    p.buf = buf;
    p.copy = copy;
    int copied = fs_iterate_data(inode, offset, length, fs_read_data_cb, &p);

    if (copied == 0) {
        return -1;
    } else {
        return copied;
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
        /* Skip dentries that aren't present */
        if (!bitmap_get(fs_dentry_map, i)) {
            continue;
        }

        /* Calculate length so we can copy in one go */
        dentry_t *dentry = fs_dentry(i);
        int len = fs_namelen(dentry->name);
        nbytes = min(nbytes, len);

        /* Perform copy */
        if (!copy_to_user(buf, dentry->name, nbytes)) {
            return -1;
        }

        /* Increment offset for next read */
        set_off(file, i + 1);
        return nbytes;
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
    if (nbytes < 0) {
        return -1;
    }

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
 * Shrinks the inode to the specified number of data blocks.
 */
static void
fs_shrink_blocks(inode_t *inode, int new_blocks)
{
    int old_blocks = fs_nblocks(inode->size);
    while (old_blocks > new_blocks) {
        fs_free_data_block(inode->data_blocks[--old_blocks]);
    }
}

/*
 * Grows the inode to the specified number of data blocks.
 * The newly allocated blocks are not cleared.
 */
static int
fs_grow_blocks(inode_t *inode, int new_blocks)
{
    int old_blocks = fs_nblocks(inode->size);

    /* Allocate new blocks, roll back if we run out of blocks */
    int i;
    for (i = old_blocks; i < new_blocks; ++i) {
        int data_idx = fs_alloc_data_block();
        if (data_idx < 0) {
            fs_shrink_blocks(inode, old_blocks);
            return -1;
        }
        inode->data_blocks[i] = data_idx;
    }

    return 0;
}

/*
 * Grows or shinks the size of the specified file.
 * If clear is true and the file size increases, the newly
 * allocated region will be zeroed out. This is guaranteed to
 * not fail when shrinking an inode.
 */
static int
fs_resize_inode(inode_t *inode, int new_length, bool clear)
{
    int old_blocks = fs_nblocks(inode->size);
    int new_blocks = fs_nblocks(new_length);

    /* Clear remainder of current block if growing a partially filled one */
    if (clear && new_length > inode->size && inode->size % FS_BLOCK_SIZE != 0) {
        int last_data_idx = inode->data_blocks[old_blocks - 1];

        /* Current offset within the last block */
        int start_offset = inode->size % FS_BLOCK_SIZE;

        /* If new blocks needed, fill to end; otherwise fill to new size */
        int end_offset = FS_BLOCK_SIZE;
        if (new_blocks == old_blocks) {
            end_offset = new_length % FS_BLOCK_SIZE;

            /* Resize will perfectly fill the last existing block */
            if (end_offset == 0) {
                end_offset = FS_BLOCK_SIZE;
            }
        }

        memset(fs_data(last_data_idx) + start_offset, 0, end_offset - start_offset);
    }

    if (new_blocks > old_blocks) {
        if (fs_grow_blocks(inode, new_blocks) < 0) {
            return -1;
        }

        /* Go through newly allocated blocks and clear them */
        if (clear) {
            int i;
            for (i = old_blocks; i < new_blocks; ++i) {
                memset(fs_data(inode->data_blocks[i]), 0, FS_BLOCK_SIZE);
            }
        }

    } else if (new_blocks < old_blocks) {
        fs_shrink_blocks(inode, new_blocks);
    }

    inode->size = new_length;
    return 0;
}

/*
 * Private extra data to pass to fs_file_write_cb().
 */
typedef struct {
    const uint8_t *buf;
} fs_file_write_private;

/*
 * fs_iterate_data() callback for fs_file_write().
 */
static int
fs_file_write_cb(void *data, int nbytes, void *private)
{
    fs_file_write_private *p = private;
    if (!copy_from_user(data, p->buf, nbytes)) {
        return -1;
    }
    p->buf += nbytes;
    return 0;
}

/*
 * write() syscall handler for files. Writes the contents of the
 * buffer to the file. Expands the file as necessary.
 */
static int
fs_file_write(file_obj_t *file, const void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    /*
     * If open was opened in append mode, seek to end of file
     * as per POSIX spec.
     */
    if (file->mode & OPEN_APPEND) {
        set_off(file, fs_inode(file->inode_idx)->size);
    }

    /* Ensure we don't overflow the maximum file size */
    int offset = get_off(file);
    nbytes = min(nbytes, FS_MAX_FILE_SIZE - offset);

    /* Number of bytes we've successfully copied into the file */
    int copied = 0;

    /* New length of file = max(offset + nbytes, current length) */
    inode_t *inode = fs_inode(file->inode_idx);
    int orig_length = inode->size;
    int new_length = orig_length;
    if (new_length < offset + nbytes) {
        new_length = offset + nbytes;

        /* If starting write beyond end of file, fill gap with zeros */
        if (offset > orig_length) {
            if (fs_resize_inode(inode, offset, true) < 0) {
                debugf("File write failed: cannot allocate data blocks to fill gap\n");
                goto exit;
            }
        }

        /* Allocate space for the actual data */
        if (fs_resize_inode(inode, new_length, false) < 0) {
            debugf("File write failed: cannot allocate data blocks to hold new data\n");
            goto exit;
        }
    }

    /* Copy data from userspace into data blocks */
    fs_file_write_private p;
    p.buf = buf;
    copied = fs_iterate_data(inode, offset, nbytes, fs_file_write_cb, &p);

exit:
    /*
     * If no bytes were copied at all, resize file back to the original
     * size, and if a gap was allocated, undo that.
     */
    if (copied == 0) {
        fs_resize_inode(inode, orig_length, false);
        return -1;
    }

    /*
     * Some bytes were copied, so we can't undo the gap allocation,
     * but we should trim off the excess bytes we allocated that
     * didn't get written at the end.
     */
    if (copied < nbytes) {
        new_length = max(orig_length, offset + copied);
        fs_resize_inode(inode, new_length, false);
    }

    /* Update file offset */
    set_off(file, offset + copied);
    return copied;
}

/*
 * seek() syscall handler for files. Sets the current read/write
 * offset. If data is written beyond the end of the file, the gap
 * is filled with zeros. Seeking beyond the maximum file size is
 * not allowed.
 */
static int
fs_file_seek(file_obj_t *file, int offset, int mode)
{
    int offset_base;
    switch (mode) {
    case SEEK_SET:
        offset_base = 0;
        break;
    case SEEK_CUR:
        offset_base = get_off(file);
        break;
    case SEEK_END:
        offset_base = fs_inode(file->inode_idx)->size;
        break;
    default:
        debugf("Unknown seek mode: %d\n", mode);
        return -1;
    }

    if (offset > 0 && offset > FS_MAX_FILE_SIZE - offset_base) {
        debugf("Seek offset greater than max file size\n");
        return -1;
    } else if (offset < 0 && offset_base + offset < 0) {
        debugf("Seek offset is negative\n");
        return -1;
    }

    set_off(file, offset_base + offset);
    return get_off(file);
}

/*
 * truncate() syscall handler for files. Sets the file size to the
 * specified value. If the new size is greater than the previous
 * size, the extra space will be filled with zeros. The current
 * offset is not modified.
 */
static int
fs_file_truncate(file_obj_t *file, int length)
{
    if (length < 0 || length > FS_MAX_FILE_SIZE) {
        return -1;
    }

    /* Reallocate data, filling in new data with zeros */
    inode_t *inode = fs_inode(file->inode_idx);
    return fs_resize_inode(inode, length, true);
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
    if (fs_dentry_map == NULL || fs_inode_map == NULL || fs_data_block_map == NULL) {
        panic("Failed to allocate filesystem bitmaps\n");
    }

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
        for (d = 0; d < fs_nblocks(inode->size); ++d) {
            bitmap_set(fs_data_block_map, inode->data_blocks[d]);
        }
    }
}

/* Directory file ops */
static const file_ops_t fs_dir_fops = {
    .open = fs_open,
    .read = fs_dir_read,
};

/* File (the real kind) file ops */
static const file_ops_t fs_file_fops = {
    .open = fs_open,
    .read = fs_file_read,
    .write = fs_file_write,
    .seek = fs_file_seek,
    .truncate = fs_file_truncate,
};

/* Initializes the filesystem */
void
fs_init(void *fs_start)
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
