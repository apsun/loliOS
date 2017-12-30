#include "dma.h"
#include "lib.h"
#include "debug.h"

#define DMA_MASK_DISABLE 4

typedef struct {
    uint16_t address_ports[4];
    uint16_t count_ports[4];
    uint16_t page_ports[4];
    uint16_t mask_port;
    uint16_t mode_port;
    uint16_t clear_ff_port;
} dma_info_t;

/* 8-bit slave DMA (channels 0-3) */
static dma_info_t dma1 = {
    .address_ports = {0x00, 0x02, 0x04, 0x06},
    .count_ports = {0x01, 0x03, 0x05, 0x07},
    .page_ports = {0x87, 0x83, 0x81, 0x82},
    .mask_port = 0x0A,
    .mode_port = 0x0B,
    .clear_ff_port = 0x0C,
};

/* 16-bit master DMA (channels 4-7) */
static dma_info_t dma2 = {
    .address_ports = {0xC0, 0xC4, 0xC8, 0xCC},
    .count_ports = {0xC2, 0xC6, 0xCA, 0xCE},
    .page_ports = {0xFFFF, 0x8B, 0x89, 0x8A},
    .mask_port = 0xD4,
    .mode_port = 0xD6,
    .clear_ff_port = 0xD8,
};

typedef union {
    struct {
        uint8_t channel        : 2;
        uint8_t operation      : 2;
        uint8_t auto_init      : 1;
        uint8_t direction      : 1;
        uint8_t mode           : 2;
    } __packed;
    uint8_t raw;
} dma_mode_t;

static void
dma_start_impl(
    const dma_info_t *dma,
    uint8_t channel, /* 0-3 only */
    uint8_t mode,    /* dma_mode_t raw value */
    uint8_t page,    /* bits 16-23 of the physical address, in bytes */
    uint16_t offset, /* bits 0-15 of the physical address, in "units" */
    uint16_t count)  /* number of "units" to transfer */
{
    printf("dma(chan=%d, mode=0x%x, page=%d, offset=0x%x, count=0x%x)\n",
            channel, mode, page, offset, count);

    /* Mask channel */
    outb(channel | DMA_MASK_DISABLE, dma->mask_port);
    
    /* Set DMA mode */
    outb(mode, dma->mode_port);
    
    /* Set buffer offset */
    outb(0x00, dma->clear_ff_port);
    outb((offset >> 0) & 0xff, dma->address_ports[channel]);
    outb((offset >> 8) & 0xff, dma->address_ports[channel]);

    /* Set transfer length in "units" */
    outb(0x00, dma->clear_ff_port);
    outb(((count - 1) >> 0) & 0xff, dma->count_ports[channel]);
    outb(((count - 1) >> 8) & 0xff, dma->count_ports[channel]);

    /* Set buffer page number */
    outb(page, dma->page_ports[channel]);

    /* Unmask channel */
    outb(channel, dma->mask_port);
}

void
dma_start(
    void *addr,
    uint16_t nbytes,
    uint8_t channel,
    bool write)
{
    uint8_t chan_index = channel & 3;
    uint32_t addr_d = (uint32_t)addr;

    ASSERT(channel < 8);

    /* Buffer must be in the first 2^24 bytes of memory */
    ASSERT((addr_d & ~0xffffff) == 0);
    ASSERT(((addr_d + nbytes - 1) & ~0xffffff) == 0);

    dma_mode_t mode;
    mode.channel = chan_index;
    mode.operation = write ? 1 : 2; /* 1 = write, 2 = read */
    mode.auto_init = 0; /* Single cycle */
    mode.direction = 0; /* Incrementing */
    mode.mode = 1; /* Single mode */

    if (channel < 4) {
        printf("8-bit dma\n");
        /* 8-bit DMA */
        dma_start_impl(
            &dma1,
            chan_index,
            mode.raw,
            (addr_d >> 16) & 0xff,
            (addr_d >> 0) & 0xffff,
            nbytes);
    } else {
        printf("16-bit dma\n");
        /* 16-bit DMA */
        ASSERT((addr_d & 1) == 0);
        ASSERT((nbytes & 1) == 0);
        dma_start_impl(
            &dma2,
            chan_index,
            mode.raw,
            (addr_d >> 16) & 0xff,
            (addr_d >> 1) & 0xffff,
            nbytes / 2);
    }
}

