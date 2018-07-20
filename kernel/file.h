#ifndef _FILE_H
#define _FILE_H

#include "types.h"
#include "syscall.h"

/* Maximum number of open files per process */
#define MAX_FILES 8

/* File type constants */
#define FILE_TYPE_RTC 0
#define FILE_TYPE_DIR 1
#define FILE_TYPE_FILE 2
#define FILE_TYPE_MOUSE 3
#define FILE_TYPE_TAUX 4
#define FILE_TYPE_SOUND 5
#define FILE_TYPE_TTY 6
#define FILE_TYPE_NULL 7
#define FILE_TYPE_COUNT 8

/* File open modes passed to create() */
#define OPEN_NONE 0
#define OPEN_READ (1 << 0)
#define OPEN_WRITE (1 << 1)
#define OPEN_ALL (OPEN_READ | OPEN_WRITE)

/* Accepted fnctl() commands */
#define FCNTL_NONBLOCK 1

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
     * Reference count of the file. When this reaches zero,
     * the file object is released.
     */
    int refcnt;

    /* Read/write mode used to open the file. */
    int mode;

    /* Whether the file is in nonblocking mode. */
    bool nonblocking;

    /*
     * inode index of this file, unused if the file
     * does not refer to a physical file on disk.
     */
    uint32_t inode_idx;

    /*
     * File-private data, use is determined by driver.
     */
    void *private;
} file_obj_t;

/* File operations table */
struct file_ops_t {
    int (*open)(file_obj_t *file);
    int (*read)(file_obj_t *file, void *buf, int nbytes);
    int (*write)(file_obj_t *file, const void *buf, int nbytes);
    int (*close)(file_obj_t *file);
    int (*ioctl)(file_obj_t *file, int req, int arg);
};

/* Registers a file ops table with an associated type */
void file_register_type(int file_type, const file_ops_t *ops_table);

/* Returns the file object (array) for the executing process */
file_obj_t **get_executing_files(void);
file_obj_t *get_executing_file(int fd);

/* File object alloc/free/retain/release functions */
file_obj_t *file_obj_alloc(const file_ops_t *ops_table, int mode, bool open);
void file_obj_free(file_obj_t *file, bool close);
file_obj_t *file_obj_retain(file_obj_t *file);
void file_obj_release(file_obj_t *file);

/* File descriptor bind/unbind/rebind functions */
int file_desc_bind(file_obj_t **files, int fd, file_obj_t *file);
int file_desc_unbind(file_obj_t **files, int fd);
int file_desc_rebind(file_obj_t **files, int fd, file_obj_t *new_file);

/* Initializes/clones the specified file object array */
void file_init(file_obj_t **files);
void file_deinit(file_obj_t **files);
void file_clone(file_obj_t **new_files, file_obj_t **old_files);

/* Direct syscall handlers */
__cdecl int file_create(const char *filename, int mode);
__cdecl int file_open(const char *filename);
__cdecl int file_read(int fd, void *buf, int nbytes);
__cdecl int file_write(int fd, const void *buf, int nbytes);
__cdecl int file_close(int fd);
__cdecl int file_ioctl(int fd, int req, int arg);
__cdecl int file_dup(int srcfd, int destfd);
__cdecl int file_fcntl(int fd, int req, int arg);

#endif /* ASM */

#endif /* _FILE_H */
