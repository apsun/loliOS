#ifndef _SB16_H
#define _SB16_H

#include "types.h"
#include "syscall.h"
#include "file.h"

#define SOUND_SET_BITS_PER_SAMPLE 1
#define SOUND_SET_NUM_CHANNELS 2
#define SOUND_SET_SAMPLE_RATE 3

#ifndef ASM

/* Sound Blaster 16 syscall handlers */
int32_t sb16_open(const char *filename, file_obj_t *file);
int32_t sb16_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t sb16_write(file_obj_t *file, const void *buf, int32_t nbytes);
int32_t sb16_close(file_obj_t *file);
int32_t sb16_ioctl(file_obj_t *file, uint32_t req, uint32_t arg);

/* Initializes the Sound Blaster 16 device */
void sb16_init(void);

#endif /* ASM */

#endif /* _SB16_H */
