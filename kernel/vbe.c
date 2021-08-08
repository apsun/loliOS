#include "vbe.h"
#include "debug.h"
#include "lib.h"
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
 * Register index numbers.
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
 * How much memory is available for the framebuffer.
 */
#define VBE_FB_SIZE ((int)(VBE_PAGE_END - VBE_PAGE_START))

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
 * Up to 8MB of video memory is supported (i.e. 1920x1080x32bpp).
 */
__cdecl int
vbe_vbemap(uint8_t **ptr, int xres, int yres, int bpp)
{
    // TODO
    return -1;
}

/*
 * Releases the framebuffer.
 */
__cdecl int
vbe_vbeunmap(uint8_t *ptr)
{
    // TODO
    return -1;
}

void
vbe_init(void)
{
    /*
     * QEMU supports up up to 0xB0C4 properly. To check for this,
     * write the version to the ID register and try to read it back;
     * if we get a lower or different number, it's unsupported.
     */
    vbe_set_register(VBE_DISPI_INDEX_ID, VBE_DISPI_ID_MAGIC);
    uint16_t id = vbe_get_register(VBE_DISPI_INDEX_ID);
    if (id != VBE_DISPI_ID_MAGIC) {
        debugf("Hardware does not support VBE version 0x%04x (got 0x%04x)\n", VBE_DISPI_ID_MAGIC, id);
        return;
    }

    /*
     * VBE must be disabled while we change xres/yres/bpp.
     */
    vbe_set_register(VBE_DISPI_INDEX_ENABLE, 0);
    vbe_set_register(VBE_DISPI_INDEX_XRES, 1920);
    vbe_set_register(VBE_DISPI_INDEX_YRES, 1080);
    vbe_set_register(VBE_DISPI_INDEX_BPP, 32);
    vbe_set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_LFB_ENABLED | VBE_DISPI_ENABLED);

    /*
     * TODO: Simple gradient lerp algorithm for testing
     *
     * https://uigradients.com/#AzurLane
     */
    uint32_t *vbemem = (uint32_t *)VBE_PAGE_START;
    uint32_t start = 0x007f7fd5;
    uint32_t end = 0x0091eae4;
    int i;
    for (i = 0; i < 1920 * 1080; ++i) {
        vbemem[i] = RGB32(
            RGB32_R(start) + (RGB32_R(end) - RGB32_R(start)) * (i % 1920) / 1920,
            RGB32_G(start) + (RGB32_G(end) - RGB32_G(start)) * (i % 1920) / 1920,
            RGB32_B(start) + (RGB32_B(end) - RGB32_B(start)) * (i % 1920) / 1920);
    }
}
