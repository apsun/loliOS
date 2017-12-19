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

/* Page table for vidmap area */
__aligned(KB(4))
static pte_t page_table_vidmap[1024];

/* Helpful macros to access page table stuff */
#define DIR_4KB(addr) (&page_dir[TO_DIR_INDEX(addr)].dir_4kb)
#define DIR_4MB(addr) (&page_dir[TO_DIR_INDEX(addr)].dir_4mb)
#define TABLE(addr) (&page_table[TO_TABLE_INDEX(addr)])
#define TABLE_VIDMAP(addr) (&page_table_vidmap[TO_TABLE_INDEX(addr)])

/* Initializes the page directory for the first 4MB of memory */
static void
paging_init_dir(void)
{
    pde_4kb_t *dir = DIR_4KB(0);
    dir->present = 1;
    dir->write = 1;
    dir->user = 0;
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
    pde_4kb_t *dir = DIR_4KB(VIDMAP_PAGE_START);
    dir->present = 1;
    dir->write = 1;
    dir->user = 1;
    dir->size = SIZE_4KB;
    dir->base_addr = TO_4KB_BASE(page_table_vidmap);

    pte_t *table = TABLE_VIDMAP(VIDMAP_PAGE_START);
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
    ASSERT(((uint32_t)page_table_vidmap & 0xfff) == 0);

    /* Initialize page table entries */
    paging_init_dir();
    paging_init_kernel();
    paging_init_video();
    paging_init_user();
    paging_init_vidmap();
    paging_init_isa_dma();

    /* Set control registers */
    paging_init_registers();
}

/*
 * Updates the process page to point to the block of
 * physical memory corresponding to the specified process.
 * This should be called during context switches.
 */
void
paging_update_process_page(int32_t pid)
{
    ASSERT(pid >= 0);

    /* Each process page is mapped starting from 8MB, each 4MB */
    uint32_t phys_addr = MB(pid * 4 + 8);

    /* Point the user page to the corresponding physical address */
    DIR_4MB(USER_PAGE_START)->base_addr = TO_4MB_BASE(phys_addr);

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
    pte_t *table = TABLE_VIDMAP(VIDMAP_PAGE_START);
    table->present = present ? 1 : 0;
    table->base_addr = TO_4KB_BASE(video_mem);

    /* Also flush the TLB */
    paging_flush_tlb();
}
