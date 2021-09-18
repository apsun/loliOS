#include "vga.h"
#include "types.h"
#include "debug.h"
#include "portio.h"
#include "string.h"
#include "paging.h"

/*
 * Converts 8-bit R/G/B values to a single 32bpp pixel.
 */
#define RGB32(r, g, b) ( \
    (((r) & 0xff) << 16) | \
    (((g) & 0xff) << 8) | \
    (((b) & 0xff) << 0))

/*
 * Decomposes a 32bpp pixel to the component colors.
 */
#define RGB32_R(rgb32) (((rgb32) >> 16) & 0xff)
#define RGB32_G(rgb32) (((rgb32) >> 8) & 0xff)
#define RGB32_B(rgb32) (((rgb32) >> 0) & 0xff)

/*
 * IO port addresses to access the VBE registers.
 */
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF

/*
 * VGA register index numbers.
 */
#define VBE_DISPI_INDEX_ID 0
#define VBE_DISPI_INDEX_XRES 1
#define VBE_DISPI_INDEX_YRES 2
#define VBE_DISPI_INDEX_BPP 3
#define VBE_DISPI_INDEX_ENABLE 4
#define VBE_DISPI_INDEX_BANK 5
#define VBE_DISPI_INDEX_VIRT_WIDTH 6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET 8
#define VBE_DISPI_INDEX_Y_OFFSET 9

/*
 * Bits in the VBE_DISPI_INDEX_ID register.
 */
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_GETCAPS 0x02
#define VBE_DISPI_8BIT_DAC 0x20
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM 0x80

/*
 * Magic constant for the minimum supported VBE version.
 */
#define VBE_DISPI_ID_MAGIC 0xB0C4

/*
 * How much memory is available for the VBE framebuffer.
 */
#define VBE_FB_SIZE ((int)(VBE_PAGE_END - VBE_PAGE_START))

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
 * Whether VBE is available on the system.
 */
static bool vbe_available = false;

/*
 * Which "display" is currently being written to by userspace.
 * Used to implement double buffering. Can be 0 or 1.
 */
static int vbe_flip = 0;

/*
 * Helper for outb(lo, port); outb(hi, port + 1);
 */
static void
outlh(uint8_t lo, uint8_t hi, uint16_t port)
{
    outw(lo | (hi << 8), port);
}

/*
 * Puts the VGA into font access mode. Fonts can be accessed
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
 * Puts the VGA back into text access mode.
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
 * Resets VGA into text mode.
 */
static void
vga_reset_text_mode(void)
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
}

/*
 * Writes one of the VBE registers. The index must be one of the
 * VBE_DISPI_INDEX_* constants.
 */
static void
vbe_set_register(uint16_t index, uint16_t data)
{
    outw(index, VBE_DISPI_IOPORT_INDEX);
    outw(data, VBE_DISPI_IOPORT_DATA);
}

/*
 * Reads one of the VBE registers. The index must be one of the
 * VBE_DISPI_INDEX_* constants.
 */
static uint16_t
vbe_get_register(uint16_t index)
{
    outw(index, VBE_DISPI_IOPORT_INDEX);
    return inw(VBE_DISPI_IOPORT_DATA);
}

/*
 * Same as vidmap(), but in graphical mode instead of text mode.
 * One process per terminal can acquire exclusive access to the
 * framebuffer at any given time; call vbeunmap() to release the
 * framebuffer.
 *
 * Up to 4MB of video memory is supported.
 */
__cdecl int
vga_vbemap(void **ptr, int xres, int yres, int bpp)
{
    if (!vbe_available) {
        debugf("VBE is not supported on this system\n");
        return -1;
    }

    switch (bpp) {
    case 8:
    case 15:
    case 16:
    case 24:
    case 32:
        break;
    default:
        debugf("Unsupported bpp: %d\n", bpp);
        return -1;
    }

    if ((xres & 0x7) || (yres & 0x7)) {
        debugf("Resolution not 8-px aligned (%d,%d)\n", xres, yres);
        return -1;
    }

    if (xres <= 0 || xres > 16000 || yres <= 0 || yres > 12000) {
        debugf("Resolution out of range (%d,%d)\n", xres, yres);
        return -1;
    }

    /* +1 is needed to round 15bpp up to 2 bytes */
    int bytespp = (bpp + 1) / 8;

    /*
     * Check that we have enough space to hold all pixels, with
     * double buffering (hence divide by 2).
     */
    if (xres * yres * bytespp > VBE_FB_SIZE / 2) {
        debugf("Resolution too large (%d*%d*%d)\n", xres, yres, bpp);
        return -1;
    }

    void *p = (void *)VBE_PAGE_START;
    if (!copy_to_user(ptr, &p, sizeof(void *))) {
        return -1;
    }

    /*
     * Save the font glyph data so we can restore it when returning
     * from VBE mode (as switching to VBE clobbers video memory,
     * where the font data is stored).
     */
    if (!vga_text_font_saved) {
        vga_read_font(vga_text_font);
        vga_text_font_saved = true;
    }

    /* Map and clear VBE page */
    paging_update_vbe_page(true);
    memset((void *)VBE_PAGE_START, 0, VBE_FB_SIZE);

    /* VBE must be disabled while we change xres/yres/bpp */
    vbe_set_register(VBE_DISPI_INDEX_ENABLE, 0);
    vbe_set_register(VBE_DISPI_INDEX_XRES, xres);
    vbe_set_register(VBE_DISPI_INDEX_YRES, yres);
    vbe_set_register(VBE_DISPI_INDEX_BPP, bpp);
    vbe_set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_LFB_ENABLED | VBE_DISPI_ENABLED);

    /* Set up virtual display for double buffering */
    vbe_set_register(VBE_DISPI_INDEX_VIRT_WIDTH, xres);
    vbe_set_register(VBE_DISPI_INDEX_X_OFFSET, 0);
    vbe_set_register(VBE_DISPI_INDEX_Y_OFFSET, 0);
    vbe_flip = 0;

    return 0;
}

/*
 * Releases the framebuffer, disabling VBE and returning to
 * text mode.
 */
__cdecl int
vga_vbeunmap(void *ptr)
{
    if (ptr != (void *)VBE_PAGE_START) {
        return -1;
    }

    /* If we never saved the font, VBE was never enabled in the first place */
    if (!vga_text_font_saved) {
        return 0;
    }

    /* Disable VBE mode */
    vbe_set_register(VBE_DISPI_INDEX_ENABLE, 0);

    /* Return to VGA text mode */
    vga_reset_text_mode();

    /* Restore font data */
    vga_write_font(vga_text_font);

    /* Unmap VBE page */
    paging_update_vbe_page(false);

    return 0;
}

/*
 * Flips the active display. Returns the index of the display that
 * should be written to (0 == write pixels at VBE_PAGE_START, 1 ==
 * write pixels at VBE_PAGE_START + (xres * yres * bytespp) for the
 * next call to vbeflip().
 */
__cdecl int
vga_vbeflip(void *ptr)
{
    if (ptr != (void *)VBE_PAGE_START) {
        return -1;
    }

    /* Point the display to the memory region we just wrote */
    uint16_t yres = vbe_get_register(VBE_DISPI_INDEX_YRES);
    vbe_set_register(VBE_DISPI_INDEX_Y_OFFSET, vbe_flip * yres);

    /* Toggle the active region */
    vbe_flip = !vbe_flip;
    return vbe_flip;
}

/*
 * Sets the VGA cursor location register.
 */
void
vga_set_cursor_location(uint16_t location)
{
    outlh(0x0E, (location >> 8) & 0xff, VGA_PORT_CRTC);
    outlh(0x0F, (location >> 0) & 0xff, VGA_PORT_CRTC);
}

/*
 * Initializes the VGA driver.
 */
void
vga_init(void)
{
    /*
     * Check if system supports the Bochs VBE extensions. QEMU
     * supports up up to 0xB0C4 properly. To check for this, write
     * the version to the ID register and try to read it back;
     * if we get a lower or different number, it's unsupported.
     */
    vbe_set_register(VBE_DISPI_INDEX_ID, VBE_DISPI_ID_MAGIC);
    uint16_t id = vbe_get_register(VBE_DISPI_INDEX_ID);
    if (id != VBE_DISPI_ID_MAGIC) {
        debugf("Hardware does not support VBE version 0x%04x (got 0x%04x)\n", VBE_DISPI_ID_MAGIC, id);
    } else {
        vbe_available = true;
    }
}
