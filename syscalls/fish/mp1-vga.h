#ifndef _MP1_VGA_H
#define _MP1_VGA_H

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

void draw_char(int x, int y, char c);
void clear_screen(void);
void vga_init(void);

#endif /* _MP1_VGA_H */
