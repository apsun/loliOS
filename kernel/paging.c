#include "paging.h"
#include "debug.h"
#include "terminal.h"
#include "bitmap.h"
#include "list.h"

/* PDE size field values */
#define SIZE_4KB 0
#define SIZE_4MB 1

/* Number of 4MB pages in the system */
#define TOTAL_PAGES 1024

/* Structure for 4KB page table entry */
typedef struct {
    uint32_t present        : 1;
    uint32_t write          : 1;
    uint32_t user           : 1;
    uint32_t write_through  : 1;
    uint32_t cache_disabled : 1;
    uint32_t accessed       : 1;
    uint32_t dirty          : 1;
    uint32_t page_attr_idx  : 1;
    uint32_t global         : 1;
    uint32_t avail          : 3;
    uint32_t base_addr      : 20;
} __packed pte_t;

/* Structure for 4KB page directory entry */
typedef struct {
    uint32_t present        : 1;
    uint32_t write          : 1;
    uint32_t user           : 1;
    uint32_t write_through  : 1;
    uint32_t cache_disabled : 1;
    uint32_t accessed       : 1;
    uint32_t reserved       : 1;
    uint32_t size           : 1;
    uint32_t global         : 1;
    uint32_t avail          : 3;
    uint32_t base_addr      : 20;
} __packed pde_4kb_t;

/* Structure for 4MB page directory entry */
typedef struct {
    uint32_t present        : 1;
    uint32_t write          : 1;
    uint32_t user           : 1;
    uint32_t write_through  : 1;
    uint32_t cache_disabled : 1;
    uint32_t accessed       : 1;
    uint32_t dirty          : 1;
    uint32_t size           : 1;
    uint32_t global         : 1;
    uint32_t avail          : 3;
    uint32_t page_attr_idx  : 1;
    uint32_t reserved       : 9;
    uint32_t base_addr      : 10;
} __packed pde_4mb_t;

/* Union of 4MB page table and 4KB page directory entries */
typedef union {
    pde_4mb_t dir_4mb;
    pde_4kb_t dir_4kb;
} pde_t;

/* Page directory */
__aligned(KB(4))
static pde_t page_dir[1024];

/* Page table for first 4MB of memory */
__aligned(KB(4))
static pte_t page_table[1024];

/*
 * We don't bother with free lists or any of that fancy stuff
 * in our page allocator. Just use a single flat bitmap with
 * one bit representing one 4KB page in the system.
 */
static bitmap_declare(allocated_pages, int, TOTAL_PAGES);

#define TO_4MB_BASE(x) (((uint32_t)(x)) >> 22)
#define TO_4KB_BASE(x) (((uint32_t)(x)) >> 12)
#define TO_DIR_INDEX(x) (((uint32_t)(x)) >> 22)
#define TO_TABLE_INDEX(x) ((((uint32_t)(x)) >> 12) & 0x3ff)

/* Helpful macros to access page table stuff */
#define DIR_4KB(addr) (&page_dir[TO_DIR_INDEX(addr)].dir_4kb)
#define DIR_4MB(addr) (&page_dir[TO_DIR_INDEX(addr)].dir_4mb)
#define TABLE(addr) (&page_table[TO_TABLE_INDEX(addr)])

/* Initializes the page directory for the first 4MB of memory */
static void
paging_init_common(void)
{
    pde_4kb_t *dir = DIR_4KB(0);
    dir->present = 1;
    dir->write = 1;
    dir->user = 1; /* Needed for vidmap page */
    dir->size = SIZE_4KB;
    dir->base_addr = TO_4KB_BASE(page_table);
    bitmap_set(allocated_pages, 0);
}

/* Initializes the page directory for the 4MB kernel page */
static void
paging_init_kernel(void)
{
    pde_4mb_t *dir = DIR_4MB(KERNEL_PAGE_START);
    dir->present = 1;
    dir->write = 1;
    dir->user = 0;
    dir->size = SIZE_4MB;
    dir->base_addr = TO_4MB_BASE(KERNEL_PAGE_START);
    bitmap_set(allocated_pages, 1);
}

/* Initializes the 4KB video memory pages */
static void
paging_init_video(void)
{
    /* Global (VGA) video memory page */
    pte_t *global_table = TABLE(VIDEO_PAGE_START);
    global_table->present = 1;
    global_table->write = 1;
    global_table->user = 0;
    global_table->base_addr = TO_4KB_BASE(VIDEO_PAGE_START);

    /* Virtual video memory pages, one per terminal */
    int i;
    for (i = 0; i < NUM_TERMINALS; ++i) {
        uint32_t term_addr = TERMINAL_PAGE_START + i * KB(4);
        pte_t *term_table = TABLE(term_addr);
        term_table->present = 1;
        term_table->write = 1;
        term_table->user = 0;
        term_table->base_addr = TO_4KB_BASE(term_addr);
    }
}

/* Initializes the 4MB user page */
static void
paging_init_user(void)
{
    pde_4mb_t *dir = DIR_4MB(USER_PAGE_START);
    dir->present = 1;
    dir->write = 1;
    dir->user = 1;
    dir->size = SIZE_4MB;
}

/* Initializes the 4KB vidmap page */
static void
paging_init_vidmap(void)
{
    pte_t *table = TABLE(VIDMAP_PAGE_START);
    table->present = 0;
    table->write = 1;
    table->user = 1;
}

/* Initializes the SB16 DMA pages */
static void
paging_init_sb16(void)
{
    uint32_t addr = SB16_PAGE_START;
    while (addr < SB16_PAGE_END) {
        pte_t *table = TABLE(addr);
        table->present = 1;
        table->write = 1;
        table->user = 0;
        table->base_addr = TO_4KB_BASE(addr);
        addr += KB(4);
    }
}

/* Initializes the 4MB heap pages */
static void
paging_init_heap(void)
{
    int i;
    for (i = 0; i < MAX_HEAP_PAGES; ++i) {
        pde_4mb_t *dir = DIR_4MB(HEAP_PAGE_START + i * MB(4));
        dir->present = 0;
        dir->write = 1;
        dir->user = 1;
        dir->size = SIZE_4MB;
    }
}

/*
 * Sets the control registers to enable paging.
 * This must be called *after* all the setup is complete.
 */
static void
paging_init_registers(void)
{
    asm volatile(
        /* Point PDR to page directory */
        "movl %%cr3, %%eax;"
        "andl $0x00000fff, %%eax;"
        "orl %0, %%eax;"
        "movl %%eax, %%cr3;"

        /* Enable 4MB pages */
        "movl %%cr4, %%eax;"
        "orl $0x00000010, %%eax;"
        "movl %%eax, %%cr4;"

        /* Enable paging (this must come last!) */
        "movl %%cr0, %%eax;"
        "orl $0x80000000, %%eax;"
        "movl %%eax, %%cr0;"
        :
        : "g"(&page_dir)
        : "eax", "cc");
}

/* Flushes the TLB */
static void
paging_flush_tlb(void)
{
    asm volatile(
        "movl %%cr3, %%eax;"
        "movl %%eax, %%cr3;"
        :
        :
        : "eax");
}

/* Enables paging. */
void
paging_enable(void)
{
    /* Ensure page table arrays are 4096-byte aligned */
    assert(((uint32_t)page_dir   & 0xfff) == 0);
    assert(((uint32_t)page_table & 0xfff) == 0);

    /* Initialize page table entries */
    paging_init_common();
    paging_init_kernel();
    paging_init_video();
    paging_init_user();
    paging_init_vidmap();
    paging_init_sb16();
    paging_init_heap();

    /* Set control registers */
    paging_init_registers();
}

/*
 * Allocates a new 4MB page. Returns its page frame number.
 * If no free pages are available, returns -1. This does
 * not modify the page directory; it only prevents this
 * function from returning the same address in the future
 * until paging_page_free() is called.
 */
static int
paging_page_alloc(void)
{
    /* Find a free page... */
    int pfn;
    bitmap_find_zero(allocated_pages, pfn);
    if (pfn >= MAX_HEAP_PAGES) {
        return -1;
    }

    /* ... and mark it as allocated. */
    bitmap_set(allocated_pages, pfn);
    return pfn;
}

/*
 * Frees a 4MB page obtained from paging_page_alloc().
 */
static void
paging_page_free(int pfn)
{
    bitmap_clear(allocated_pages, pfn);
}

/*
 * Allocates a new 4MB heap page on behalf of the
 * calling process. This will modify the page directory.
 */
static int
paging_heap_alloc(int vi)
{
    /* Allocate a new page */
    int pi = paging_page_alloc();
    if (pi < 0) {
        return -1;
    }

    uint32_t vaddr = HEAP_PAGE_START + vi * MB(4);
    uint32_t paddr = HEAP_PAGE_START + pi * MB(4);

    /* Update PDE */
    pde_4mb_t *entry = DIR_4MB(vaddr);
    assert(!entry->present);
    entry->present = 1;
    entry->base_addr = TO_4MB_BASE(paddr);

    /* Zero out page for security */
    paging_flush_tlb();
    memset_dword((void *)vaddr, 0, MB(4) / 4);
    return pi;
}

/*
 * Frees a 4MB heap page previously allocated using
 * paging_heap_alloc. This will modify the page directory.
 */
static void
paging_heap_free(int vi, int pi)
{
    assert(bitmap_get(allocated_pages, pi));
    pde_4mb_t *entry = DIR_4MB(HEAP_PAGE_START + vi * MB(4));
    entry->present = 0;
    paging_page_free(pi);
    paging_flush_tlb();
}

/*
 * Initializes a new process heap.
 */
void
paging_heap_init(paging_heap_t *heap)
{
    heap->size = 0;
    heap->num_pages = 0;
}

/*
 * Grows or shrinks a heap, depending on the value
 * of delta. Returns -1 on error (e.g. shrinking by
 * more than available, or not enough physical memory).
 * On success, returns the previous brk.
 */
int
paging_heap_sbrk(paging_heap_t *heap, int delta)
{
    int orig_size = heap->size;
    int orig_num_pages = heap->num_pages;
    int orig_brk = HEAP_PAGE_START + orig_size;

    /* Upper bound limit (if delta is huge, rhs is negative -> true) */
    if (delta > 0 && orig_size > MAX_HEAP_SIZE - delta) {
        debugf("Trying to expand heap beyond virtual capacity\n");
        return -1;
    }

    /* Lower bound limit */
    if (delta < 0 && orig_size + delta < 0) {
        debugf("Trying to deallocate more than was allocated\n");
        return -1;
    }

    int new_size = orig_size + delta;
    int new_num_pages = (new_size + MB(4) - 1) / MB(4);

    /* Allocate new pages as necessary */
    while (heap->num_pages < new_num_pages) {
        int page = paging_heap_alloc(heap->num_pages);

        /* If we don't have enough pages, undo allocation */
        if (page < 0) {
            debugf("Physical memory exhausted\n");

            while (heap->num_pages > orig_num_pages) {
                int vi = --heap->num_pages;
                int pi = heap->pages[vi];
                paging_heap_free(vi, pi);
            }

            return -1;
        }

        heap->pages[heap->num_pages++] = page;
    }

    /* Free deallocated pages as necessary */
    while (heap->num_pages > new_num_pages) {
        int vi = --heap->num_pages;
        int pi = heap->pages[vi];
        paging_heap_free(vi, pi);
    }

    heap->size = new_size;
    return orig_brk;
}

/*
 * Deallocates a heap, freeing all pages used by it.
 */
void
paging_heap_destroy(paging_heap_t *heap)
{
    int vi;
    for (vi = 0; vi < heap->num_pages; ++vi) {
        int pi = heap->pages[vi];
        paging_heap_free(vi, pi);
    }
    heap->size = 0;
    heap->num_pages = 0;
    paging_flush_tlb();
}

/*
 * Updates the process and heap pages to point to the
 * physical pages corresponding to the specified process.
 * This should be called during context switching.
 */
void
paging_set_context(int pid, paging_heap_t *heap)
{
    assert(pid >= 0);

    /* Point the user page to the corresponding physical address */
    DIR_4MB(USER_PAGE_START)->base_addr = TO_4MB_BASE(MB(pid * 4 + 8));

    /* Replace heap page directory entries */
    int i;
    for (i = 0; i < MAX_HEAP_PAGES; ++i) {
        pde_4mb_t *entry = DIR_4MB(HEAP_PAGE_START + i * MB(4));
        if (i < heap->num_pages) {
            entry->present = 1;
            entry->base_addr = TO_4MB_BASE(HEAP_PAGE_START + heap->pages[i] * MB(4));
        } else {
            entry->present = 0;
        }
    }

    /* Flush the TLB */
    paging_flush_tlb();
}

/*
 * Updates the vidmap page to point to the specified address.
 * If present is false, the vidmap page is disabled. Returns
 * the virtual address of the vidmap page.
 */
void
paging_update_vidmap_page(uint8_t *video_mem, bool present)
{
    /* Update page table structures */
    pte_t *table = TABLE(VIDMAP_PAGE_START);
    table->present = present ? 1 : 0;
    table->base_addr = TO_4KB_BASE(video_mem);

    /* Also flush the TLB */
    paging_flush_tlb();
}

/*
 * Returns whether a single page access would be valid in userspace.
 * addr should point to some address to check on input. On output,
 * it will point to the next page that may need to be checked. This
 * only checks a single byte, since it does not support accesses
 * spanning multiple pages.
 */
static bool
is_page_user_accessible(uint32_t *addr, bool write)
{
    /* Access page info through the directory */
    pde_4kb_t *dir = DIR_4KB(*addr);
    if (!dir->present || !dir->user || (!dir->write && write)) {
        return false;
    }

    /* If it's a 4MB page, we're done. */
    if (dir->size == SIZE_4MB) {
        *addr = (*addr + MB(4)) & -MB(4);
        return true;
    }

    /*
     * It's a 4KB page so it must be in the first 4MB.
     * Access it through the single page table.
     */
    pte_t *table = TABLE(*addr);
    if (!table->present || !table->user || (!table->write && write)) {
        return false;
    }

    *addr = (*addr + KB(4)) & -KB(4);
    return true;
}

/*
 * Checks whether a memory access would be valid in userspace.
 * That is, this function will return true iff accessing
 * the same address in ring 3 would not cause a page fault.
 */
bool
is_user_accessible(const void *start, int nbytes, bool write)
{
    /* Negative accesses are obviously impossible */
    if (nbytes < 0) {
        return false;
    }

    /* Check for overflow */
    uint32_t addr = (uint32_t)start;
    uint32_t end = addr + (uint32_t)nbytes;
    if (end < addr) {
        return false;
    }

    /* Go through pages and ensure they're all accessible */
    while (addr < end) {
        if (!is_page_user_accessible(&addr, write)) {
            return false;
        }
    }

    return true;
}

/*
 * Copies a string from userspace, with page boundary checking.
 * Returns true if the buffer was big enough and the source string
 * could be fully copied to the buffer. Returns false otherwise.
 * This does NOT pad excess chars in dest with zeros!
 */
bool
strscpy_from_user(char *dest, const char *src, int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        if (!is_user_accessible(&src[i], 1, false)) {
            return false;
        }

        if ((dest[i] = src[i]) == '\0') {
            return true;
        }
    }

    /* Didn't reach the terminator before n characters */
    return false;
}

/*
 * Copies a buffer from userspace to kernelspace, checking
 * that the source buffer is a valid userspace buffer. Returns
 * true if the entire buffer could be copied, and false otherwise.
 */
bool
copy_from_user(void *dest, const void *src, int n)
{
    if (!is_user_accessible(src, n, false)) {
        return false;
    }

    memcpy(dest, src, n);
    return true;
}

/*
 * Copies a buffer from kernelspace to userspace, checking
 * that the destination buffer is a valid userspace buffer. Returns
 * true if the entire buffer could be copied, and false otherwise.
 */
bool
copy_to_user(void *dest, const void *src, int n)
{
    if (!is_user_accessible(dest, n, true)) {
        return false;
    }

    memcpy(dest, src, n);
    return true;
}
