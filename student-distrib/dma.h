#ifndef _DMA_H
#define _DMA_H

#include "types.h"

#ifndef ASM

void
dma_start(
    void *addr,
    uint16_t len,
    uint8_t channel,
    bool write);

#endif /* ASM */

#endif /* _DMA_H */
