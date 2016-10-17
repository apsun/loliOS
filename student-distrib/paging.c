#include "paging.h"
#include "debug.h"

#define VIDEO_ADDR 0xB8000

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)

#define TO_4MB_BASE(x) (((uint32_t)x) >> 22)
#define TO_4KB_BASE(x) (((uint32_t)x) >> 12)

/* Initializes the page directory entries */
static void
paging_init_dir(void)
{
    int i;

    /* Initialize directory for first 4MB of memory */
    page_dir[0].dir_4kb.present = 1;
    page_dir[0].dir_4kb.write = 1;
    page_dir[0].dir_4kb.user = 0;
    page_dir[0].dir_4kb.write_through = 0;
    page_dir[0].dir_4kb.cache_disabled = 0;
    page_dir[0].dir_4kb.accessed = 0;
    page_dir[0].dir_4kb.reserved = 0;
    page_dir[0].dir_4kb.size = 0;
    page_dir[0].dir_4kb.global = 0;
    page_dir[0].dir_4kb.avail = 0;
    page_dir[0].dir_4kb.base_addr = TO_4KB_BASE(page_table);

    /* Initialize directory for second 4MB of memory */
    page_dir[1].dir_4mb.present = 1;
    page_dir[1].dir_4mb.write = 1;
    page_dir[1].dir_4mb.user = 0;
    page_dir[1].dir_4mb.write_through = 0;
    page_dir[1].dir_4mb.cache_disabled = 0;
    page_dir[1].dir_4mb.accessed = 0;
    page_dir[1].dir_4mb.dirty = 0;
    page_dir[1].dir_4mb.size = 1;
    page_dir[1].dir_4mb.global = 1;
    page_dir[1].dir_4mb.avail = 0;
    page_dir[1].dir_4mb.page_attr_idx = 0;
    page_dir[1].dir_4mb.reserved = 0;
    page_dir[1].dir_4mb.base_addr = TO_4MB_BASE(MB(1 * 4));

    /* Initialize remaining page directory entries to non-present
     * (actually we can skip this since they're already 0-initialized) */
    for (i = 2; i < NUM_PDE; ++i) {
        page_dir[i].dir_4kb.present = 0;
        page_dir[i].dir_4kb.write = 0;
        page_dir[i].dir_4kb.user = 0;
        page_dir[i].dir_4kb.write_through = 0;
        page_dir[i].dir_4kb.cache_disabled = 0;
        page_dir[i].dir_4kb.accessed = 0;
        page_dir[i].dir_4kb.reserved = 0;
        page_dir[i].dir_4kb.size = 0;
        page_dir[i].dir_4kb.global = 0;
        page_dir[i].dir_4kb.avail = 0;
        page_dir[i].dir_4kb.base_addr = 0;
    }
}

/* Initializes the page table entries */
static void
paging_init_table(void)
{
    int i;

    /* Initialize all 4KB pages in first 4MB of memory */
    for (i = 0; i < NUM_PTE; ++i) {
        page_table[i].present = 0;
        page_table[i].write = 1;
        page_table[i].user = 1;
        page_table[i].write_through = 0;
        page_table[i].cache_disabled = 0;
        page_table[i].accessed = 0;
        page_table[i].dirty = 0;
        page_table[i].page_attr_idx = 0;
        page_table[i].global = 0;
        page_table[i].avail = 0;
        page_table[i].base_addr = TO_4KB_BASE(KB(i * 4));
    }
}

/*
 * Initializes the video memory page table entry.
 * This must be called after paging_init_table().
 */
static void
paging_init_video_page(void)
{
    /* Set video memory page to present, and
     * disable caching since it's memory-mapped I/O */
    page_table[TO_4KB_BASE(VIDEO_ADDR)].present = 1;
    page_table[TO_4KB_BASE(VIDEO_ADDR)].cache_disabled = 1;
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

/* Enables memory paging. */
void
paging_enable(void)
{
    /* Ensure page dir and table are 4096-byte aligned */
    ASSERT(((uint32_t)page_table & 0xfff) == 0);
    ASSERT(((uint32_t)page_dir   & 0xfff) == 0);

    /* Initialize page directory entries */
    paging_init_dir();

    /* Initialize page table entries */
    paging_init_table();

    /* Initialize video page */
    paging_init_video_page();

    /* Set control registers */
    paging_init_registers();
}
