#include "mp1-vga.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <syscall.h>

/* Needs to be visible to mp1.S, so not static */
unsigned char *vmem_base_addr;

void
draw_char(int x, int y, char c)
{
    vmem_base_addr[(y * SCREEN_WIDTH + x) << 1] = c;
}

void
draw_string(int x, int y, const char *s)
{
    while (*s) {
        draw_char(x++, y, *s++);
    }
}

void
draw_centered_string(int y, const char *s)
{
    draw_string((SCREEN_WIDTH - strlen(s)) / 2, y, s);
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
