#include "filesys.h"

/* macro to get info from statistic entry */
#define NUM_DENTRY (boot_block->stat_entry.num_dentry)
#define NUM_INODE_BLOCK (boot_block->stat_entry.num_inode)
#define NUM_DATA_BLOCK (boot_block->stat_entry.num_data_block)

/* starting address of data block and inode array */
#define INODE_BLOCK_ARR ((inode_t *)(boot_block + 1))
#define DATA_BLOCK_ARR ((data_block_t *)(boot_block + NUM_INODE_BLOCK + 1))

/* global pointer to store the address of the boot_block */
static boot_block_t* boot_block = NULL;

/* declaration of private util functions*/
static data_block_t* fs_get_data_block(inode_t* inode, uint32_t entry_idx);
static int32_t fs_compute_num_block(uint32_t file_size, uint32_t block_size);
static void fs_copy_within_block(data_block_t* data_block, uint32_t offset, uint8_t* buf, uint32_t length);
static int32_t fs_cmp_fname(const uint8_t* tgt_fname, const uint8_t* src_fname);

/* save the starting address of the file system */
void
filesys_init(void* boot_block_addr)
{
    boot_block = (boot_block_t *)boot_block_addr;
}

/*
 * Inputs: a pointer to dentry struct of the file we want to read
 * Return Value: return the file size
 * Function: get the size in Byte of this file
 */
uint32_t filesys_get_fsize(dentry_t* dentry){
    inode_t* tgt_inode = INODE_BLOCK_ARR + dentry->inode_idx;
    return tgt_inode->len;
}


/*
 * Inputs: A pointer fname to the string of characters of the name
 *         of the file we need, and a dentry_t struct.
 * Return Value: 0 on success and -1 on failure
 * Function: The dentry_t struct passed with the function is filled with
 *           the corresponding entries of the given index.
 *           The entries are file name, file type and inode #.
 *           Returns -1 if dentry does not exist.
 */
int32_t
read_dentry_by_name(const uint8_t* fname, dentry_t* dentry)
{
    uint32_t i;
    for (i = 0; i < NUM_DENTRY; ++i) {
        dentry_t* curr_dentry = &boot_block->dentry_arr[i];
        uint8_t* curr_fname = curr_dentry->fname;
        if (fs_cmp_fname(fname, curr_fname) == 0) {
            *dentry = *curr_dentry;
            return 0;
        }
    }
    return -1;
}

/*
 * Inputs: An index to the boot block dir. entries and
 *         a dentry_t struct.
 * Return Value: 0 on success and -1 on failure
 * Function: The dentry_t struct passed with the function is filled with
 *           the corresponding entries of the given index. The entries are file name,
 *           file type and inode #. Returns -1 if dentry does not exist.
 */
int32_t
read_dentry_by_index(uint32_t index, dentry_t* dentry)
{
    if (index < NUM_DENTRY) {
        *dentry = boot_block->dentry_arr[index];
        return 0;
    }
    return -1;
}

/*
 * Inputs:
 *     inode: inode # of the file to read from
 *     offset: byte offset of the start of the file
 *     buf: the buffer to copy data into
 *     length: the length of the data to read in bytes
 * Return Value: number of bytes read, or -1 if error
 * Function: fills the buffer with the bytes read and returns the buffer.
 * Returns the bytes read until the end of the file if the length was larger.
 */
int32_t
read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length)
{
    if (inode >= NUM_INODE_BLOCK) {
        return -1;
    }

    /* offset 1 block to get the starting addr of
     * inode block array
     */
    inode_t* tgt_inode = INODE_BLOCK_ARR + inode;

    /* compute the number of block_indices that is valid in target inode */
    uint32_t num_entry = fs_compute_num_block(tgt_inode->len, BLOCK_SIZE);

    /* check if num_entry exceed max number of entries
     * an inode can have;
     * deduct 1 for num_entry itself
     */
    if (num_entry > INODE_CAPACITY - 1) return -1;

    /* invalid offset */
    if (offset > tgt_inode->len) return -1;

    /* if length exceed the end of the file
     * reset the length such that offset + length reach the last byte of file
     */
    if (offset + length > tgt_inode->len)
        length = tgt_inode->len - offset;

    /* compute the start and end position in terms of data block index
     * and start and end offset in start and end data block
     */
    uint32_t start_entry = offset / BLOCK_SIZE;
    uint32_t start_offset = offset % BLOCK_SIZE;
    uint32_t end_entry = (offset + length) / BLOCK_SIZE;
    uint32_t end_offset = (offset + length) % BLOCK_SIZE;
    /* prepare to loop through entries */
    uint32_t curr_entry = start_entry;
    uint32_t curr_offset = start_offset;
    uint32_t bytes_read = 0;
    uint32_t bytes_to_read = BLOCK_SIZE - start_offset;
    data_block_t* tgt_data_block;

    while (curr_entry <= end_entry) {
        /* get the data block pointer with given inode and
         * its entry which contain the index to data_block_array
         */
        tgt_data_block = fs_get_data_block(tgt_inode, curr_entry);
        if (!tgt_data_block) return -1;

        /* if current entry is the same with end entry, we are reading the final block */
        if (curr_entry == end_entry)
            bytes_to_read = end_offset - curr_offset;

        /* copy the data to the buffer */
        fs_copy_within_block(tgt_data_block, curr_offset, buf + bytes_read, bytes_to_read);

        /* loop tail */
        curr_offset = 0;
        bytes_read += bytes_to_read;
        bytes_to_read = BLOCK_SIZE;
        curr_entry++;
    }
    return bytes_read;
}

/* private utility functions */

/*
 * Inputs: inode_t* inode = the inode where we get block index from
 *         uint32_t entry_idx = index of the target data block
 * Return Value: pointer to target data block
 * Function: get pointer the data block of the given index.
 *              return NULL if the index of the target block is
 *              out of boundry
 */
static data_block_t*
fs_get_data_block(inode_t* inode, uint32_t entry_idx)
{
    uint32_t data_block_idx = inode->data_block_idx[entry_idx];
    /* check if invalid data block index */
    if (data_block_idx >= NUM_DATA_BLOCK)
        return NULL;
    return DATA_BLOCK_ARR + data_block_idx;
}

/*
 * Inputs: uint32_t file_size = Size of the file
 *         uint32_t block_size = Size of a block
 * Function: Returns the number of blocks file_size would fit in as an
 *           integer.
 */
static int32_t
fs_compute_num_block(uint32_t file_size, uint32_t block_size)
{
    return file_size / block_size + (file_size % block_size ? 1 : 0);
}

/*
 * Inputs: data_block_t* data_block = pointer to the data block that we copy from
 *         uint32_t offset = the offset of the start of the data that we copy from
 *         uint32_t *buf = the buffer that we copy to
 *         uint32_t length = length of the copied data in bytes
 * Return Value: none
 * Function: copies the specified length of data from the data_block_t struct into buffer
 */
static void
fs_copy_within_block(data_block_t* data_block, uint32_t offset, uint8_t* buf, uint32_t length)
{
    uint8_t* data_block_start = data_block->data + offset;
    memcpy((void *)buf, (void *)data_block_start, length);
}

/*
 * Inputs: int8_t* tgt_fname = target array
 *         int8_t* src_fname = source array
 * Return Value: 0 for success, -1 or any non-zero number on failure
 * Function: filename comparison function (adapted from strncmp)
 */
static int32_t
fs_cmp_fname(const uint8_t* tgt_fname, const uint8_t* src_fname)
{
    int32_t i;
    for (i = 0; i < FNAME_LEN; i++) {
        if ((tgt_fname[i] != src_fname[i]) || (tgt_fname[i] == '\0')) {
            return tgt_fname[i] - src_fname[i];
        }
    }

    /* We checked all 32 chars, now check if the target filename
     * is also 32 chars long (meaning a \0 at the 33rd byte),
     * otherwise the filenames don't actually match */
    if (tgt_fname[i] == '\0') {
        return 0;
    }

    return -1;
}
