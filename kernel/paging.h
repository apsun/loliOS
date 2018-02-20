#ifndef _PAGING_H
#define _PAGING_H

#include "types.h"

#define SIZE_4KB 0
#define SIZE_4MB 1

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)

#define VIDEO_PAGE_START    0x000B8000
#define VIDEO_PAGE_END      0x000B9000

#define VIDMAP_PAGE_START   0x000B9000
#define VIDMAP_PAGE_END     0x000BA000

#define TERMINAL_PAGE_START 0x000BB000
/* End point is determined by the number of terminals */

#define SB16_PAGE_START     0x003C0000
#define SB16_PAGE_END       0x003C2000

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
    /* Size of the heap in bytes, might not be a multiple of 4MB */
    int size;

    /* Number of valid entries in the array below */
    int num_pages;

    /*
     * Holds a list of allocated heap pages, represented as
     * an index offset from HEAP_PAGE_START.
     */
    int pages[MAX_HEAP_PAGES];
} paging_heap_t;

/* Enables paging */
void paging_enable(void);

/* Initializes a process heap */
void paging_heap_init(paging_heap_t *heap);

/* Expands or shrinks the data break */
int paging_heap_sbrk(paging_heap_t *heap, int delta);

/* Frees a process heap */
void paging_heap_destroy(paging_heap_t *heap);

/* Updates the process pages */
void paging_set_context(int pid, paging_heap_t *heap);

/* Updates the vidmap page to point to the specified address */
void paging_update_vidmap_page(uint8_t *video_mem, bool present);

/* User-kernel copy functions */
bool is_user_accessible(const void *addr, int nbytes, bool write);
bool strscpy_from_user(char *dest, const char *src, int n);
bool copy_from_user(void *dest, const void *src, int n);
bool copy_to_user(void *dest, const void *src, int n);

#endif /* ASM */

#endif /* _PAGING_H */
