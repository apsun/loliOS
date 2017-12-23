#include "paging.h"
#include "debug.h"
#include "terminal.h"

#define TO_4MB_BASE(x) (((uint32_t)(x)) >> 22)
#define TO_4KB_BASE(x) (((uint32_t)(x)) >> 12)

#define TO_DIR_INDEX(x) (((uint32_t)(x)) >> 22)
#define TO_TABLE_INDEX(x) ((((uint32_t)(x)) >> 12) & 0x3ff)

/* Page directory */
__aligned(KB(4))
static pde_t page_dir[1024];

/* Page table for first 4MB of memory */
__aligned(KB(4))
static pte_t page_table[1024];

/*
 * We will use a very simple heap allocation scheme:
 * each process allocates heap space in increments of
 * 4MB. Ideally this would be 4KB, but the overhead
 * of maintaining 1024 page tables instead of just
 * 1 page directory would be too high. Additionally, the
 * maximum number of processes is capped, so sharing
 * a limited number of pages is less of an issue.
 *
 * Finding free pages is just done using a linear scan
 * over this map. If desired, this can be packed into
 * a bit vector (since there are only 31 heap pages).
 */
static bool heap_map[MAX_HEAP_PAGES];

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
    int32_t i;
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

/* Initializes the 64KB ISA DMA zone pages */
static void
paging_init_isa_dma(void)
{
    uint32_t addr = ISA_DMA_PAGE_START;
    while (addr < ISA_DMA_PAGE_END) {
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
    int32_t i;
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
    ASSERT(((uint32_t)page_dir          & 0xfff) == 0);
    ASSERT(((uint32_t)page_table        & 0xfff) == 0);

    /* Initialize page table entries */
    paging_init_common();
    paging_init_kernel();
    paging_init_video();
    paging_init_user();
    paging_init_vidmap();
    paging_init_isa_dma();
    paging_init_heap();

    /* Set control registers */
    paging_init_registers();
}

/**
 * Allocates a new 4MB heap page on behalf of the
 * calling process. This will modify the page directory,
 * but will NOT flush the TLB.
 */
static int32_t
paging_heap_alloc(int32_t vi)
{
    int32_t pi;
    for (pi = 0; pi < MAX_HEAP_PAGES; ++pi) {
        if (!heap_map[pi]) {
            pde_4mb_t *entry = DIR_4MB(HEAP_PAGE_START + vi * MB(4));
            ASSERT(!entry->present);
            entry->present = 1;
            entry->base_addr = TO_4MB_BASE(HEAP_PAGE_START + pi * MB(4));
            heap_map[pi] = true;
            return pi;
        }
    }
    return -1;
}

/**
 * Frees a 4MB heap page previously allocated using
 * paging_heap_alloc. This will modify the page directory,
 * but will NOT flush the TLB.
 */
static void
paging_heap_free(int32_t vi, int32_t pi)
{
    ASSERT(heap_map[pi]);
    pde_4mb_t *entry = DIR_4MB(HEAP_PAGE_START + vi * MB(4));
    entry->present = 0;
    heap_map[pi] = false;
}

/**
 * Initializes a new process heap.
 */
void
paging_heap_init(paging_heap_t *heap)
{
    heap->size = 0;
    heap->num_pages = 0;
}

/**
 * Grows or shrinks a heap, depending on the value
 * of delta. Returns -1 on error (e.g. shrinking by
 * more than available, or not enough physical memory).
 * On success, returns the previous brk.
 */
int32_t
paging_heap_sbrk(paging_heap_t *heap, int32_t delta)
{
    int32_t orig_size = heap->size;
    int32_t orig_num_pages = heap->num_pages;
    int32_t orig_brk = HEAP_PAGE_START + orig_size;

    /* TODO: Overflow and bound checks needed here */
    int32_t new_size = orig_size + delta;
    int32_t new_num_pages = (new_size + MB(4) - 1) / MB(4);

    /* Allocate new pages as necessary */
    while (heap->num_pages < new_num_pages) {
        int32_t page = paging_heap_alloc(heap->num_pages);

        /* If we don't have enough pages, undo allocation */
        if (page < 0) {
            while (heap->num_pages > orig_num_pages) {
                int32_t vi = --heap->num_pages;
                int32_t pi = heap->pages[vi];
                paging_heap_free(vi, pi);
            }

            paging_flush_tlb();
            return -1;
        }

        heap->pages[heap->num_pages++] = page;
    }

    /* Free deallocated pages as necessary */
    while (heap->num_pages > new_num_pages) {
        int32_t vi = --heap->num_pages;
        int32_t pi = heap->pages[vi];
        paging_heap_free(vi, pi);
    }

    heap->size = new_size;
    paging_flush_tlb();
    return orig_brk;
}

/**
 * Deallocates a heap, freeing all pages used by it.
 */
void
paging_heap_destroy(paging_heap_t *heap)
{
    int32_t vi;
    for (vi = 0; vi < heap->num_pages; ++vi) {
        int32_t pi = heap->pages[vi];
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
paging_set_context(int32_t pid, paging_heap_t *heap)
{
    ASSERT(pid >= 0);

    /* Each process page is mapped starting from 8MB, each 4MB */
    uint32_t phys_addr = MB(pid * 4 + 8);

    /* Point the user page to the corresponding physical address */
    DIR_4MB(USER_PAGE_START)->base_addr = TO_4MB_BASE(phys_addr);

    /* Replace heap page directory entries */
    int32_t i;
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
