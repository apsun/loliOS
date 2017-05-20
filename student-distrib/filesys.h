#ifndef _FILESYS_H
#define _FILESYS_H

#include "types.h"
#include "file.h"

/* Size of a single filesystem block, in bytes */
#define FS_BLOCK_SIZE 4096

/* Maximum filename length */
#define FS_MAX_FNAME_LEN 32

/* File type constants */
#define FTYPE_RTC 0
#define FTYPE_DIR 1
#define FTYPE_FILE 2
#define FTYPE_MOUSE 3

#ifndef ASM

/* dentry structure */
typedef struct {
    /* Name of the file */
    uint8_t name[FS_MAX_FNAME_LEN];

    /* Type of the file */
    uint32_t type;

    /* Index of inode corresponding to this dentry */
    uint32_t inode_idx;

    /* Pad struct to 64 bytes */
    uint8_t reserved[24];
} dentry_t;

/* Stat entry structure */
typedef struct stat_entry_t {
    /* Number of dentries in the filesystem */
    uint32_t dentry_count;

    /* Number of inode blocks in the filesystem */
    uint32_t inode_count;

    /* Number of data blocks in the filesystem */
    uint32_t data_block_count;

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
    uint32_t size;

    /* Array of data block indices that hold the file data */
    uint32_t data_blocks[1023];
} inode_t;

/* Finds a dentry by its name */
int32_t read_dentry_by_name(const uint8_t *fname, dentry_t *dentry);

/* Finds a dentry by its index */
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry);

/* Reads some data from a file with the specified inode index */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t *buf, uint32_t length);

/* Initializes the filesystem */
void fs_init(uint32_t fs_start);

/* Filesystem syscall interface */
int32_t fs_open(const uint8_t *filename, file_obj_t *file);
int32_t fs_file_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t fs_dir_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t fs_write(file_obj_t *file, const void *buf, int32_t nbytes);
int32_t fs_close(file_obj_t *file);

#endif /* ASM */

#endif /* _FILESYS_H */
