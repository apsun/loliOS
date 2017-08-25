#include "mp1-vga.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

/* Needs to be visible to mp1.S, so not static */
uint8_t *vmem_base_addr;

void
draw_char(int32_t x, int32_t y, char c)
{
    vmem_base_addr[(y * SCREEN_WIDTH + x) << 1] = c;
}

void
clear_screen(void)
{
    int32_t x, y;
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
