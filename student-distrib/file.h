#ifndef _FILE_H
#define _FILE_H

#include "types.h"

#define MAX_FILES 8
#define FD_STDIN  0
#define FD_STDOUT 1

#ifndef ASM

typedef struct file_ops_t file_ops_t;

/* File object */
typedef struct {
    /* O/R/W/C file operation table for this file */
    file_ops_t *ops_table;

    /* inode index of this file, unused if the file
     * does not refer to a physical file on disk.
     */
    uint32_t inode_idx;

    /* Offset information for repeated read operations.
     * For directories, this is the *index* of the next
     * file when enumerating. For files, this is the
     * *offset in bytes* of the current file position.
     * For all other types, this is unused.
     */
    uint32_t offset;

    /*
     * Whether this file object is currently used.
     */
    uint32_t valid : 1;
} file_obj_t;

/* File operations table */
struct file_ops_t {
    int32_t (*open)(const uint8_t *filename, file_obj_t *file);
    int32_t (*read)(file_obj_t *file, void *buf, int32_t nbytes);
    int32_t (*write)(file_obj_t *file, const void *buf, int32_t nbytes);
    int32_t (*close)(file_obj_t *file);
};

/* Initializes file info for the current process */
void file_init(void);

/* Direct syscall handlers */
int32_t file_open(const uint8_t *filename);
int32_t file_read(int32_t fd, void *buf, int32_t nbytes);
int32_t file_write(int32_t fd, const void *buf, int32_t nbytes);
int32_t file_close(int32_t fd);

#endif /* ASM */

#endif /* _FILE_H */
