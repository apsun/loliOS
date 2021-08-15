#ifndef _FILESYS_H
#define _FILESYS_H

#include "types.h"

#define MAX_FILENAME_LEN 32
#define MAX_DENTRIES 63
#define MAX_DATA_BLOCKS 1023

#ifndef ASM

/* dentry structure */
typedef struct {
    /* Name of the file */
    char name[MAX_FILENAME_LEN];

    /* Type of the file (see file.h) */
    uint32_t type;

    /* Index of inode corresponding to this dentry */
    uint32_t inode_idx;

    /* Pad struct to 64 bytes */
    uint8_t reserved[24];
} dentry_t;

/* Boot block structure */
typedef struct {
    struct {
        /* Number of dentries in the filesystem */
        uint32_t dentry_count;

        /* Number of inode blocks in the filesystem */
        uint32_t inode_count;

        /* Number of data blocks in the filesystem */
        uint32_t data_block_count;

        /* Pad struct to 64 bytes */
        uint8_t reserved[52];
    };

    /* Remaining entries hold our directory entries */
    dentry_t dir_entries[MAX_DENTRIES];
} boot_block_t;

/* inode block structure */
typedef struct {
    /*
     * Since the maximum size of a file is 4096 * 1023 bytes,
     * we have 10 free upper bits in each inode's length field.
     * We use that to store the inode refcount and a pending deletion
     * flag, for a maximum of 511 open copies of a single file.
     * Also note that dup'd file descriptors do not count toward
     * this limit, since the refcount is per file object.
     *
     * This obviously isn't a good idea for disk-based filesystems,
     * but since our fs is loaded into memory this is a reasonable
     * alternative to maintaining a list of open inodes.
     */
    struct {
        /* Size of the file in bytes */
        uint32_t size : 22;

        /* Internal inode reference count */
        uint32_t refcnt : 9;

        /* Whether this inode should be deleted when refcnt hits zero */
        uint32_t delet : 1;
    };

    /* Array of data block indices that hold the file data */
    uint32_t data_blocks[MAX_DATA_BLOCKS];
} inode_t;

/* Filesystem syscall helpers */
int fs_create_file(const char *filename, dentry_t **dentry);
int fs_delete_file(const char *filename);

/* stat() syscall helper */
struct stat;
int fs_stat(const char *filename, struct stat *st);

/* Manages the inode reference count */
int fs_acquire_inode(int inode_idx);
void fs_release_inode(int inode_idx);

/* Finds a dentry by its name */
int fs_dentry_by_name(const char *fname, dentry_t **dentry);

/* Reads some data from a file with the specified inode index */
int fs_read_data(
    int inode_idx,
    int offset,
    void *buf,
    int length,
    void *(*copy)(void *dest, const void *src, int nbytes));

/* Initializes the filesystem */
void fs_init(void *fs_start);

#endif /* ASM */

#endif /* _FILESYS_H */
