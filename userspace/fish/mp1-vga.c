#include "mp1-vga.h"
#include <assert.h>
#include <stddef.h>
#include <syscall.h>

/* Needs to be visible to mp1.S, so not static */
unsigned char *vmem_base_addr;

void
draw_char(int x, int y, char c)
{
    vmem_base_addr[(y * SCREEN_WIDTH + x) << 1] = c;
}

void
clear_screen(void)
{
    int x, y;
    for (y = 0; y < SCREEN_HEIGHT; ++y) {
        for (x = 0; x < SCREEN_WIDTH; ++x) {
            draw_char(x, y, ' ');
        }
    }
}

void
vga_init(void)
{
    if (vidmap(&vmem_base_addr) < 0) {
        assert(0);
    }
}
