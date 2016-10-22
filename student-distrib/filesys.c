#include "filesys.h"

#define NUM_DENTRY (boot_block->stat_entry.num_dentry)
#define NUM_INODE_BLOCK (boot_block->stat_entry.num_inode)
#define NUN_DATA_BLOCK (boot_block->stat_entry.num_data_block)
#define GET_DATA_BLOCK_ARR() 						\
do {												\
	((data_block_t *)(boot_block + NUM_INODE + 1)) 	\
} while (0);

static boot_block_t* boot_block = NULL;

/* save the starting address of the file system */
void
filesys_init(void* boot_block_addr)
{
	boot_block = (boot_block_t *) boot_block_addr;
}

/*
 *   Inputs: data_block_t* data_block_arr = data block array
 *           inode_t* inode = the inode where we get block index from
 *           uint32_t entry_idx = index of the target data block
 *   Return Value: pointer to data dest
 *   Function: copy n bytes of src to dest
 */
int32_t
read_dentry_by_name (const uint8_t* fname, dentry_t* dentry)
{
	int i;
	for (i = 0; i < NUM_DENTRY; ++i) {		
		dentry_t* curr_dentry = (boot_block->dentry_arr) + i;
		char curr_fname[FNAME_LEN] = curr_dentry->fname
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

int32_t
read_dentry_by_index (uint32_t index, dentry_t* dentry)
{
	uint32_t i;
	for (i = 0; i < NUM_DENTRY; ++i) {		
		dentry_t* curr_dentry = (boot_block->dentry_arr) + i;
		if (index == curr_dentry->inode_idx) {
			memcpy((void *)dentry, (void *)curr_dentry, DIR_ENTRY_SIZE);
			return 0;
		}
	}
	return -1;
}

int32_t
read_data (uint32_t inode, uint32_t offset, uint8_t buf, uint32_t length)
{
	uint32_t num_inode = boot_block->stat_entry.num_inode;
	uint32_t num_data_block = boot_block->stat_entry.num_data_block;
	
	if (inode >= NUM_INODE)	{
		return -1;
	}
	/* offset 1 block to get the starting addr of 
	 * inode block array
	 */
	inode_t* inode_arr = boot_block + 1;
	inode_t* tgt_inode = inode_arr + inode;
	data_block_t* data_block_arr = inode_arr + NUM_INODE;
	data_block_t* tgt_data_block;

	uint32_t num_entry = fs_compute_num_block(tgt_inode->len, BLOCK_SIZE);

	uint32_t start_entry = offset / BLOCK_SIZE;
	uint32_t start_offset = offset % BLOCK_SIZE;
	uint32_t end_entry = (offset + length) / BLOCK_SIZE;
	uint32_t end_offset = (offset + length) % BLOCK_SIZE;
	/* invalid inode entry index */
	if (start_entry >= num_entry) return -1;		
	
	if (end_entry >= num_entry) {
		end_entry = num_entry - 1;
		end_offset = BLOCK_SIZE;
	}
	uint32_t curr_entry = start_entry;
	uint32_t curr_offset = start_offset;
	uint32_t bytes_read = 0;
	uint32_t bytes_to_read = BLOCK_SIZE - start_offset;	
	while (curr_entry != end_entry) {
		tgt_data_block = fs_get_data_block(data_block_arr, tgt_inode, curr_entry);
		if (!tgt_data_block) return -1;
		
		fs_copy_within_block(tgt_data_block, curr_offset, buf, bytes_to_read);
		curr_offset = 0;
		bytes_to_read = BLOCK_SIZE;
		bytes_read += bytes_to_read;
		curr_entry ++;
	}
	tgt_data_block = fs_get_data_block(data_block_arr, tgt_inode, curr_entry);
		if (!tgt_data_block) return -1;

	bytes_to_read = end_offset - curr_offset;
	fs_copy_within_block(tgt_data_block, curr_offset, buf, bytes_to_read);
	bytes_read += bytes_to_read;
	return bytes_read;
	
}
/* private utility functions */

/*
 *   Inputs: data_block_t* data_block_arr = data block array
 *           inode_t* inode = the inode where we get block index from
 *           uint32_t entry_idx = index of the target data block
 *   Return Value: pointer to target data block
 *   Function: get pointer the data block of the given index.
 * 			   return NULL if the index of the target block is 
 * 			   out of boundry
 */
static data_block_t*
fs_get_data_block(data_block_t* data_block_arr, inode_t* inode, uint32_t entry_idx)
{

	data_block_t* data_block_arr = fs_get_data_block_arr();
	uint32_t num_data_block = boot_block->stat_entry.num_data_block;
	data_block_idx = tgt_inode->data_block_idx[curr_entry];
	/* check if invalid data block index */
	if (data_block_idx >= num_data_block) 
		return NULL;
	return data_block_arr + data_block_idx;
}

static data_block_t*
fs_get_data_block_arr(void)
{
	/* offset 1 for the boot block */
	data_block_t* data_block_arr = (data_block_t *)(boot_block + NUM_INODE + 1);
	return data_block_arr;
}

static inode_t*
fs_get_inode_arr(void)
{
	/* offset 1 for the boot block */
	inode_t* inode_arr = (inode_t *)(boot_block + 1);
	return inode_arr;
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

static int32_t
fs_copy_within_block(data_block_t* data_block, uint32_t offset, uint8_t buf, uint32_t length) 
{	
	uint8_t* data_block_start = (data_block->data) + offset;
	memcpy((void *)buf, (void *)data_block_start, length);
}

static int32_t
fs_strncmp(const int8_t* tgt_fname, const int8_t* src_fname, uint32_t n)
{
    int32_t i;
    for(i=0; i<n; i++) {
        if( (s1[i] != s2[i]) ||
                (s1[i] == '\0') /* || s2[i] == '\0' */ ) {
            /* The s2[i] == '\0' is unnecessary because of the short-circuit
             * semantics of 'if' expressions in C.  If the first expression
             * (s1[i] != s2[i]) evaluates to false, that is, if s1[i] ==
             * s2[i], then we only need to test either s1[i] or s2[i] for
             * '\0', since we know they are equal. */
            return s1[i] - s2[i];
        }
    }
    /* check if target file name is also 32 char */
    if (tgt_fname[i] == '\0')
    	return 0;
    return -1;
}

