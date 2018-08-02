#include "paging.h"
#include "lib.h"
#include "debug.h"
#include "terminal.h"
#include "bitmap.h"
#include "filesys.h"

/* PDE size field values */
#define SIZE_4KB 0
#define SIZE_4MB 1

/* Amount of physical memory present in the system (256MB) */
#define MAX_RAM MB(256)
#define MAX_PAGES (MAX_RAM / MB(4))

/* Where in the user page to begin copying the userspace program */
#define PROCESS_OFFSET 0x48000

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
 * one bit representing one 4MB page in the system.
 */
static bitmap_declare(allocated_pages, MAX_PAGES);

/*
 * Helpful macros to access page table stuff. Conventions:
 *
 * DIR = base address of page directory
 * TABLE = base address of page table
 * PDE = pointer to single page directory entry
 * PTE = pointer to single page table entry
 */
#define TO_4MB_BASE(x) (((uint32_t)(x)) >> 22)
#define TO_4KB_BASE(x) (((uint32_t)(x)) >> 12)
#define TO_DIR_INDEX(x) (((uint32_t)(x)) >> 22)
#define TO_TABLE_INDEX(x) ((((uint32_t)(x)) >> 12) & 0x3ff)
#define DIR_TO_PDE_4KB(dir, addr) (&(dir)[TO_DIR_INDEX(addr)].dir_4kb)
#define DIR_TO_PDE_4MB(dir, addr) (&(dir)[TO_DIR_INDEX(addr)].dir_4mb)
#define TABLE_TO_PTE(table, addr) (&(table)[TO_TABLE_INDEX(addr)])
#define PDE_TO_TABLE(pde) ((pte_t *)((pde)->base_addr << 12))
#define PDE_TO_PTE(pde, addr) (TABLE_TO_PTE(PDE_TO_TABLE(pde), addr))
#define PDE_4KB(addr) (DIR_TO_PDE_4KB(page_dir, addr))
#define PDE_4MB(addr) (DIR_TO_PDE_4MB(page_dir, addr))
#define PTE(addr) (PDE_TO_PTE(PDE_4KB(addr), addr))

/* Initializes the page directory for the first 4MB of memory */
static void
paging_init_common(void)
{
    pde_4kb_t *pde = PDE_4KB(0);
    pde->present = 1;
    pde->write = 1;
    pde->user = 1; /* Needed for vidmap page */
    pde->size = SIZE_4KB;
    pde->base_addr = TO_4KB_BASE(page_table);
    bitmap_set(allocated_pages, 0);
}

/* Initializes the page directory for the 4MB kernel page */
static void
paging_init_kernel(void)
{
    pde_4mb_t *pde = PDE_4MB(KERNEL_PAGE_START);
    pde->present = 1;
    pde->write = 1;
    pde->user = 0;
    pde->size = SIZE_4MB;
    pde->base_addr = TO_4MB_BASE(KERNEL_PAGE_START);
    bitmap_set(allocated_pages, 1);
}

/* Initializes the 4KB video memory pages */
static void
paging_init_video(void)
{
    /* Global (VGA) video memory page */
    pte_t *vga_pte = PTE(VIDEO_PAGE_START);
    vga_pte->present = 1;
    vga_pte->write = 1;
    vga_pte->user = 0;
    vga_pte->base_addr = TO_4KB_BASE(VIDEO_PAGE_START);

    /* Virtual video memory pages, one per terminal */
    int i;
    for (i = 0; i < NUM_TERMINALS; ++i) {
        uint32_t term_addr = TERMINAL_PAGE_START + i * KB(4);
        pte_t *tty_pte = PTE(term_addr);
        tty_pte->present = 1;
        tty_pte->write = 1;
        tty_pte->user = 0;
        tty_pte->base_addr = TO_4KB_BASE(term_addr);
    }
}

/* Initializes the 4KB vidmap page */
static void
paging_init_vidmap(void)
{
    pte_t *pte = PTE(VIDMAP_PAGE_START);
    pte->present = 0;
    pte->write = 1;
    pte->user = 1;
}

/* Initializes the SB16 DMA pages */
static void
paging_init_sb16(void)
{
    uint32_t addr = SB16_PAGE_START;
    while (addr < SB16_PAGE_END) {
        pte_t *pte = PTE(addr);
        pte->present = 1;
        pte->write = 1;
        pte->user = 0;
        pte->base_addr = TO_4KB_BASE(addr);
        addr += KB(4);
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

    /* Initialize static page table entries */
    paging_init_common();
    paging_init_kernel();
    paging_init_video();
    paging_init_vidmap();
    paging_init_sb16();

    /* Set control registers */
    paging_init_registers();
}

/*
 * Allocates a new page and returns its page frame number.
 * If no free pages are available, returns -1. This does
 * not modify the page directory; it only prevents this
 * function from returning the same address in the future
 * until paging_page_free() is called.
 */
int
paging_page_alloc(void)
{
    /* Find a free page... */
    int pfn = bitmap_find_zero(allocated_pages, MAX_PAGES);
    if (pfn >= MAX_PAGES) {
        return -1;
    }

    /* ... and mark it as allocated. */
    bitmap_set(allocated_pages, pfn);
    return pfn;
}

/*
 * Frees a page obtained from paging_page_alloc().
 */
void
paging_page_free(int pfn)
{
    assert(bitmap_get(allocated_pages, pfn));
    bitmap_clear(allocated_pages, pfn);
}

/*
 * Modifies the page tables to map the specified virtual
 * address to the specified physical address. This should
 * only be used for pages obtained from paging_page_alloc().
 */
static void
paging_page_map(int vfn, int pfn, bool user)
{
    uint32_t vaddr = vfn * MB(4);
    pde_4mb_t *entry = PDE_4MB(vaddr);
    entry->present = 1;
    entry->write = 1;
    entry->user = user;
    entry->size = SIZE_4MB;
    entry->base_addr = pfn;
    paging_flush_tlb();
}

/*
 * Modifies the page tables to unmap the specified virtual
 * address.
 */
static void
paging_page_unmap(int vfn)
{
    uint32_t vaddr = vfn * MB(4);
    pde_4mb_t *entry = PDE_4MB(vaddr);
    entry->present = 0;
    paging_flush_tlb();
}

/*
 * Allocates and maps a new page. This will modify the page
 * directory. Returns the PFN of the allocated page, or -1
 * if there are no free pages remaining.
 */
int
get_free_page(int vfn, bool user)
{
    int pfn = paging_page_alloc();
    if (pfn < 0) {
        return -1;
    } else {
        paging_page_map(vfn, pfn, user);
        return pfn;
    }
}

/*
 * Frees a page previously allocated using get_free_page().
 * This will modify the page directory.
 */
void
free_page(int vfn, int pfn)
{
    paging_page_unmap(vfn);
    paging_page_free(pfn);
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
 * Shrinks the specified heap to the specified number of pages.
 * This will flush the TLB.
 */
static void
paging_heap_shrink(paging_heap_t *heap, int new_pages)
{
    while (heap->num_pages > new_pages) {
        int i = --heap->num_pages;
        int vfn = HEAP_PAGE_START / MB(4) + i;
        int pfn = heap->pages[i];
        free_page(vfn, pfn);
    }
}

/*
 * Grows the specified heap to the specified number of pages.
 * This will flush the TLB. If there are not enough free
 * pages to satisfy the allocation, the heap will not be
 * modified and -1 will be returned.
 */
static int
paging_heap_grow(paging_heap_t *heap, int new_pages)
{
    int orig_pages = heap->num_pages;
    while (heap->num_pages < new_pages) {
        int vfn = HEAP_PAGE_START / MB(4) + heap->num_pages;
        int pfn = get_free_page(vfn, true);

        /* If allocation fails, undo allocations */
        if (pfn < 0) {
            debugf("Physical memory exhausted\n");
            paging_heap_shrink(heap, orig_pages);
            return -1;
        }

        heap->pages[heap->num_pages++] = pfn;
    }

    return 0;
}

/*
 * Grows or shrinks a heap, depending on the value
 * of delta. Returns -1 on error (e.g. shrinking by
 * more than available, or not enough physical memory).
 * On success, returns the previous brk's virtual address.
 */
int
paging_heap_sbrk(paging_heap_t *heap, int delta)
{
    int orig_size = heap->size;
    int orig_num_pages = heap->num_pages;
    int orig_brk = HEAP_PAGE_START + orig_size;
    void *orig_page_brk = (void *)(HEAP_PAGE_START + orig_num_pages * MB(4));

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

    /* Grow or shrink heap as necessary */
    if (new_num_pages > orig_num_pages) {
        if (paging_heap_grow(heap, new_num_pages) < 0) {
            return -1;
        }
        memset(orig_page_brk, 0, (new_num_pages - orig_num_pages) * MB(4));
    } else if (new_num_pages < orig_num_pages) {
        paging_heap_shrink(heap, new_num_pages);
    }

    /* Update heap size */
    heap->size = new_size;
    return orig_brk;
}

/*
 * Deallocates a heap, freeing all pages used by it.
 * This restores the heap to its initial (empty) state.
 */
void
paging_heap_destroy(paging_heap_t *heap)
{
    paging_heap_shrink(heap, 0);
    heap->size = 0;
}

/*
 * Clones an existing process's heap. Note that this
 * currently does not perform copy-on-write optimization.
 */
int
paging_heap_clone(paging_heap_t *dest, paging_heap_t *src)
{
    /* Allocate same number of pages as src */
    dest->size = src->size;
    dest->num_pages = 0;
    if (paging_heap_grow(dest, src->num_pages) < 0) {
        return -1;
    }

    /*
     * Copy page contents, 4MB at a time. Temporary page
     * is necessary since we need to be able to view both
     * physical addresses simultaneously, but both have
     * the same virtual address.
     */
    int i;
    for (i = 0; i < dest->num_pages; ++i) {
        paging_page_map(TEMP_PAGE_START / MB(4), dest->pages[i], false);
        paging_page_map(TEMP_PAGE_2_START / MB(4), src->pages[i], false);
        memcpy((void *)TEMP_PAGE_START, (void *)TEMP_PAGE_2_START, MB(4));
    }
    return 0;
}

/*
 * Clones an already-present page to a new, unmapped physical
 * memory address.
 */
void
paging_page_clone(int dest_pfn, void *src_vaddr)
{
    paging_page_map(TEMP_PAGE_START / MB(4), dest_pfn, false);
    memcpy((void *)TEMP_PAGE_START, src_vaddr, MB(4));
}

/*
 * Copies a program into memory, and returns the virtual
 * address of the entry point. This does not clobber any
 * page mappings.
 */
uint32_t
paging_load_exe(uint32_t inode_idx, int pfn)
{
    paging_page_map(TEMP_PAGE_START / MB(4), pfn, false);

    /* Clear process page first */
    memset((void *)TEMP_PAGE_START, 0, MB(4));

    /* Copy program into memory */
    int count;
    int offset = 0;
    do {
        uint8_t *vaddr = (uint8_t *)TEMP_PAGE_START + PROCESS_OFFSET + offset;
        count = read_data(inode_idx, offset, vaddr, MB(4));
        offset += count;
    } while (count > 0);

    /*
     * The entry point is located at bytes 24-27 of the executable.
     * If the "executable" is less than 28 bytes long, this will just
     * read garbage, which will cause the program to fault in userspace.
     * No need to handle it here.
     */
    return *(uint32_t *)(TEMP_PAGE_START + PROCESS_OFFSET + 24);
}

/*
 * Updates the process and heap pages to point to the
 * physical pages corresponding to the specified process.
 * This should be called during context switching.
 */
void
paging_set_context(int pfn, paging_heap_t *heap)
{
    /* Point the user page to the corresponding physical address */
    if (pfn >= 0) {
        paging_page_map(USER_PAGE_START / MB(4), pfn, true);
    } else {
        paging_page_unmap(USER_PAGE_START / MB(4));
    }

    /* Replace heap page directory entries */
    int i;
    for (i = 0; i < MAX_HEAP_PAGES; ++i) {
        int vfn = HEAP_PAGE_START / MB(4) + i;
        if (i < heap->num_pages) {
            paging_page_map(vfn, heap->pages[i], true);
        } else {
            paging_page_unmap(vfn);
        }
    }
}

/*
 * Updates the vidmap page to point to the specified address.
 * If present is false, the vidmap page is disabled.
 */
void
paging_update_vidmap_page(uint8_t *video_mem, bool present)
{
    /* Update page table structures */
    pte_t *table = PTE(VIDMAP_PAGE_START);
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
    pde_4kb_t *pde = PDE_4KB(*addr);
    if (!pde->present || !pde->user || (!pde->write && write)) {
        return false;
    }

    /* If it's a 4MB page, we're done. */
    if (pde->size == SIZE_4MB) {
        *addr = (*addr + MB(4)) & -MB(4);
        return true;
    }

    /* It's a 4KB page, access info through the table */
    pte_t *pte = PDE_TO_PTE(pde, addr);
    if (!pte->present || !pte->user || (!pte->write && write)) {
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
    int i = 0;
    uint32_t limit = (uint32_t)src;
    while (i < n && is_page_user_accessible(&limit, false)) {
        for (; i < n && (uint32_t)&src[i] < limit; ++i) {
            if ((dest[i] = src[i]) == '\0') {
                return true;
            }
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
