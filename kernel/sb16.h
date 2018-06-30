#ifndef _SB16_H
#define _SB16_H

#include "types.h"
#include "syscall.h"
#include "file.h"

/* ioctl() commands */
#define SOUND_SET_BITS_PER_SAMPLE 1
#define SOUND_SET_NUM_CHANNELS 2
#define SOUND_SET_SAMPLE_RATE 3

#ifndef ASM

/* Initializes the Sound Blaster 16 device */
void sb16_init(void);

#endif /* ASM */

#endif /* _SB16_H */
