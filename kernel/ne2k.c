#include "ne2k.h"
#include "debug.h"
#include "lib.h"
#include "irq.h"
#include "paging.h"

#define NE2K_IOBASE 0x300
#define NE2K_PORT_RESET (NE2K_IOBASE + 0x1F)
#define NE2K_PORT_ISR (NE2K_IOBASE + 0x07)

static file_obj_t *open_device = NULL;

/* Checks whether a NE2K device is installed */
static bool
ne2k_exists(void)
{
    /* TODO */
    return true;
}

/* Resets the NE2K device */
static void
ne2k_reset(void)
{
    /* Send reset signal */
    outb(NE2K_PORT_RESET, inb(NE2K_PORT_RESET));

    /* Reset OK flag = MSB in ISR register */
    while ((inb(NE2K_PORT_ISR) & 0x80) == 0);
}

/* Copies the MAC address of the NE2K device to buf */
static void
ne2k_get_macaddr(uint8_t buf[6])
{
    int i;
    for (i = 0; i < 6; ++i) {
        /* TODO */
    }
}

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
 * Reads a packet from the NE2K device inbox.
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
 * Writes a packet to the NE2K device outbox.
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

/* ioctl() version of ne2k_get_macaddr() */
static int
ne2k_ioctl_get_macaddr(int arg)
{
    /* Copy MAC address to buf */
    uint8_t buf[6];
    ne2k_get_macaddr(buf);

    /* Copy buf to userspace */
    void *ptr = (void *)arg;
    if (!copy_to_user(ptr, buf, sizeof(buf))) {
        return -1;
    }
    return 0;
}

/* NE2K ioctl() handler */
int
ne2k_ioctl(file_obj_t *file, int req, int arg)
{
    switch (req) {
    case NET_GET_MAC_ADDRESS:
        return ne2k_ioctl_get_macaddr(arg);
    default:
        return -1;
    }
}

/* NE2K interrupt handler */
static void
ne2k_handle_irq(void)
{
    /* TODO */
}

/* Initializes the NE2000 device */
void
ne2k_init(void)
{
    if (ne2k_exists()) {
        debugf("NE2000 device installed, initializing\n");
        ne2k_reset();
        irq_register_handler(IRQ_NE2K, ne2k_handle_irq);
    } else {
        debugf("NE2000 device not installed\n");
    }
}
