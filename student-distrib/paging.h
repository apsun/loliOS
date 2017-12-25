#ifndef _PAGING_H
#define _PAGING_H

#define SIZE_4KB 0
#define SIZE_4MB 1

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)

#define ISA_DMA_PAGE_START  0x000A0000
#define ISA_DMA_PAGE_END    0x000B0000

#define VIDEO_PAGE_START    0x000B8000
#define VIDEO_PAGE_END      0x000B9000

#define VIDMAP_PAGE_START   0x000B9000
#define VIDMAP_PAGE_END     0x000BA000

#define TERMINAL_PAGE_START 0x000BB000
/* End point is determined by the number of terminals */

#define KERNEL_PAGE_START   0x00400000
#define KERNEL_PAGE_END     0x00800000

#define USER_PAGE_START     0x08000000
#define USER_PAGE_END       0x08400000

#define HEAP_PAGE_START     0x08400000
#define HEAP_PAGE_END       0x10000000

#define MAX_HEAP_SIZE (HEAP_PAGE_END - HEAP_PAGE_START)
#define MAX_HEAP_PAGES (MAX_HEAP_SIZE / (MB(4)))

#ifndef ASM

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
} __packed pte_t;

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
} __packed pde_4kb_t;

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
} __packed pde_4mb_t;

/* Union of 4MB page table and 4KB page directory entries */
typedef union {
    pde_4mb_t dir_4mb;
    pde_4kb_t dir_4kb;
} pde_t;

/* Container for a process's heap info */
typedef struct {
    /* Size of the heap in bytes, might not be a multiple of 4MB */
    int32_t size;

    /* Number of valid entries in the array below */
    int32_t num_pages;

    /*
     * Holds a list of allocated heap pages, represented as
     * an index offset from HEAP_PAGE_START.
     */
    int32_t pages[MAX_HEAP_PAGES];
} paging_heap_t;

/* Enables paging */
void paging_enable(void);

/* Initializes a process heap */
void paging_heap_init(paging_heap_t *heap);

/* Expands or shrinks the data break */
int32_t paging_heap_sbrk(paging_heap_t *heap, int32_t delta);

/* Frees a process heap */
void paging_heap_destroy(paging_heap_t *heap);

/* Updates the process pages */
void paging_set_context(int32_t pid, paging_heap_t *heap);

/* Updates the vidmap page to point to the specified address */
void paging_update_vidmap_page(uint8_t *video_mem, bool present);

/* User-kernel copy functions */
bool is_user_accessible(const void *addr, int32_t nbytes, bool write);
bool strscpy_from_user(char *dest, const char *src, int32_t n);
bool copy_from_user(void *dest, const void *src, int32_t n);
bool copy_to_user(void *dest, const void *src, int32_t n);

#endif /* ASM */

#endif /* _PAGING_H */
