#ifndef _VBE_H
#define _VBE_H

#include "types.h"

#ifndef ASM

/* Syscalls */
__cdecl int vbe_vbemap(void **ptr, int xres, int yres, int bpp);
__cdecl int vbe_vbeunmap(void *ptr);

void vbe_init(void);

#endif /* ASM */

#endif /* _VBE_H */
