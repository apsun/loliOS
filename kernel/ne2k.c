#include "ne2k.h"
#include "debug.h"
#include "lib.h"

static file_obj_t *open_device = NULL;

/*
 * Acquires exclusive access to the NE2000 device.
 */
int
ne2k_open(const char *filename, file_obj_t *file)
{
    if (open_device != NULL) {
        debugf("Device busy, cannot open\n");
        return -1;
    }

    open_device = file;
    return 0;
}

/*
 * Reads a packet from the NE2K network card inbox.
 * If there are no packets available, this will
 * immediately return zero. If there is a packet
 * available but it is larger than nbytes, this
 * will return -1. (but not consume the packet).
 * Otherwise, copies the packet to buf and returns
 * the length of the packet.
 */
int
ne2k_read(file_obj_t *file, void *buf, int nbytes)
{
    /* TODO */
    return 0;
}

/*
 * Writes a packet to the NE2K network card outbox.
 * If there is no space in the outbox, this returns
 * 0. If the packet is invalid, this returns -1.
 * Otherwise, copies the packet to the outbox and
 * returns the length of the packet (nbytes).
 */
int
ne2k_write(file_obj_t *file, const void *buf, int nbytes)
{
    /* TODO */
    return 0;
}

/*
 * Releases exclusive access to the NE2K device.
 */
int
ne2k_close(file_obj_t *file)
{
    ASSERT(file == open_device);
    open_device = NULL;
    return 0;
}

/*
 * NE2K ioctl() handler.
 */
int
ne2k_ioctl(file_obj_t *file, int req, int arg)
{
    /* No ioctls for now... */
    return -1;
}

