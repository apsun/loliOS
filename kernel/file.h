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
    /*
     * File operations table for this file.
     */
    const file_ops_t *ops_table;

    /*
     * Index (aka file descriptor) of this file object.
     * Set to -1 if the file is unused.
     */
    int fd;

    /*
     * inode index of this file, unused if the file
     * does not refer to a physical file on disk.
     */
    int inode_idx;

    /*
     * File-private data, use is determined by driver.
     */
    int private;
} file_obj_t;

/* File operations table */
struct file_ops_t {
    int (*open)(const char *filename, file_obj_t *file);
    int (*read)(file_obj_t *file, void *buf, int nbytes);
    int (*write)(file_obj_t *file, const void *buf, int nbytes);
    int (*close)(file_obj_t *file);
    int (*ioctl)(file_obj_t *file, int req, int arg);
};

/* Returns all file objects for the executing process */
file_obj_t *get_executing_files(void);

/* Returns a file object for the executing process */
file_obj_t *get_executing_file(int fd);

/* Allocates a new file obj (returns NULL if too many open files) */
file_obj_t *file_obj_alloc(void);

/* Frees a file object */
void file_obj_free(file_obj_t *file);

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