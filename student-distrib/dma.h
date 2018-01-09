#ifndef _DMA_H
#define _DMA_H

#include "types.h"

#define DMA_OP_VERIFY    (0 << 2)
#define DMA_OP_WRITE     (1 << 2)
#define DMA_OP_READ      (2 << 2)
#define DMA_AUTO_INIT    (1 << 4)
#define DMA_REVERSE      (1 << 5)
#define DMA_MODE_DEMAND  (0 << 6)
#define DMA_MODE_SINGLE  (1 << 6)
#define DMA_MODE_BLOCK   (2 << 6)
#define DMA_MODE_CASCADE (3 << 6)

#ifndef ASM

/* Begins a DMA transfer on the specified channel */
void
dma_start(
    void *buf,
    uint16_t nbytes,
    uint8_t channel,
    uint8_t mode);

#endif /* ASM */

#endif /* _DMA_H */
