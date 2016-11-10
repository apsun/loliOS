#include "paging.h"
#include "debug.h"

#define SIZE_4KB 0
#define SIZE_4MB 1

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)

#define TO_4MB_BASE(x) (((uint32_t)x) >> 22)
#define TO_4KB_BASE(x) (((uint32_t)x) >> 12)

#define TO_DIR_INDEX(x) (((uint32_t)x) >> 22)
#define TO_TABLE_INDEX(x) ((((uint32_t)x) >> 12) & 0x3ff)

#define ALIGN_4KB __attribute__((aligned(KB(4))))

/* Page directory */
static ALIGN_4KB page_dir_entry_t page_dir[NUM_PDE];

/* Page table for first 4MB of memory */
static ALIGN_4KB page_table_entry_4kb_t page_table[NUM_PTE];

/* Page table for vidmap area */
static ALIGN_4KB page_table_entry_4kb_t page_table_vidmap[NUM_PTE];

/* Helpful macros to access page table stuff */
#define DIR_4KB(addr) page_dir[TO_DIR_INDEX(addr)].dir_4kb
#define DIR_4MB(addr) page_dir[TO_DIR_INDEX(addr)].dir_4mb
#define TABLE(addr) page_table[TO_TABLE_INDEX(addr)]
#define TABLE_VIDMAP(addr) page_table_vidmap[TO_TABLE_INDEX(addr)]

/* Initializes the 4MB kernel page */
static void
paging_init_kernel(void)
{
    DIR_4MB(KERNEL_PAGE_START).present = 1;
    DIR_4MB(KERNEL_PAGE_START).write = 1;
    DIR_4MB(KERNEL_PAGE_START).user = 0;
    DIR_4MB(KERNEL_PAGE_START).size = SIZE_4MB;
    DIR_4MB(KERNEL_PAGE_START).global = 1;
    DIR_4MB(KERNEL_PAGE_START).base_addr = TO_4MB_BASE(KERNEL_PAGE_START);
}

/* Initializes the 4KB VGA page */
static void
paging_init_video(void)
{
    DIR_4KB(VIDEO_PAGE_START).present = 1;
    DIR_4KB(VIDEO_PAGE_START).write = 1;
    DIR_4KB(VIDEO_PAGE_START).user = 0;
    DIR_4KB(VIDEO_PAGE_START).cache_disabled = 1;
    DIR_4KB(VIDEO_PAGE_START).size = SIZE_4KB;
    DIR_4KB(VIDEO_PAGE_START).global = 0;
    DIR_4KB(VIDEO_PAGE_START).base_addr = TO_4KB_BASE(page_table);

    TABLE(VIDEO_PAGE_START).present = 1;
    TABLE(VIDEO_PAGE_START).write = 1;
    TABLE(VIDEO_PAGE_START).user = 0;
    TABLE(VIDEO_PAGE_START).cache_disabled = 1;
    TABLE(VIDEO_PAGE_START).base_addr = TO_4KB_BASE(VIDEO_PAGE_START);
}

/* Initializes the 4MB user page */
static void
paging_init_user(void)
{
    DIR_4MB(USER_PAGE_START).present = 1;
    DIR_4MB(USER_PAGE_START).write = 1;
    DIR_4MB(USER_PAGE_START).user = 1;
    DIR_4MB(USER_PAGE_START).size = SIZE_4MB;
    DIR_4MB(USER_PAGE_START).global = 0;
}

/* Initializes the 4KB vidmap page */
static void
paging_init_vidmap(void)
{
    DIR_4KB(VIDMAP_PAGE_START).present = 1;
    DIR_4KB(VIDMAP_PAGE_START).write = 1;
    DIR_4KB(VIDMAP_PAGE_START).user = 1;
    DIR_4KB(VIDMAP_PAGE_START).cache_disabled = 1;
    DIR_4KB(VIDMAP_PAGE_START).size = SIZE_4KB;
    DIR_4KB(VIDMAP_PAGE_START).global = 0;
    DIR_4KB(VIDMAP_PAGE_START).base_addr = TO_4KB_BASE(page_table_vidmap);

    TABLE_VIDMAP(VIDMAP_PAGE_START).present = 0;
    TABLE_VIDMAP(VIDMAP_PAGE_START).write = 1;
    TABLE_VIDMAP(VIDMAP_PAGE_START).user = 1;
    TABLE_VIDMAP(VIDMAP_PAGE_START).cache_disabled = 1;
    TABLE_VIDMAP(VIDMAP_PAGE_START).base_addr = TO_4KB_BASE(VIDEO_PAGE_START);
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
        /* Enable caching of page directory */
        /* Enable write-back caching */
        "movl %%cr3, %%eax;"
        "andl $0x00000fe7, %%eax;"
        "orl $page_dir, %%eax;"
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
        :
        : "eax", "cc");
}

/* Flushes the TLB */
static void
paging_flush_tlb(void)
{
    asm volatile("movl %%cr3, %%eax;"
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
    paging_init_kernel();
    paging_init_video();
    paging_init_user();
    paging_init_vidmap();

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
    DIR_4MB(USER_PAGE_START).base_addr = TO_4MB_BASE(phys_addr);

    /* Flush the TLB */
    paging_flush_tlb();
}

/*
 * Updates the vidmap page. This should be called during
 * context switches and when the vidmap syscall is invoked.
 * Returns the virtual address of the vidmap page.
 */
uint8_t *
paging_update_vidmap_page(bool present)
{
    /* Set page to present if vidmap was called */
    TABLE_VIDMAP(VIDMAP_PAGE_START).present = present ? 1 : 0;

    /* Also flush the TLB */
    paging_flush_tlb();

    return (uint8_t *)(VIDMAP_PAGE_START);
}
