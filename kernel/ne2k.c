#include "ne2k.h"
#include "debug.h"
#include "lib.h"
#include "irq.h"
#include "paging.h"

/*
 * Interestingly, the NE2000 works a lot like the Sound Blaster 16.
 * To transmit a packet, we first write all of the bytes to the
 * NIC using an outb() loop; then we send a command to the NIC to
 * begin transmission of the packet. While we wait for it to finish,
 * we can begin writing the bytes of the next packet to the NIC.
 * When the NIC has finished transmitting all the bytes, it will
 * raise an interrupt. We can use that interrupt to begin the next
 * transfer if we wrote an entire packet while the previous one was
 * being transmitted.
 *
 * Unlike the Sound Blaster 16, we also need to support input.
 * When we get a RX interrupt, we poll the packets from the NIC,
 * just like the keyboard handler.
 */
#define NE2K_IOBASE 0x300
#define NE2K_PORT(x) (NE2K_IOBASE + (x))

/* Below definitions from QEMU and Linux kernel */
#define NE2K_CMD        NE2K_PORT(0x00)
#define NE2K_CLDALO     NE2K_PORT(0x01) /* Low byte of current local dma addr RD */
#define NE2K_STARTPG    NE2K_PORT(0x01) /* Starting page of ring bfr WR */
#define NE2K_CLDAHI     NE2K_PORT(0x02) /* High byte of current local dma addr RD */
#define NE2K_STOPPG     NE2K_PORT(0x02) /* Ending page +1 of ring bfr WR */
#define NE2K_BOUNDARY   NE2K_PORT(0x03) /* Boundary page of ring bfr RD WR */
#define NE2K_TSR        NE2K_PORT(0x04) /* Transmit status reg RD */
#define NE2K_TPSR       NE2K_PORT(0x04) /* Transmit starting page WR */
#define NE2K_NCR        NE2K_PORT(0x05) /* Number of collision reg RD */
#define NE2K_TCNTLO     NE2K_PORT(0x05) /* Low byte of tx byte count WR */
#define NE2K_FIFO       NE2K_PORT(0x06) /* FIFO RD */
#define NE2K_TCNTHI     NE2K_PORT(0x06) /* High byte of tx byte count WR */
#define NE2K_ISR        NE2K_PORT(0x07) /* Interrupt status reg RD WR */
#define NE2K_CRDALO     NE2K_PORT(0x08) /* low byte of current remote dma address RD */
#define NE2K_RSARLO     NE2K_PORT(0x08) /* Remote start address reg 0 */
#define NE2K_CRDAHI     NE2K_PORT(0x09) /* high byte, current remote dma address RD */
#define NE2K_RSARHI     NE2K_PORT(0x09) /* Remote start address reg 1 */
#define NE2K_RCNTLO     NE2K_PORT(0x0a) /* Remote byte count reg WR */
#define NE2K_RTL8029ID0 NE2K_PORT(0x0a) /* Realtek ID byte #1 RD */
#define NE2K_RCNTHI     NE2K_PORT(0x0b) /* Remote byte count reg WR */
#define NE2K_RTL8029ID1 NE2K_PORT(0x0b) /* Realtek ID byte #2 RD */
#define NE2K_RSR        NE2K_PORT(0x0c) /* rx status reg RD */
#define NE2K_RXCR       NE2K_PORT(0x0c) /* RX configuration reg WR */
#define NE2K_TXCR       NE2K_PORT(0x0d) /* TX configuration reg WR */
#define NE2K_COUNTER0   NE2K_PORT(0x0d) /* Rcv alignment error counter RD */
#define NE2K_DCFG       NE2K_PORT(0x0e) /* Data configuration reg WR */
#define NE2K_COUNTER1   NE2K_PORT(0x0e) /* Rcv CRC error counter RD */
#define NE2K_IMR        NE2K_PORT(0x0f) /* Interrupt mask reg WR */
#define NE2K_COUNTER2   NE2K_PORT(0x0f) /* Rcv missed frame error counter RD */
#define NE2K_DATA       NE2K_PORT(0x10)
#define NE2K_RESET      NE2K_PORT(0x1f)

/* Bits in command register */
#define NE2K_CMD_STOP   0x01 /* Stop and reset the chip */
#define NE2K_CMD_START  0x02 /* Start the chip, clear reset */
#define NE2K_CMD_TRANS  0x04 /* Transmit a frame */
#define NE2K_CMD_RREAD  0x08 /* Remote read */
#define NE2K_CMD_RWRITE 0x10 /* Remote write  */
#define NE2K_CMD_NODMA  0x20 /* Remote DMA */
#define NE2K_CMD_PAGE0  0x00 /* Select page chip registers */
#define NE2K_CMD_PAGE1  0x40 /* using the two high-order bits */
#define NE2K_CMD_PAGE2  0x80 /* Page 3 is invalid. */

/* Bits in interrupt status register */
#define NE2K_ISR_RX       0x01 /* Receiver, no error */
#define NE2K_ISR_TX       0x02 /* Transmitter, no error */
#define NE2K_ISR_RX_ERR   0x04 /* Receiver, with error */
#define NE2K_ISR_TX_ERR   0x08 /* Transmitter, with error */
#define NE2K_ISR_OVER     0x10 /* Receiver overwrote the ring */
#define NE2K_ISR_COUNTERS 0x20 /* Counters need emptying */
#define NE2K_ISR_RDC      0x40 /* remote dma complete */
#define NE2K_ISR_RESET    0x80 /* Reset completed */
#define NE2K_ISR_ALL      0x3f /* Interrupts we will enable */

/* Other used configuration bits */
#define NE2K_DCFG_WORD      0x01
#define NE2K_DCFG_LOOPBACK  0x08
#define NE2K_RXCR_BROADCAST 0x04
#define NE2K_RXCR_MONITOR   0x20
#define NE2K_TXCR_LOOPBACK  0x02

static file_obj_t *open_device = NULL;

/* Reads some bytes from the NE2K PROM */
static void
ne2k_read_mem(int offset, int nbytes, uint8_t *buf)
{
    /* Monitor mode (don't write packets to memory) */
    outb(NE2K_RXCR_MONITOR, NE2K_RXCR);

    /* Loopback mode */
    outb(NE2K_TXCR_LOOPBACK, NE2K_TXCR);

    /* Set number of bytes to read (pretend we're reading words) */
    int count = nbytes << 1;
    outb((count >> 0) & 0xff, NE2K_RCNTLO);
    outb((count >> 8) & 0xff, NE2K_RCNTHI);

    /* Set starting offset */
    outb((offset >> 0) & 0xff, NE2K_RSARLO);
    outb((offset >> 8) & 0xff, NE2K_RSARHI);

    /* Begin transfer */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_RREAD | NE2K_CMD_START, NE2K_CMD);

    /* Read the data (discard the high byte) */
    int i;
    for (i = 0; i < nbytes; ++i) {
        buf[i] = inb(NE2K_DATA);
    }

    /* Disable monitor and loopback, restore default mode */
    outb(NE2K_RXCR_BROADCAST, NE2K_RXCR);
    outb(0x00, NE2K_TXCR);
}

/* Resets the NE2K device. Returns whether the device exists. */
static bool
ne2k_reset(void)
{
    /* Send reset signal */
    outb(NE2K_RESET, inb(NE2K_RESET));

    /* Check for reset ACK (this won't work on real hardware) */
    if ((inb(NE2K_ISR) & NE2K_ISR_RESET) == 0) {
        return false;
    }

    /* Page 0, disable DMA mode */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);

    /* Word access and loopback mode */
    outb(NE2K_DCFG_WORD | NE2K_DCFG_LOOPBACK, NE2K_DCFG);

    /* Reset interrupt status register, mask all interrupts */
    outb(0x00, NE2K_IMR);
    outb(0xff, NE2K_ISR);

    /* Read PROM bytes */
    uint8_t buf[16];
    ne2k_read_mem(0, 16, buf);
    printf("PROM: ");
    int i;
    for (i = 0; i < 16; ++i) {
        printf("%x, ", buf[i]);
    }
    printf("\n");

    /* Check for NE2000 magic bytes */
    if (buf[14] != 0x57 || buf[15] != 0x57) {
        return false;
    }

    /* Enable interrupts except for remote DMA complete and reset */
    outb(NE2K_ISR_ALL, NE2K_IMR);

    return true;
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
    printf("NE2K interrupt received!\n");
}

/* Initializes the NE2000 device */
void
ne2k_init(void)
{
    if (ne2k_reset()) {
        debugf("NE2000 device installed, reset complete\n");
        irq_register_handler(IRQ_NE2K, ne2k_handle_irq);
    } else {
        debugf("NE2000 device not installed\n");
    }
}
