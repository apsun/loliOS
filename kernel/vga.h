#ifndef _VGA_H
#define _VGA_H

#include "types.h"

/* VGA text mode screen dimensions */
#define VGA_TEXT_COLS 80
#define VGA_TEXT_ROWS 25
#define VGA_TEXT_CHARS (VGA_TEXT_COLS * VGA_TEXT_ROWS)
#define VGA_TEXT_SIZE (VGA_TEXT_CHARS * 2)

#ifndef ASM

/* Saves and restores VGA text mode */
void vga_save_text_mode(void);
void vga_restore_text_mode(void);

/* Sets the text mode cursor location */
void vga_set_cursor_location(int x, int y);

/* Writes a single character at the specified location */
void vga_write_char(uint8_t *mem, int x, int y, char c);

/* Clears the screen in text mode */
void vga_clear_screen(uint8_t *mem, char attrib);

/* Scrolls the screen down one row in text mode */
void vga_scroll_down(uint8_t *mem, char attrib);

#endif /* ASM */

#endif /* _VGA_H */
