#include "vbe.h"
#include "types.h"
#include "debug.h"
#include "portio.h"
#include "string.h"
#include "paging.h"
#include "process.h"
#include "terminal.h"
#include "vga.h"

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
#define VBE_FB_SIZE ((int)(VGA_VBE_PAGE_END - VGA_VBE_PAGE_START))

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
 * Number of processes that have the VBE framebuffer mapped into
 * their address space. When this reaches zero, the VGA card is
 * put back into text mode.
 */
static int vbe_refcnt = 0;

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
 * Disables VBE and returns to text mode. This must only be called
 * from vbe_release() or if the kernel is panicking, since it leaves
 * the refcount in an inconsistent state otherwise.
 */
void
vbe_reset(void)
{
    vbe_set_register(VBE_DISPI_INDEX_ENABLE, 0);
    vga_restore_text_mode();
    terminal_reset_framebuffer();
}

/*
 * Increments the framebuffer refcount. Used when forking a process
 * with fbmap active, so that we can disable VBE mode when all
 * processes with the framebuffer mapped are gone.
 */
bool
vbe_retain(bool fbmap)
{
    if (fbmap) {
        assert(vbe_refcnt > 0);
        vbe_refcnt++;
    }
    return fbmap;
}

/*
 * Decrements the framebuffer refcount. If it reaches zero, disables
 * VBE and returns to text mode.
 */
void
vbe_release(bool fbmap)
{
    if (fbmap) {
        assert(vbe_refcnt > 0);
        if (--vbe_refcnt == 0) {
            vbe_reset();
        }
    }
}

/*
 * Updates the fbmap page for the executing process to point to
 * the right location.
 */
void
vbe_update_fbmap_page(bool fbmap)
{
    paging_update_vbe_page(fbmap);
}

/*
 * Puts the VGA card into VBE (graphical) mode and maps the
 * framebuffer into memory. The address of the framebuffer is
 * written to ptr.
 *
 * Only one process may map the framebuffer at a time. The mapping
 * is preserved across fork(), but all forked processes must exit
 * or call fbunmap() before another process may call fbmap() again.
 *
 * Up to 8MB of video memory is supported (half that per frame,
 * due to double buffering) 
 */
__cdecl int
vbe_fbmap(void **ptr, int xres, int yres, int bpp)
{
    if (!vbe_available) {
        debugf("VBE is not supported on this system\n");
        return -1;
    }

    if (vbe_refcnt > 0) {
        debugf("VBE framebuffer is already mapped\n");
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

    /* Copy framebuffer address to userspace */
    void *p = (void *)VGA_VBE_PAGE_START;
    if (!copy_to_user(ptr, &p, sizeof(void *))) {
        return -1;
    }

    /* Update process page mapping */
    pcb_t *pcb = get_executing_pcb();
    pcb->fbmap = true;
    vbe_update_fbmap_page(pcb->fbmap);

    /*
     * Inform terminal that the framebuffer is enabled. This brings
     * the terminal containing this process to the foreground and
     * pins it in place.
     */
    terminal_set_framebuffer(pcb->terminal);

    /*
     * Save the font glyph data so we can restore it when returning
     * from VBE mode (as switching to VBE clobbers video memory,
     * where the font data is stored).
     */
    vga_save_text_mode();

    /* Clear VBE page */
    memset((void *)VGA_VBE_PAGE_START, 0, VBE_FB_SIZE);

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

    vbe_refcnt = 1;
    return 0;
}

/*
 * Unmaps the framebuffer in the calling process and decrements
 * the framebuffer refcount. If it reaches zero, disables the
 * framebuffer and returns to text mode.
 */
__cdecl int
vbe_fbunmap(void *ptr)
{
    if (ptr != (void *)VGA_VBE_PAGE_START) {
        return -1;
    }

    pcb_t *pcb = get_executing_pcb();
    if (!pcb->fbmap) {
        return -1;
    }

    /* Decrement refcount, possibly disable VBE mode */
    vbe_release(pcb->fbmap);

    /* Update process page mapping */
    pcb->fbmap = false;
    vbe_update_fbmap_page(pcb->fbmap);

    return 0;
}

/*
 * Flips the active display. Returns the index of the display that
 * should be written to (0 == write pixels at VBE_PAGE_START, 1 ==
 * write pixels at VBE_PAGE_START + (xres * yres * bytespp) for the
 * next call to vbeflip().
 */
__cdecl int
vbe_fbflip(void *ptr)
{
    if (ptr != (void *)VGA_VBE_PAGE_START) {
        return -1;
    }

    pcb_t *pcb = get_executing_pcb();
    if (!pcb->fbmap) {
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
 * Initializes the VBE driver. Checks whether VBE is available
 * on the system.
 */
void
vbe_init(void)
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
        debugf(
            "Hardware does not support VBE version 0x%04x (got 0x%04x)\n",
            VBE_DISPI_ID_MAGIC, id);
    } else {
        vbe_available = true;
    }
}
