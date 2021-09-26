#include "paging.h"
#include "types.h"
#include "debug.h"
#include "string.h"
#include "terminal.h"
#include "bitmap.h"
#include "filesys.h"

/* PDE size field values */
#define SIZE_4KB 0
#define SIZE_4MB 1

/* Amount of physical memory present in the system (256MB) */
#define MAX_RAM MB(256)
#define MAX_PAGES (MAX_RAM / PAGE_SIZE)

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
 * one bit representing one page in the system.
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
#define TO_4MB_BASE(x) (((uintptr_t)(x)) >> 22)
#define TO_4KB_BASE(x) (((uintptr_t)(x)) >> 12)
#define TO_DIR_INDEX(x) (((uintptr_t)(x)) >> 22)
#define TO_TABLE_INDEX(x) ((((uintptr_t)(x)) >> 12) & 0x3ff)
#define DIR_TO_PDE_4KB(dir, addr) (&(dir)[TO_DIR_INDEX(addr)].dir_4kb)
#define DIR_TO_PDE_4MB(dir, addr) (&(dir)[TO_DIR_INDEX(addr)].dir_4mb)
#define TABLE_TO_PTE(table, addr) (&(table)[TO_TABLE_INDEX(addr)])
#define PDE_TO_TABLE(pde) ((pte_t *)((pde)->base_addr << 12))
#define PDE_TO_PTE(pde, addr) (TABLE_TO_PTE(PDE_TO_TABLE(pde), (addr)))
#define PDE_4KB(addr) (DIR_TO_PDE_4KB(page_dir, (addr)))
#define PDE_4MB(addr) (DIR_TO_PDE_4MB(page_dir, (addr)))
#define PTE(addr) (PDE_TO_PTE(PDE_4KB(addr), (addr)))

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

/* Initializes the VGA text mode page */
static void
paging_init_vga_text(void)
{
    pte_t *pte = PTE(VGA_TEXT_PAGE_START);
    pte->present = 1;
    pte->write = 1;
    pte->user = 0;
    pte->base_addr = TO_4KB_BASE(VGA_TEXT_PAGE_START);
}

/* Initializes the VGA font access pages */
static void
paging_init_vga_font(void)
{
    uintptr_t addr;
    for (addr = VGA_FONT_PAGE_START; addr < VGA_FONT_PAGE_END; addr += KB(4)) {
        pte_t *pte = PTE(addr);
        pte->present = 1;
        pte->write = 1;
        pte->user = 0;
        pte->base_addr = TO_4KB_BASE(addr);
    }
}

/* Initializes the VBE framebuffer pages */
static void
paging_init_vga_vbe(void)
{
    uintptr_t addr;
    for (addr = VGA_VBE_PAGE_START; addr < VGA_VBE_PAGE_END; addr += MB(4)) {
        pde_4mb_t *pde = PDE_4MB(addr);
        pde->present = 0;
        pde->write = 1;
        pde->user = 1;
        pde->size = SIZE_4MB;
        pde->base_addr = TO_4MB_BASE(addr);
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
        : "eax", "memory", "cc");
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
        : "eax", "memory");
}

/* Initializes all initial page tables and enables paging. */
void
paging_init(void)
{
    /* Ensure page table arrays are 4096-byte aligned */
    assert(((uintptr_t)page_dir   & 0xfff) == 0);
    assert(((uintptr_t)page_table & 0xfff) == 0);

    /* Initialize static page table entries */
    paging_init_common();
    paging_init_kernel();
    paging_init_vga_text();
    paging_init_vga_font();
    paging_init_vga_vbe();
    paging_init_vidmap();

    /* Set control registers */
    paging_init_registers();
}

/*
 * Allocates a new page and returns its physical address.
 * If no free pages are available, returns 0. This does
 * not modify the page directory; it only prevents this
 * function from returning the same address in the future
 * until paging_page_free() is called.
 */
uintptr_t
paging_page_alloc(void)
{
    /* Find a free page... */
    int pfn = bitmap_find_zero(allocated_pages, MAX_PAGES);
    if (pfn >= MAX_PAGES) {
        return -1;
    }

    /* ... and mark it as allocated. */
    bitmap_set(allocated_pages, pfn);
    uintptr_t paddr = pfn * PAGE_SIZE;
    return paddr;
}

/*
 * Frees a page obtained from paging_page_alloc().
 */
void
paging_page_free(uintptr_t paddr)
{
    assert(paddr != 0);
    int pfn = paddr / PAGE_SIZE;
    assert(bitmap_get(allocated_pages, pfn));
    bitmap_clear(allocated_pages, pfn);
}

/*
 * Modifies the page tables to map one page (of size PAGE_SIZE)
 * from the specified virtual address to the specified physical
 * address. Flushes the TLB.
 */
void
paging_page_map(uintptr_t vaddr, uintptr_t paddr, bool user)
{
    assert(PAGE_SIZE == MB(4));

    pde_4mb_t *entry = PDE_4MB(vaddr);
    entry->present = 1;
    entry->write = 1;
    entry->user = user;
    entry->size = SIZE_4MB;
    entry->base_addr = TO_4MB_BASE(paddr);
    paging_flush_tlb();
}

/*
 * Modifies the page tables to unmap the specified page. Flushes
 * the TLB.
 */
void
paging_page_unmap(uintptr_t vaddr)
{
    assert(PAGE_SIZE == MB(4));

    pde_4mb_t *entry = PDE_4MB(vaddr);
    entry->present = 0;
    paging_flush_tlb();
}

/*
 * Copies the contents of the user page to the specified physical
 * address. This does not clobber any page mappings.
 */
void
paging_clone_user_page(uintptr_t dest_paddr)
{
    paging_page_map(TEMP_PAGE_START, dest_paddr, false);
    memcpy((void *)TEMP_PAGE_START, (const void *)USER_PAGE_START, MB(4));
    paging_page_unmap(TEMP_PAGE_START);
}

/*
 * Updates the user page to point to the specified physical
 * address.
 */
void
paging_map_user_page(uintptr_t paddr)
{
    paging_page_map(USER_PAGE_START, paddr, true);
}

/*
 * Updates the vidmap page to point to the specified address.
 * If present is false, the vidmap page is disabled.
 */
void
paging_update_vidmap_page(uintptr_t paddr, bool present)
{
    pte_t *pte = PTE(VIDMAP_PAGE_START);
    pte->present = present ? 1 : 0;
    pte->base_addr = TO_4KB_BASE(paddr);
    paging_flush_tlb();
}

/*
 * Enables or disables the VBE framebuffer pages.
 */
void
paging_update_vbe_page(bool present)
{
    uintptr_t addr;
    for (addr = VGA_VBE_PAGE_START; addr < VGA_VBE_PAGE_END; addr += MB(4)) {
        pde_4mb_t *pde = PDE_4MB(addr);
        pde->present = present ? 1 : 0;
    }
    paging_flush_tlb();
}

/*
 * Returns whether a single byte access would be valid.
 * addr should point to some address to check on input. On output,
 * it will point to the next page that may need to be checked.
 */
static bool
is_page_accessible(uintptr_t *addr, bool user, bool write)
{
    /* Access page info through the directory */
    pde_4kb_t *pde = PDE_4KB(*addr);
    if (!pde->present || (!pde->user && user) || (!pde->write && write)) {
        return false;
    }

    /* If it's a 4MB page, we're done. */
    if (pde->size == SIZE_4MB) {
        *addr = (*addr + MB(4)) & -MB(4);
        return true;
    }

    /* It's a 4KB page, access info through the table */
    pte_t *pte = PDE_TO_PTE(pde, *addr);
    if (!pte->present || (!pte->user && user) || (!pte->write && write)) {
        return false;
    }

    *addr = (*addr + KB(4)) & -KB(4);
    return true;
}

/*
 * Checks whether a memory access would be valid.
 * That is, this function will return true iff accessing
 * every byte in the given range would not cause any
 * page faults.
 */
bool
is_memory_accessible(const void *start, int nbytes, bool user, bool write)
{
    /* Negative accesses are obviously impossible */
    if (nbytes < 0) {
        return false;
    }

    /* Check for overflow */
    uintptr_t addr = (uintptr_t)start;
    uintptr_t end = addr + nbytes;
    if (end < addr) {
        return false;
    }

    /* Go through pages and ensure they're all accessible */
    while (addr < end) {
        if (!is_page_accessible(&addr, user, write)) {
            return false;
        }
    }

    return true;
}

/*
 * Copies a string from userspace, with page boundary checking.
 * Returns the length of the string if the buffer was big enough
 * and the source string could be fully copied to the buffer.
 * Returns -1 otherwise.
 */
int
strscpy_from_user(char *dest, const char *src, int n)
{
    int i = 0;
    uintptr_t limit = (uintptr_t)src;
    while (i < n && is_page_accessible(&limit, true, false)) {
        for (; i < n && (uintptr_t)&src[i] < limit; ++i) {
            if ((dest[i] = src[i]) == '\0') {
                return i;
            }
        }
    }

    /* Didn't reach the terminator before n characters */
    return -1;
}

/*
 * Copies a buffer from userspace to kernelspace, checking
 * that the source buffer is a valid userspace buffer. Returns
 * dest if the entire buffer could be copied, and null otherwise.
 */
void *
copy_from_user(void *dest, const void *src, int n)
{
    if (!is_memory_accessible(src, n, true, false)) {
        return NULL;
    }

    memcpy(dest, src, n);
    return dest;
}

/*
 * Copies a buffer from kernelspace to userspace, checking
 * that the destination buffer is a valid userspace buffer. Returns
 * dest if the entire buffer could be copied, and null otherwise.
 */
void *
copy_to_user(void *dest, const void *src, int n)
{
    if (!is_memory_accessible(dest, n, true, true)) {
        return NULL;
    }

    memcpy(dest, src, n);
    return dest;
}

/*
 * Fills a userspace buffer with the specified byte, checking
 * that the buffer is valid. Returns s if the entire buffer could
 * be filled, and null otherwise.
 */
void *
memset_user(void *s, unsigned char c, int n)
{
    if (!is_memory_accessible(s, n, true, true)) {
        return NULL;
    }

    memset(s, c, n);
    return s;
}
