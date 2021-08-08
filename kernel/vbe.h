#ifndef _VBE_H
#define _VBE_H

#include "types.h"

#ifndef ASM

/* Syscalls */
__cdecl int vbe_vbemap(uint8_t **ptr, int xres, int yres, int bpp);
__cdecl int vbe_vbeunmap(uint8_t *ptr);

void vbe_init(void);

#endif /* ASM */

#endif /* _VBE_H */
