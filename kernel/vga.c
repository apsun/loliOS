#include "vga.h"
#include "types.h"
#include "debug.h"
#include "portio.h"
#include "string.h"
#include "paging.h"

/*
 * IO port addresses to access the VGA registers.
 */
#define VGA_PORT_SEQ 0x03C4
#define VGA_PORT_CRTC 0x03D4
#define VGA_PORT_ATTR 0x03C0
#define VGA_PORT_GFX 0x03CE
#define VGA_PORT_IS1 0x03DA
#define VGA_PORT_MISC 0x03C2

/*
 * Magical incantation to return to VGA text mode.
 */
static const uint8_t vga_text_seq[] = {
    0x03, /* Reset Register */
    0x00, /* Clocking Mode Register */
    0x03, /* Map Mask Register */
    0x00, /* Character Map Select Register */
    0x02, /* Sequencer Memory Mode Register */
};
static const uint8_t vga_text_crtc[] = {
    0x5F, /* Horizontal Total Register */
    0x4F, /* End Horizontal Display Register */
    0x50, /* Start Horizontal Blanking Register */
    0x82, /* End Horizontal Blanking Register */
    0x55, /* Start Horizontal Retrace Register */
    0x81, /* End Horizontal Retrace Register */
    0xBF, /* Vertical Total Register */
    0x1F, /* Overflow Register */
    0x00, /* Preset Row Scan Register */
    0x4F, /* Maximum Scan Line Register */
    0x0D, /* Cursor Start Register */
    0x0E, /* Cursor End Register */
    0x00, /* Start Address High Register */
    0x00, /* Start Address Low Register */
    0x00, /* Cursor Location High Register */
    0x00, /* Cursor Location Low Register */
    0x9C, /* Vertical Retrace Start Register */
    0x8E, /* Vertical Retrace End Register */
    0x8F, /* Vertical Display End Register */
    0x28, /* Offset Register */
    0x1F, /* Underline Location Register */
    0x96, /* Start Vertical Blanking Register */
    0xB9, /* End Vertical Blanking */
    0xA3, /* CRTC Mode Control Register */
    0xFF, /* Line Compare Register */
};
static const uint8_t vga_text_attr[] = {
    /* Palette Registers */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,

    0x0C, /* Attribute Mode Control Register */
    0x00, /* Overscan Color Register */
    0x0F, /* Color Plane Enable Register */
    0x08, /* Horizontal Pixel Panning Register */
    0x00, /* Color Select Register */
};
static const uint8_t vga_text_gfx[] = {
    0x00, /* Set/Reset Register */
    0x00, /* Enable Set/Reset Register */
    0x00, /* Color Compare Register */
    0x00, /* Data Rotate Register */
    0x00, /* Read Map Select Register */
    0x10, /* Graphics Mode Register */
    0x0E, /* Miscellaneous Graphics Register */
    0x0F, /* Color Don't Care Register */
    0xFF, /* Bit Mask Register */
};

/*
 * Holds font data saved from VGA memory.
 */
static uint8_t vga_text_font[256][16];
static bool vga_text_font_saved = false;

/*
 * Helper for outb(lo, port); outb(hi, port + 1);
 */
static void
outlh(uint8_t lo, uint8_t hi, uint16_t port)
{
    outw(lo | (hi << 8), port);
}

/*
 * Puts the VGA card into font access mode. Fonts can be accessed
 * in 0xA0000~0xB0000 in banks of 8KB (32B/char * 256chars).
 */
static void
vga_begin_font_access(void)
{
    /*
     * Implementation note: font glyphs are stored in plane 2.
     * Normally planes 0 and 1 are mapped in even/odd mode for
     * character and attributes correspondingly. To set font
     * data, we disable even/odd mode and select plane 2 for
     * writing.
     *
     * Each character is 8x16, but takes up 32B (first 16B is
     * the data, remaining 16B is ignored).
     *
     * Technically, we don't need to remap the video addresses,
     * since we only ever actually use the first bank of
     * characters (8KB in size).
     */

    /* Write to plane 2 */
    outlh(0x02, 0x04, VGA_PORT_SEQ);

    /* Disable odd/even write */
    outlh(0x04, 0x06, VGA_PORT_SEQ);

    /* Read from plane 2 */
    outlh(0x04, 0x02, VGA_PORT_GFX);

    /* Disable odd/even read */
    outlh(0x05, 0x00, VGA_PORT_GFX);

    /* Map 0xA0000~0xB0000 (64KB, enough for all 8 font banks) */
    outlh(0x06, 0x04, VGA_PORT_GFX);
}

/*
 * Puts the VGA card back into text access mode.
 */
static void
vga_end_font_access(void)
{
    outlh(0x02, vga_text_seq[0x02], VGA_PORT_SEQ);
    outlh(0x04, vga_text_seq[0x04], VGA_PORT_SEQ);
    outlh(0x04, vga_text_gfx[0x04], VGA_PORT_GFX);
    outlh(0x05, vga_text_gfx[0x05], VGA_PORT_GFX);
    outlh(0x06, vga_text_gfx[0x06], VGA_PORT_GFX);
}

/*
 * Reads font glyph data from VGA memory.
 */
static void
vga_read_font(uint8_t font[256][16])
{
    vga_begin_font_access();
    int i;
    for (i = 0; i < 256; ++i) {
        memcpy(font[i], (void *)(VGA_FONT_PAGE_START + 32 * i), 16);
    }
    vga_end_font_access();
}

/*
 * Writes font glyph data into VGA memory.
 */
static void
vga_write_font(const uint8_t font[256][16])
{
    vga_begin_font_access();
    int i;
    for (i = 0; i < 256; ++i) {
        memcpy((void *)(VGA_FONT_PAGE_START + 32 * i), font[i], 16);
    }
    vga_end_font_access();
}

/*
 * Saves the VGA text mode font. Must be called before
 * vga_restore_text_mode().
 */
void
vga_save_text_mode(void)
{
    if (!vga_text_font_saved) {
        vga_read_font(vga_text_font);
        vga_text_font_saved = true;
    }
}

/*
 * Puts the VGA card back into text mode and restores
 * all font data.
 */
void
vga_restore_text_mode(void)
{
    int i;

    /* Write sequencer registers */
    for (i = 0; i < array_len(vga_text_seq); ++i) {
        outlh(i, vga_text_seq[i], VGA_PORT_SEQ);
    }

    /* Disable CRTC register protection */
    outlh(0x11, 0x00, VGA_PORT_CRTC);

    /* Write CRTC registers */
    for (i = 0; i < array_len(vga_text_crtc); ++i) {
        outlh(i, vga_text_crtc[i], VGA_PORT_CRTC);
    }

    /* Reset attribute register flip-flop */
    inb(VGA_PORT_IS1);

    /* Write attribute registers */
    for (i = 0; i < array_len(vga_text_attr); ++i) {
        outb(i, VGA_PORT_ATTR);
        outb(vga_text_attr[i], VGA_PORT_ATTR);
    }

    /* Write graphics registers */
    for (i = 0; i < array_len(vga_text_gfx); ++i) {
        outlh(i, vga_text_gfx[i], VGA_PORT_GFX);
    }

    /* Write misc register */
    outb(0x67, VGA_PORT_MISC);

    /* Disable blanking */
    outb(0x20, VGA_PORT_ATTR);

    /* Restore font data */
    if (vga_text_font_saved) {
        vga_write_font(vga_text_font);
    }
}

/*
 * Sets the VGA text mode cursor location.
 */
void
vga_set_cursor_location(int x, int y)
{
    assert(x >= 0 && x < VGA_TEXT_COLS);
    assert(y >= 0 && y < VGA_TEXT_ROWS);

    uint16_t pos = y * VGA_TEXT_COLS + x;
    outlh(0x0E, (pos >> 8) & 0xff, VGA_PORT_CRTC);
    outlh(0x0F, (pos >> 0) & 0xff, VGA_PORT_CRTC);
}

/*
 * Writes a single character at the specified location.
 */
void
vga_write_char(uint8_t *mem, int x, int y, char c)
{
    int offset = (y * VGA_TEXT_COLS + x) * 2;
    mem[offset] = c;
}

/*
 * Clears a region of text mode memory starting at mem with
 * the specified attribute byte.
 */
static void
vga_clear_region(uint8_t *mem, int nchars, uint8_t attrib)
{
    uint16_t pattern = ('\x00' << 0) | (attrib << 8);
    memset_word(mem, pattern, nchars);
}

/*
 * Clears the screen in text mode.
 */
void
vga_clear_screen(uint8_t *mem, uint8_t attrib)
{
    vga_clear_region(mem, VGA_TEXT_CHARS, attrib);
}

/*
 * Scrolls the screen down one row in text mode.
 */
void
vga_scroll_down(uint8_t *mem, uint8_t attrib)
{
    int bytes_per_row = VGA_TEXT_COLS * 2;
    int shift_count = VGA_TEXT_SIZE - bytes_per_row;

    /* Shift rows forward by one row */
    memmove(mem, mem + bytes_per_row, shift_count);

    /* Clear out last row */
    vga_clear_region(mem + shift_count, VGA_TEXT_COLS, attrib);
}
