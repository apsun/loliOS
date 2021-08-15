#ifndef _PAGING_H
#define _PAGING_H

#include "types.h"

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)

#define PAGE_SIZE MB(4)

#define VGA_FONT_PAGE_START 0x000A0000
#define VGA_FONT_PAGE_END   0x000B0000

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

#define KERNEL_HEAP_START   0x00800000
#define KERNEL_HEAP_END     0x08000000

#define USER_PAGE_START     0x08000000
#define USER_PAGE_END       0x08400000

#define USER_HEAP_START     0x08400000
#define USER_HEAP_END       0x10000000

#define TEMP_PAGE_START     0x10000000
#define TEMP_PAGE_END       0x10400000

#define TEMP_PAGE_2_START   0x10400000
#define TEMP_PAGE_2_END     0x10800000

#define VBE_PAGE_START      0xE0000000
#define VBE_PAGE_END        (VBE_PAGE_START + MB(8))

#ifndef ASM

/* Initializes paging */
void paging_init(void);

/* Allocates a page without mapping it */
uintptr_t paging_page_alloc(void);

/* Deallocates a page without unmapping it */
void paging_page_free(uintptr_t paddr);

/* Maps a page into memory */
void paging_page_map(uintptr_t vaddr, uintptr_t paddr, bool user);

/* Unmaps a page from memory */
void paging_page_unmap(uintptr_t vaddr);

/* Loads a program into the user page */
uint32_t paging_load_user_page(int inode_idx, uintptr_t paddr);

/* Clones an existing user page */
void paging_clone_user_page(uintptr_t dest_paddr);

/* Updates the user page */
void paging_map_user_page(uintptr_t paddr);

/* Updates the vidmap page to point to the specified address */
void paging_update_vidmap_page(uint8_t *video_mem, bool present);

/* Checks if a range of memory is accessible */
bool is_memory_accessible(const void *start, int nbytes, bool user, bool write);

/* User-kernel copy functions */
int strscpy_from_user(char *dest, const char *src, int n);
void *copy_from_user(void *dest, const void *src, int n);
void *copy_to_user(void *dest, const void *src, int n);
void *memset_user(void *s, unsigned char c, int n);

#endif /* ASM */

#endif /* _PAGING_H */
