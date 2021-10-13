#ifndef _PAGING_H
#define _PAGING_H

#include "types.h"

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)

#define PAGE_SIZE MB(4)

#define VGA_FONT_PAGE_START 0x000A0000U
#define VGA_FONT_PAGE_END   0x000B0000U

#define VGA_TEXT_PAGE_START 0x000B8000U
#define VGA_TEXT_PAGE_END   0x000B9000U

#define VIDMAP_PAGE_START   0x000B9000U
#define VIDMAP_PAGE_END     0x000BA000U

#define KERNEL_PAGE_START   0x00400000U
#define KERNEL_PAGE_END     0x00800000U

#define KERNEL_HEAP_START   0x00800000U
#define KERNEL_HEAP_END     0x07C00000U

#define TEMP_PAGE_START     0x07C00000U
#define TEMP_PAGE_END       0x08000000U

#define USER_PAGE_START     0x08000000U
#define USER_PAGE_END       0x08400000U

#define USER_HEAP_START     0x08400000U
#define USER_HEAP_END       0x10000000U

#define VGA_VBE_PAGE_START  0xE0000000U
#define VGA_VBE_PAGE_END    0xE0800000U

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
void paging_update_vidmap_page(uintptr_t paddr, bool present);

/* Enables or disables the VBE framebuffer pages */
void paging_update_vbe_page(bool present);

/* Checks if a range of memory is accessible */
bool is_memory_accessible(const void *start, int nbytes, bool user, bool write);

/* User-kernel copy functions */
int strscpy_from_user(char *dest, const char *src, int n);
void *copy_from_user(void *dest, const void *src, int n);
void *copy_to_user(void *dest, const void *src, int n);
void *memset_user(void *s, unsigned char c, int n);

#endif /* ASM */

#endif /* _PAGING_H */
