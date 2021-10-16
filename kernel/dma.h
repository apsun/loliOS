#ifndef _DMA_H
#define _DMA_H

#ifndef ASM

/* Constants for DMA mode parameter */
typedef enum {
    DMA_OP_VERIFY    = (0 << 2),
    DMA_OP_WRITE     = (1 << 2),
    DMA_OP_READ      = (2 << 2),
    DMA_AUTO_INIT    = (1 << 4),
    DMA_REVERSE      = (1 << 5),
    DMA_MODE_DEMAND  = (0 << 6),
    DMA_MODE_SINGLE  = (1 << 6),
    DMA_MODE_BLOCK   = (2 << 6),
    DMA_MODE_CASCADE = (3 << 6),
} dma_mode_t;

/* Begins a DMA transfer on the specified channel */
void
dma_start(
    void *buf,
    int nbytes,
    int channel,
    dma_mode_t mode);

#endif /* ASM */

#endif /* _DMA_H */
