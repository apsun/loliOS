#ifndef _VBE_H
#define _VBE_H

#include "types.h"

#ifndef ASM

/* Disables VBE and returns to text mode */
void vbe_reset(void);

/* Increments the framebuffer refcount */
bool vbe_retain(bool fbmap);

/* Decrements the framebuffer refcount, disables VBE if it reaches zero */
void vbe_release(bool fbmap);

/* VBE syscall handlers */
__cdecl int vbe_fbmap(void **ptr, int xres, int yres, int bpp);
__cdecl int vbe_fbunmap(void *ptr);
__cdecl int vbe_fbflip(void *ptr);

/* Updates the executing process's fbmap page */
void vbe_update_fbmap_page(bool fbmap);

/* Initializes the VBE driver */
void vbe_init(void);

#endif /* ASM */

#endif /* _VBE_H */
