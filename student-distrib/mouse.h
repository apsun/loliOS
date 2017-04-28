#ifndef _MOUSE_H
#define _MOUSE_H

#include "types.h"
#include "file.h"

/* Mouse packet flags */
#define MOUSE_LEFT (1 << 0)
#define MOUSE_RIGHT (1 << 1)
#define MOUSE_MIDDLE (1 << 2)
#define MOUSE_X_SIGN (1 << 4)
#define MOUSE_Y_SIGN (1 << 5)
#define MOUSE_X_OVERFLOW (1 << 6)
#define MOUSE_Y_OVERFLOW (1 << 7)

#ifndef ASM

/* Mouse syscall handlers */
int32_t mouse_open(const uint8_t *filename, file_obj_t *file);
int32_t mouse_read(file_obj_t *file, void *buf, int32_t nbytes);
int32_t mouse_write(file_obj_t *file, const void *buf, int32_t nbytes);
int32_t mouse_close(file_obj_t *file);

/* Handles mouse interrupts */
void mouse_handle_irq(uint8_t data[3]);

#endif /* ASM */

#endif /* _MOUSE_H */
