#ifndef _PAGING_H
#define _PAGING_H

/* number of entries in page directory */
#define NUM_PDE 1024

/* number of entries in page table */
#define NUM_PTE 1024

#define VIDEO_PAGE_START  0x000B8000
#define VIDEO_PAGE_END    0x000B9000

#define KERNEL_PAGE_START 0x00400000
#define KERNEL_PAGE_END   0x00800000

#define USER_PAGE_START   0x08000000
#define USER_PAGE_END     0x08400000

#define VIDMAP_PAGE_START 0x084B8000
#define VIDMAP_PAGE_END   0x084B9000

#ifndef ASM

#include "x86_desc.h"
#include "types.h"

/* Structure for 4KB page table entry */
typedef struct {
    uint8_t present        : 1;
    uint8_t write          : 1;
    uint8_t user           : 1;
    uint8_t write_through  : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed       : 1;
    uint8_t dirty          : 1;
    uint8_t page_attr_idx  : 1;
    uint8_t global         : 1;
    uint8_t avail          : 3;
    uint32_t base_addr     : 20;
} __attribute__((packed)) page_table_entry_4kb_t;

/* Structure for 4KB page directory entry */
typedef struct {
    uint8_t present        : 1;
    uint8_t write          : 1;
    uint8_t user           : 1;
    uint8_t write_through  : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed       : 1;
    uint8_t reserved       : 1;
    uint8_t size           : 1;
    uint8_t global         : 1;
    uint8_t avail          : 3;
    uint32_t base_addr     : 20;
} __attribute__((packed)) page_dir_entry_4kb_t;

/* Structure for 4MB page directory entry */
typedef struct {
    uint8_t present        : 1;
    uint8_t write          : 1;
    uint8_t user           : 1;
    uint8_t write_through  : 1;
    uint8_t cache_disabled : 1;
    uint8_t accessed       : 1;
    uint8_t dirty          : 1;
    uint8_t size           : 1;
    uint8_t global         : 1;
    uint8_t avail          : 3;
    uint8_t page_attr_idx  : 1;
    uint16_t reserved      : 9;
    uint16_t base_addr     : 10;
} __attribute__((packed)) page_dir_entry_4mb_t;

/* Union of 4MB page table and 4KB page directory entries */
typedef union {
    page_dir_entry_4mb_t dir_4mb;
    page_dir_entry_4kb_t dir_4kb;
} page_dir_entry_t;

/* Enables paging */
void paging_enable(void);

/* Updates the process page */
void paging_update_process_page(int32_t pid);

/* Updates the vidmap page */
uint8_t *paging_update_vidmap_page(bool present);

#endif /* ASM */

#endif /* _PAGING_H */
