#ifndef _PAGING_H
#define _PAGING_H

#include "types.h"

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)

#define VIDEO_PAGE_START    0x000B8000
#define VIDEO_PAGE_END      0x000B9000

#define VIDMAP_PAGE_START   0x000B9000
#define VIDMAP_PAGE_END     0x000BA000

#define SB16_PAGE_START     0x00100000
#define SB16_PAGE_END       0x00102000

#define TERMINAL_PAGE_START 0x00102000
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

/* Container for a process's heap info */
typedef struct {
    /* Size of the heap in bytes, might not be a multiple of page size */
    int size;

    /* Number of valid entries in the array below */
    int num_pages;

    /* List of physical pages that are allocated for this heap */
    int pages[MAX_HEAP_PAGES];
} paging_heap_t;

/* Enables paging */
void paging_enable(void);

/* Allocates a physical 4MB page */
int paging_page_alloc(void);

/* Deallocates a physical 4MB page */
void paging_page_free(int pfn);

/* Maps a virtual page to a physical page */
void paging_page_map(int vfn, int pfn, bool user);

/* Unmaps a virtual page */
void paging_page_unmap(int vfn);

/* Initializes a process heap */
void paging_heap_init(paging_heap_t *heap);

/* Expands or shrinks the data break */
int paging_heap_sbrk(paging_heap_t *heap, int delta);

/* Frees a process heap */
void paging_heap_destroy(paging_heap_t *heap);

/* Updates the process pages */
void paging_set_context(int pfn, paging_heap_t *heap);

/* Updates the vidmap page to point to the specified address */
void paging_update_vidmap_page(uint8_t *video_mem, bool present);

/* User-kernel copy functions */
bool is_user_accessible(const void *addr, int nbytes, bool write);
bool strscpy_from_user(char *dest, const char *src, int n);
bool copy_from_user(void *dest, const void *src, int n);
bool copy_to_user(void *dest, const void *src, int n);

#endif /* ASM */

#endif /* _PAGING_H */