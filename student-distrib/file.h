#ifndef _FILE_H
#define _FILE_H

#include "types.h"
#include "syscall.h"

/* Maximum number of open files per process */
#define MAX_FILES 8

/* File type constants */
#define FTYPE_RTC 0
#define FTYPE_DIR 1
#define FTYPE_FILE 2
#define FTYPE_MOUSE 3
#define FTYPE_TAUX 4

#ifndef ASM

/* Forward declaration */
typedef struct file_ops_t file_ops_t;

/* File object structure */
typedef struct {
    /* O/R/W/C file operation table for this file */
    struct file_ops_t *ops_table;

    /*
     * inode index of this file, unused if the file
     * does not refer to a physical file on disk.
     */
    uint32_t inode_idx;

    /*
     * Offset information for repeated read operations.
     * For directories, this is the *index* of the next
     * file when enumerating. For files, this is the
     * *offset in bytes* of the current file position.
     * For the RTC, this holds the virtual interrupt
     * frequency. For the mouse, this holds the index
     * of the corresponding input buffer. For the taux
     * controller, this holds the 
     */
    uint32_t offset;

    /*
     * Whether this file object is currently used.
     */
    bool valid;
} file_obj_t;

/* File operations table */
struct file_ops_t {
    int32_t (*open)(const uint8_t *filename, file_obj_t *file);
    int32_t (*read)(file_obj_t *file, void *buf, int32_t nbytes);
    int32_t (*write)(file_obj_t *file, const void *buf, int32_t nbytes);
    int32_t (*close)(file_obj_t *file);
    int32_t (*ioctl)(file_obj_t *file, uint32_t req, uint32_t arg);
};

/* Initializes the specified file object array */
void file_init(file_obj_t *files);

/* Direct syscall handlers */
__cdecl int32_t file_open(const uint8_t *filename);
__cdecl int32_t file_read(int32_t fd, void *buf, int32_t nbytes);
__cdecl int32_t file_write(int32_t fd, const void *buf, int32_t nbytes);
__cdecl int32_t file_close(int32_t fd);
__cdecl int32_t file_ioctl(int32_t fd, uint32_t req, uint32_t arg);

#endif /* ASM */

#endif /* _FILE_H */
