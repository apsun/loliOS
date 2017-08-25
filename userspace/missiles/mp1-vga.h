#ifndef _MP1_VGA_H
#define _MP1_VGA_H

#include <stdint.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

void draw_char(int32_t x, int32_t y, char c);
void draw_string(int32_t x, int32_t y, const char *s);
void draw_centered_string(int32_t, const char *s);
void clear_screen(void);
void vga_init(void);

#endif /* _MP1_VGA_H */
