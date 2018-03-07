#ifndef _NE2K_H
#define _NE2K_H

#include "file.h"

#define NET_GET_MAC_ADDRESS 1

#ifndef ASM

/* NE2000 syscall handlers */
int ne2k_open(const char *filename, file_obj_t *file);
int ne2k_read(file_obj_t *file, void *buf, int nbytes);
int ne2k_write(file_obj_t *file, const void *buf, int nbytes);
int ne2k_close(file_obj_t *file);
int ne2k_ioctl(file_obj_t *file, int req, int arg);

/* Initializes the NE2000 device */
void ne2k_init(void);

#endif /* ASM */

#endif /* _NE2K_H */
