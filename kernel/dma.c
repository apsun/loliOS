#include "dma.h"
#include "debug.h"
#include "portio.h"

#define DMA_MASK_DISABLE 4

/* DMA port info */
typedef struct {
    uint16_t address_ports[4];
    uint16_t count_ports[4];
    uint16_t page_ports[4];
    uint16_t mask_port;
    uint16_t mode_port;
    uint16_t clear_ff_port;
} dma_info_t;

/* 8-bit slave DMA (channels 0-3) */
static const dma_info_t dma1 = {
    .address_ports = {0x00, 0x02, 0x04, 0x06},
    .count_ports = {0x01, 0x03, 0x05, 0x07},
    .page_ports = {0x87, 0x83, 0x81, 0x82},
    .mask_port = 0x0A,
    .mode_port = 0x0B,
    .clear_ff_port = 0x0C,
};

/* 16-bit master DMA (channels 4-7) */
static const dma_info_t dma2 = {
    .address_ports = {0xC0, 0xC4, 0xC8, 0xCC},
    .count_ports = {0xC2, 0xC6, 0xCA, 0xCE},
    .page_ports = {0xFFFF, 0x8B, 0x89, 0x8A},
    .mask_port = 0xD4,
    .mode_port = 0xD6,
    .clear_ff_port = 0xD8,
};

/* Generic DMA transfer start implementation */
static void
dma_start_impl(
    const dma_info_t *dma,
    uint8_t channel, /* 0-3 only */
    uint8_t mode,    /* dma_mode_t raw value */
    uint8_t page,    /* bits 16-23 of the physical address, in bytes */
    uint16_t offset, /* bits 0-15 of the physical address, in "units" */
    uint16_t count)  /* number of "units" to transfer, minus 1 */
{
    /* Mask channel */
    outb(channel | DMA_MASK_DISABLE, dma->mask_port);

    /* Set DMA mode */
    outb(mode, dma->mode_port);

    /* Set buffer offset */
    outb(0x00, dma->clear_ff_port);
    outb((offset >> 0) & 0xff, dma->address_ports[channel]);
    outb((offset >> 8) & 0xff, dma->address_ports[channel]);

    /* Set transfer length in "units" minus 1 */
    outb(0x00, dma->clear_ff_port);
    outb((count >> 0) & 0xff, dma->count_ports[channel]);
    outb((count >> 8) & 0xff, dma->count_ports[channel]);

    /* Set buffer page number */
    outb(page, dma->page_ports[channel]);

    /* Unmask channel */
    outb(channel, dma->mask_port);
}

/* Begins a DMA transfer on the specified channel */
void
dma_start(
    void *buf,
    int nbytes,
    int channel,
    int mode)
{
    /* Basic sanity check */
    assert((mode & (~0xff | 3)) == 0);

    /* Buffer must be in the first 16MB = 2^24 bytes of memory */
    uint32_t addr = (uint32_t)buf;
    assert((addr & ~0xffffff) == 0);
    assert(((addr + nbytes - 1) & ~0xffffff) == 0);

    if (channel < 4) {
        /* 8-bit DMA */
        dma_start_impl(
            &dma1,
            channel,
            mode | channel,
            (addr >> 16) & 0xff,
            (addr >> 0) & 0xffff,
            (nbytes >> 0) - 1);
    } else {
        /* 16-bit DMA */
        assert((addr & 1) == 0);
        assert((nbytes & 1) == 0);
        dma_start_impl(
            &dma2,
            channel - 4,
            mode | (channel - 4),
            (addr >> 16) & 0xff,
            (addr >> 1) & 0xffff,
            (nbytes >> 1) - 1);
    }
}
