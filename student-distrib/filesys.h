#ifndef _FILESYS_H
#define _FILESYS_H

#include "types.h"
#include "file.h"

/* Size of a single filesystem block, in bytes */
#define FS_BLOCK_SIZE 4096

/* Maximum filename length */
#define MAX_FILENAME_LEN 32

#ifndef ASM

/* dentry structure */
typedef struct {
    /* Name of the file */
    char name[MAX_FILENAME_LEN];

    /* Type of the file */
    int32_t type;

    /* Index of inode corresponding to this dentry */
    int32_t inode_idx;

    /* Pad struct to 64 bytes */
    uint8_t reserved[24];
} dentry_t;

/* Stat entry structure */
typedef struct stat_entry_t {
    /* Number of dentries in the filesystem */
    int32_t dentry_count;

    /* Number of inode blocks in the filesystem */
    int32_t inode_count;

    /* Number of data blocks in the filesystem */
    int32_t data_block_count;

    /* Pad struct to 64 bytes */
    uint8_t reserved[52];
} stat_entry_t;

/* Boot block structure */
typedef struct {
    /* First entry holds some statistics about our filesystem */
    stat_entry_t stat;

    /* Remaining entries hold our directory entries */
    dentry_t dir_entries[63];
} boot_block_t;

/* inode block structure */
typedef struct {
    /* Size of the file in bytes */
    int32_t size;

    /* Array of data block indices that hold the file data */
    int32_t data_blocks[1023];
} inode_t;

/* Finds a dentry by its name */
int read_dentry_by_name(const char *fname, dentry_t *dentry);

/* Finds a dentry by its index */
int read_dentry_by_index(int index, dentry_t* dentry);

/* Reads some data from a file with the specified inode index */
int read_data(int inode, int offset, uint8_t *buf, int length);

/* Initializes the filesystem */
void fs_init(uint32_t fs_start);

/* Filesystem syscall interface */
int fs_open(const char *filename, file_obj_t *file);
int fs_file_read(file_obj_t *file, void *buf, int nbytes);
int fs_dir_read(file_obj_t *file, void *buf, int nbytes);
int fs_write(file_obj_t *file, const void *buf, int nbytes);
int fs_close(file_obj_t *file);
int fs_ioctl(file_obj_t *file, int req, int arg);

#endif /* ASM */

#endif /* _FILESYS_H */
