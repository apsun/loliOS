#ifndef _FILE_H
#define _FILE_H

#include "types.h"
#include "syscall.h"

#define MAX_FILES 8
#define FD_STDIN  0
#define FD_STDOUT 1

#ifndef ASM

typedef struct file_ops_t file_ops_t;

/* File file data */
typedef struct {
    /* inode index of the file */
    uint32_t inode_index;

    /* read() offset for the file */
    uint32_t offset;
} file_data_t;

/* Directory file data */
typedef struct {
    /* dentry index */
    uint32_t dentry_index;
} dir_data_t;

/* RTC file data */
typedef struct {
    /* Virtual RTC frequency */
    uint32_t frequency;
} rtc_data_t;

/* Mouse file data */
typedef struct {
    struct {
        /*
         * Flag bits
         * 0 - left button down?
         * 1 - right button down?
         * 2 - middle button down?
         * 3 - ignored
         * 4 - x sign
         * 5 - y sign
         * 6 - x overflow
         * 7 - y overflow
         */
        uint8_t flags;

        /*
         * Mouse delta x (if x sign bit is 1, then this
         * should be OR'd with 0xFFFFFF00)
         */
        uint8_t dx;

        /*
         * Mouse delta y (if y sign bit is 1, then this
         * should be OR'd with 0xFFFFFF00)
         */
        uint8_t dy;

        /* 1 if this is used, 0 if not */
        uint8_t valid;
    } input[16];
} mouse_data_t;

/* File object */
typedef struct {
    /* O/R/W/C file operation table for this file */
    file_ops_t *ops_table;

    /* Driver-specific data for the file */
    union {
        file_data_t file;
        dir_data_t dir;
        rtc_data_t rtc;
        mouse_data_t mouse;
    } data;

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
};

/* Initializes the specified file object array */
void file_init(file_obj_t *files);

/* Direct syscall handlers */
__cdecl int32_t file_open(const uint8_t *filename);
__cdecl int32_t file_read(int32_t fd, void *buf, int32_t nbytes);
__cdecl int32_t file_write(int32_t fd, const void *buf, int32_t nbytes);
__cdecl int32_t file_close(int32_t fd);

#endif /* ASM */

#endif /* _FILE_H */
