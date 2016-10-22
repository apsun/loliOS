#include "filesys.h"

#define NUM_DENTRY (boot_block->stat_entry.num_dentry)
#define NUM_INODE_BLOCK (boot_block->stat_entry.num_inode)
#define NUM_DATA_BLOCK (boot_block->stat_entry.num_data_block)

#define DATA_BLOCK_ARR ((data_block_t *)(boot_block + NUM_INODE_BLOCK + 1))
#define INODE_BLOCK_ARR ((inode_t *)(boot_block + 1)) 


/* global pointer to store the address of the boot_block */
static boot_block_t* boot_block = NULL;
/* declaration of private util functions*/
static data_block_t* fs_get_data_block(inode_t* inode, uint32_t entry_idx);
static int32_t fs_compute_num_block(uint32_t file_size, uint32_t block_size);
static void fs_copy_within_block(data_block_t* data_block, uint32_t offset, uint8_t* buf, uint32_t length);
static int32_t fs_strncmp(const int8_t* tgt_fname, const int8_t* src_fname, uint32_t n);

/* save the starting address of the file system */
void
filesys_init(void* boot_block_addr)
{
	boot_block = (boot_block_t *) boot_block_addr;
}

/*
 * read_dentry_by_name (const uint8_t* fname, dentry_t* dentry)
 *  Inputs: A pointer fname to the string of characters of the name
 *    		of the file we need, and a dentry_t struct.
 *  Return Value: 0 on success and -1 on failure
 *  Function: The dentry_t struct passed with the function is filled with
 *  	      the corresponding entries of the given index. 
 *  		  The entries are file name, file type and inode #. 
 *  		  Returns -1 if dentry does not exist.
 */
int32_t
read_dentry_by_name (const uint8_t* fname, dentry_t* dentry)
{
	int i;
	for (i = 0; i < NUM_DENTRY; ++i) {		
		dentry_t* curr_dentry = (boot_block->dentry_arr) + i;
		char* curr_fname = curr_dentry->fname;
		/* since fname is not necessarily 0 terminated
		 * we need an additional check that if fname is 32 char
		 */
		if (!fs_strncmp(fname, curr_fname, FNAME_LEN)) {
			memcpy((void *)dentry, (void *)curr_dentry, DIR_ENTRY_SIZE);
			return 0;
		}
	}
	return -1;
}

/*         
 * read_dentry_by_index (uint32_t index, dentry_t* dentry)
 *  Inputs: An index to the boot block dir. entries and 
 *	 	    a dentry_t struct.
 *  Return Value: 0 on success and -1 on failure
 *  Function: The dentry_t struct passed with the function is filled with
 *      	  the corresponding entries of the given index. The entries are file name, 
 *    		  file type and inode #. Returns -1 if dentry does not exist.
 */
int32_t
read_dentry_by_index (uint32_t index, dentry_t* dentry)
{
	if (index < NUM_DENTRY)
	{
		dentry_t* curr_dentry = (boot_block->dentry_arr) + index;
		memcpy((void *)dentry, (void *)curr_dentry, DIR_ENTRY_SIZE);
		return 0;
	}
	return -1;
}
/*
 * read_data (uint32_t inode, uint32_t offset, uint8_t buf, uint32_t length)
 *   Inputs: 
 *       inode: inode # of the file to read from
 *       offset: byte offset of the start of the file
 *       buf: the buffer to copy data into
 *       length: the length of the data to read in bytes
 *   Return Value: number of bytes read, or -1 if error
 *   Function: fills the buffer with the bytes read and returns the buffer.  
 *   Returns the bytes read until the end of the file if the length was larger.
 */
int32_t
read_data (uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length)
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
	/* compute the start and end position in terms of data block index
	 * and start and end offset in start and end data block
	 */
	uint32_t start_entry = offset / BLOCK_SIZE;
	uint32_t start_offset = offset % BLOCK_SIZE;
	uint32_t end_entry = (offset + length) / BLOCK_SIZE;
	uint32_t end_offset = (offset + length) % BLOCK_SIZE;
	/* invalid inode entry index */
	if (start_entry >= num_entry) return -1;
	/* if length exceed the end of the file 
	 * set end entry the last entry and end offset the last byte
	 */	
	if (end_entry >= num_entry) {
		end_entry = num_entry - 1;
		end_offset = tgt_inode->len % BLOCK_SIZE ;
	}
	/* prepare to loop through entries */
	uint32_t curr_entry = start_entry;
	uint32_t curr_offset = start_offset;
	uint32_t bytes_read = 0;
	uint32_t bytes_to_read = BLOCK_SIZE - start_offset;
	data_block_t* tgt_data_block;
	while (curr_entry != end_entry) {
		/* get the data block pointer with given inode and 
		 * its entry which contain the index to data_block_array 
		 */
		tgt_data_block = fs_get_data_block(tgt_inode, curr_entry);
		if (!tgt_data_block) return -1;
		/* copy the data to the buffer */
		fs_copy_within_block(tgt_data_block, curr_offset, buf + bytes_read, bytes_to_read);
		/* loop tail */
		curr_offset = 0;
		bytes_to_read = BLOCK_SIZE;
		bytes_read += bytes_to_read;
		curr_entry ++;
	}
	/* check if the data block index in the givne inode entry is valid*/
	tgt_data_block = fs_get_data_block(tgt_inode, curr_entry);
	if (!tgt_data_block) return -1;
	/* compute number of bytes to read in last entry 
	 * and copy them to the buffer 
	 */
	bytes_to_read = end_offset - curr_offset;
	fs_copy_within_block(tgt_data_block, curr_offset, buf + bytes_read, bytes_to_read);
	bytes_read += bytes_to_read;
	return bytes_read;	
}
/* private utility functions */

/*
 * static data_block_t* fs_get_data_block(inode_t* inode, uint32_t entry_idx)
 *   Inputs: data_block_t* data_block_arr = data block array
 *           inode_t* inode = the inode where we get block index from
 *           uint32_t entry_idx = index of the target data block
 *   Return Value: pointer to target data block
 *   Function: get pointer the data block of the given index.
 *                return NULL if the index of the target block is 
 *                out of boundry
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
 *   Inputs: data_block_t* data_block_arr = data block array
 *           inode_t* inode = the inode where we get block index from
 *           uint32_t entry_idx = index of the target data block
 *   Return Value: pointer to target data block
 *   Function: get pointer the data block of the given index.
 * 			   return NULL if the index of the target block is 
 * 			   out of boundry
 */
static int32_t
fs_compute_num_block(uint32_t file_size, uint32_t block_size) {	
	return file_size / block_size + (file_size % block_size ? 0 : 1);
}
/*
 * fs_copy_within_block(data_block_t* data_block, uint32_t offset, uint8_t buf, uint32_t length)
 *   Inputs: data_block_t* data_block = pointer to the data block that we copy from
 *           uint32_t offset = the offset of the start of the data that we copy from
 *           uint32_t buf = the buffer that we copy to
 *           uint32_t length = length of the copied data in bytes
 *   Return Value: none
 *   Function: copies the specified length of data from the data_block_t struct into buffer
 */
static void
fs_copy_within_block(data_block_t* data_block, uint32_t offset, uint8_t* buf, uint32_t length) 
{	
	uint8_t* data_block_start = (data_block->data) + offset;
	memcpy((void *)buf, (void *)data_block_start, length);
}
/*
 * fs_copy_within_block(data_block_t* data_block, uint32_t offset, uint8_t buf, uint32_t length)
 *   Inputs: int8_t* tgt_fname = target array
 *           int8_t* src_fname = source array
 *           uint32_t n = 
 *   Return Value: 0 for success, -1 or any non-zero number on failure
 *   Function: string compare function similar to the one in lib.c but handles more than 32 bits.
 */
static int32_t
fs_strncmp(const int8_t* tgt_fname, const int8_t* src_fname, uint32_t n)
{
    int32_t i;
    for(i=0; i<n; i++) {
        if( (tgt_fname[i] != src_fname[i]) ||
                (tgt_fname[i] == '\0') /* || src_fname[i] == '\0' */ ) {
            /* The src_fname[i] == '\0' is unnecessary because of the short-circuit
             * semantics of 'if' expressions in C.  If the first expression
             * (tgt_fname[i] != src_fname[i]) evaluates to false, that is, if tgt_fname[i] ==
             * src_fname[i], then we only need to test either tgt_fname[i] or src_fname[i] for
             * '\0', since we know they are equal. */
            return tgt_fname[i] - src_fname[i];
        }
    }
    /* check if target file name is also 32 char */
    if (tgt_fname[i] == '\0')
    	return 0;
    return -1;
}

