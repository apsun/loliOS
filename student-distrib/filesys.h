#ifndef _FILESYS_H
#define _FILESYS_H

#include "lib.h"
#include "types.h"

#define KB(x) ((x) * 1024)
/* size of boot block, inode block and data block */
#define BLOCK_SIZE KB(4)
/* max number of directory entries in the boot block */
#define DIR_CAPACITY 63
/* size of a directory entry in the boot block */
#define DIR_ENTRY_SIZE 64
/* the max number of char that a file name can have */
#define FNAME_LEN 32
/* size of one inode entry */
#define INODE_ENTRY_SIZE 4
/* max number of inode entries */
#define INODE_CAPACITY (BLOCK_SIZE / INODE_ENTRY_SIZE)

#define FTYPE_RTC 0
#define FTYPE_DIR 1
#define FTYPE_FILE 2

#ifndef ASM

/* 64 Byte struct for directory entry */
typedef struct dentry_t {
    /* 32 char for file name */
    uint8_t fname[FNAME_LEN];
    /* file type:
     *  0 for rtc
     *  1 for directory
     *  2 for regular file
     */
    uint32_t ftype;
    /* index into inode array */
    uint32_t inode_idx;
    /* 24 byte reserved */
    uint32_t reserved[6];
} dentry_t;

/* 64 Byte struct for statistic entry */
typedef struct stat_entry_t {
    uint32_t num_dentry;
    uint32_t num_inode;
    uint32_t num_data_block;
    /* 52 byte reserved */
    uint32_t reserved[13];
} stat_entry_t;

/* 4kB boot block structure */
typedef struct boot_block_t {
    stat_entry_t stat_entry;
    dentry_t dentry_arr[DIR_CAPACITY];
} boot_block_t;

/* 4kB inode block structure */
typedef struct inode_t {
    /* actual number of data block numbers */
    uint32_t len;
    /* array for data_block_numbers;
     * deduct 1 because the first entry for len
     */
    uint32_t data_block_idx[INODE_CAPACITY - 1];
} inode_t;

/* 4kB data block structure */
typedef struct data_block_t {
    /* the data block is of size 4kB */
    uint8_t data[BLOCK_SIZE];
} data_block_t;

/* function declaration, see specification of each function */
int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry);
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry);
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length);
void filesys_init(void* fs_start_addr);

#endif /* ASM */

#endif /* _FILESYS_H */
