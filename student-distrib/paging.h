#ifndef _PAGING_H
#define _PAGING_H

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

/* Page directory (defined in x86_desc.S) */
extern page_dir_entry_t page_dir[NUM_PDE];

/* Page table for first 4MB of memory (defined in x86_desc.S) */
extern page_table_entry_4kb_t page_table[NUM_PTE];

/* Enables paging */
void paging_enable(void);

/* Updates the process page */
void paging_update_process_page(int32_t pid);

#endif /* ASM */

#endif /* _PAGING_H */
