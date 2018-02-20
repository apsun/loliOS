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
#define FTYPE_SOUND 5

#ifndef ASM

/* Forward declaration */
typedef struct file_ops_t file_ops_t;

/* File object structure */
typedef struct {
    /* O/R/W/C file operation table for this file */
    const file_ops_t *ops_table;

    /*
     * inode index of this file, unused if the file
     * does not refer to a physical file on disk.
     */
    int inode_idx;

    /*
     * Offset information for repeated read operations.
     * For directories, this is the *index* of the next
     * file when enumerating. For files, this is the
     * *offset in bytes* of the current file position.
     * For the RTC, this holds the virtual interrupt
     * frequency.
     */
    int offset;

    /*
     * Whether this file object is currently used.
     */
    bool valid;
} file_obj_t;

/* File operations table */
struct file_ops_t {
    int (*open)(const char *filename, file_obj_t *file);
    int (*read)(file_obj_t *file, void *buf, int nbytes);
    int (*write)(file_obj_t *file, const void *buf, int nbytes);
    int (*close)(file_obj_t *file);
    int (*ioctl)(file_obj_t *file, int req, int arg);
};

/* Initializes the specified file object array */
void file_init(file_obj_t *files);

/* Direct syscall handlers */
__cdecl int file_open(const char *filename);
__cdecl int file_read(int fd, void *buf, int nbytes);
__cdecl int file_write(int fd, const void *buf, int nbytes);
__cdecl int file_close(int fd);
__cdecl int file_ioctl(int fd, int req, int arg);

#endif /* ASM */

#endif /* _FILE_H */
