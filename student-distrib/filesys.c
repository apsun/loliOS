#include "filesys.h"
#include "lib.h"
#include "debug.h"
#include "paging.h"

/* Macros to access inode/data blocks */
#define FS_INODE(idx) ((inode_t *)(fs_boot_block + 1 + (idx)))
#define FS_DATA(idx) ((uint8_t *)(fs_boot_block + 1 + fs_boot_block->stat.inode_count + (idx)))

/* Holds the address of the boot block */
static boot_block_t *fs_boot_block = NULL;

/*
 * Compares a search (NUL-terminated) file name with a
 * potentially non-NUL-terminated raw file name. Essentially
 * the same as strcmp(), but limited to at most 32 chars.
 */
static int32_t
fs_namecmp(const char *search_name, const char *file_name)
{
    int32_t i;
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
static int32_t
fs_namelen(const char *file_name)
{
    int32_t i;
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
int32_t
read_dentry_by_name(const char *fname, dentry_t *dentry)
{
    uint32_t i;
    for (i = 0; i < fs_boot_block->stat.dentry_count; ++i) {
        dentry_t* curr_dentry = &fs_boot_block->dir_entries[i];
        if (fs_namecmp(fname, curr_dentry->name) == 0) {
            *dentry = *curr_dentry;
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
int32_t
read_dentry_by_index(uint32_t index, dentry_t *dentry)
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
int32_t
read_data(uint32_t inode, uint32_t offset, uint8_t *buf, uint32_t length)
{
    /* Check inode index bounds */
    if (inode >= fs_boot_block->stat.inode_count) {
        return -1;
    }

    inode_t *inode_p = FS_INODE(inode);

    /* Reading past EOF is an error */
    if (offset > inode_p->size) {
        return -1;
    }

    /* Clamp read length to end of file */
    if (length > inode_p->size - offset) {
        length = inode_p->size - offset;
    }

    /* Compute intra-block offsets */
    uint32_t first_block = offset / FS_BLOCK_SIZE;
    uint32_t first_offset = offset % FS_BLOCK_SIZE;
    uint32_t last_block = (offset + length) / FS_BLOCK_SIZE;
    uint32_t last_offset = (offset + length) % FS_BLOCK_SIZE;

    /* Now copy the data! */
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

        /* Check 0-sized copy to avoid out-of-bound read */
        uint32_t copy_len = end_offset - start_offset;
        if (copy_len > 0) {
            uint8_t *data = FS_DATA(inode_p->data_blocks[i]);
            memcpy(buf, data + start_offset, copy_len);
            buf += copy_len;
        }
    }

    return length;
}

/*
 * Open syscall for files/directories. Always succeeds.
 */
int32_t
fs_open(const char *filename, file_obj_t *file)
{
    return 0;
}

/*
 * Read syscall for directories. Writes the name of the next
 * entry in the directory to the buffer, NOT including the
 * NUL terminator. Returns the number of characters written.
 */
int32_t
fs_dir_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    if (nbytes < 0) {
        return -1;
    }

    /* Read next file dentry, return 0 if no more entries */
    dentry_t dentry;
    if (read_dentry_by_index(file->offset, &dentry) != 0) {
        return 0;
    }

    /* Calculate length so we can copy in one go */
    int32_t length = fs_namelen(dentry.name);
    if (nbytes > length) {
        nbytes = length;
    }

    /* Perform copy */
    if (!copy_to_user(buf, dentry.name, nbytes)) {
        return -1;
    }

    /* Increment offset for next read */
    file->offset++;

    /* Return number of chars read, excluding NUL terminator */
    return nbytes;
}

/*
 * Read syscall for files. Writes the contents of the file
 * to the buffer, starting from where the previous call to read
 * left off. Returns the number of bytes written.
 */
int32_t
fs_file_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    /* Check that the buffer is valid */
    if (!is_user_accessible(buf, nbytes, true)) {
        return -1;
    }

    /* Read directly into userspace buffer */
    int32_t count = read_data(file->inode_idx, file->offset, buf, nbytes);
    if (count <= 0) {
        return count;
    }

    /* Increment byte offset for next read */
    file->offset += count;

    /* Return how many bytes we read */
    return count;
}

/*
 * Write syscall for files/directories. Always fails.
 */
int32_t
fs_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return -1;
}

/*
 * Close syscall for files/directories. Always succeeds.
 */
int32_t
fs_close(file_obj_t *file)
{
    return 0;
}

/*
 * Ioctl syscall for files/directories. Always fails.
 */
int32_t
fs_ioctl(file_obj_t *file, uint32_t req, uint32_t arg)
{
    return -1;
}

/* Initializes the filesystem */
void
fs_init(uint32_t fs_start)
{
    /* Some basic sanity checks */
    ASSERT(sizeof(dentry_t) == 64);
    ASSERT(sizeof(stat_entry_t) == 64);
    ASSERT(sizeof(boot_block_t) == 4096);
    ASSERT(sizeof(inode_t) == 4096);

    /* Save address of boot block for future use */
    fs_boot_block = (boot_block_t *)fs_start;
}
