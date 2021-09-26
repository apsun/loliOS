#ifndef _VGA_H
#define _VGA_H

#include "types.h"

#ifndef ASM

/* Syscall handlers */
__cdecl int vga_fbmap(void **ptr, int xres, int yres, int bpp);
__cdecl int vga_fbunmap(void *ptr);
__cdecl int vga_fbflip(void *ptr);

/* Sets the VGA cursor location */
void vga_set_cursor_location(uint16_t location);

/* Initializes the VGA driver */
void vga_init(void);

#endif /* ASM */

#endif /* _VGA_H */
