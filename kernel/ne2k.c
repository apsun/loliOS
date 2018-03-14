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
 *
 * The NE2k has some memory (not shared with our normal RAM), divided
 * into 256B pages. We can allocate a portion of it to be used for
 * transmitting packets, and the remaining portion as a ring buffer
 * for receiving packets. Each packet is always page-aligned, so
 * there may be excess space at the end that is unused.
 *
 * TX slot
 *     |  TX slot 1            curr page      boundary
 *     |      |                    |             |
 *     v      v     [1]    [2]     v             v     [0]
 * [  TX  |  TX  |  RX  |  RX  | FREE | FREE | FREE |  RX  ]
 * |_____________|_________________________________________|
 *    12 pages          Ring buffer (remaining pages)
 *
 * With 12 pages for TX, we can just barely hold two maximum-sized
 * Ethernet frames (12 * 256B = 3072B = 2 * 1536B).
 *
 * The boundary represents the last page in the ring buffer that
 * is free. The current page represents the first page that is free.
 * In circular queue terms, the boundary is the head (minus 1),
 * and the current page is the tail.
 *
 * Note that the NE2k also has three "register pages", which are
 * completely unrelated to the "pages" above. These hold memory-mapped
 * (again, in NE2k memory, not main RAM) configuration registers.
 * Writing to page 0 or page 2 will modify the config registers;
 * writing to page 1 will modify the physical and multicast address
 * registers. In all pages, 0x00 is still used to send a command,
 * 0x10 is used to send data, and 0x1f is used to reset the device.
 */
#define NE2K_IOBASE 0x300
#define NE2K_PORT(x) (NE2K_IOBASE + (x))

/* Common register numbers */
#define NE2K_CMD        NE2K_PORT(0x00)
#define NE2K_DATA       NE2K_PORT(0x10)
#define NE2K_RESET      NE2K_PORT(0x1f)

/* Registers in page 0 */
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

/* Registers in page 1 */
#define NE2K_PHYS(i)    NE2K_PORT((i) + 1)
#define NE2K_CURPAG     NE2K_PORT(0x07)
#define NE2K_MULT(i)    NE2K_PORT((i) + 8)

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

/* Other used configuration bits */
#define NE2K_DCFG_WORD      0x01
#define NE2K_DCFG_LOOPBACK  0x08
#define NE2K_RXCR_BROADCAST 0x04
#define NE2K_RXCR_MONITOR   0x20
#define NE2K_TXCR_LOOPBACK  0x02

/* Common mode bits */
#define NE2K_ISR_ALL  0x3f
#define NE2K_RXCR_OFF NE2K_RXCR_MONITOR
#define NE2K_TXCR_OFF NE2K_TXCR_LOOPBACK
#define NE2K_RXCR_ON  NE2K_RXCR_BROADCAST
#define NE2K_TXCR_ON  0x00

/* Offsets in NE2k memory */
#define NE2K_TX_START_PAGE 0x40
#define NE2K_RX_STOP_PAGE  0x80
#define NE2K_TX_PAGES      12
#define NE2K_RX_START_PAGE (NE2K_TX_START_PAGE + NE2K_TX_PAGES)

/* Saved MAC address */
static uint8_t mac_addr[6];

/* Next buffer to be transmitted (0 or 1) */
static int next_tx_buffer = 0;

/* Contexts of next packet to be transmitted */
#if 0 /* TODO */
static bool next_packet_ready = false;
static uint8_t next_packet[1536];
#endif

/* Reads the contents of the NE2k memory */
static void
ne2k_read_mem(void *buf, int offset, int nbytes)
{
    outb((nbytes >> 0) & 0xff, NE2K_RCNTLO);
    outb((nbytes >> 8) & 0xff, NE2K_RCNTHI);

    /* Set starting offset */
    outb((offset >> 0) & 0xff, NE2K_RSARLO);
    outb((offset >> 8) & 0xff, NE2K_RSARHI);

    /* Begin transfer */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_RREAD | NE2K_CMD_START, NE2K_CMD);

    /* Read the data */
    uint16_t *bufp = buf;
    int i;
    for (i = 0; i < nbytes / 2; ++i) {
        bufp[i] = inw(NE2K_DATA);
    }

    /* Stop transfer */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);
}

/* Resets the NE2k device. Returns whether the device exists. */
static bool
ne2k_reset(void)
{
    /* Send reset signal */
    outb(NE2K_RESET, inb(NE2K_RESET));

    /* Check for reset ACK (this should be a loop on real hardware) */
    if ((inb(NE2K_ISR) & NE2K_ISR_RESET) == 0) {
        return false;
    }

    /* Write to page 0 */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);

    /* Word access and loopback mode */
    outb(NE2K_DCFG_WORD | NE2K_DCFG_LOOPBACK, NE2K_DCFG);

    /* Disable tx and rx */
    outb(NE2K_RXCR_OFF, NE2K_RXCR);
    outb(NE2K_TXCR_OFF, NE2K_TXCR);

    /* Mask interrupts */
    outb(0x00, NE2K_IMR);
    outb(0xff, NE2K_ISR);

    /*
     * Read PROM bytes. For some reason, the bytes are duplicated,
     * most likely due to existing drivers being set in word access
     * mode and buggy hardware ignoring the mode when reading PROM.
     * Hence, we also need to read in words, then discard the high byte.
     */
    uint16_t prom[16];
    ne2k_read_mem(prom, 0, sizeof(prom));

    /* Check for NE2k magic bytes */
    if ((prom[14] & 0xff) != 0x57 || (prom[15] & 0xff) != 0x57) {
        return false;
    }

    /* Save MAC address */
    int i;
    for (i = 0; i < 6; ++i) {
        mac_addr[i] = (uint8_t)prom[i];
    }

    /* Reset byte counter */
    outb(0x00, NE2K_RCNTLO);
    outb(0x00, NE2K_RCNTHI);

    /* Set up memory regions for tx and rx */
    outb(NE2K_TX_START_PAGE, NE2K_TPSR);
    outb(NE2K_RX_START_PAGE, NE2K_STARTPG);
    outb(NE2K_RX_STOP_PAGE, NE2K_STOPPG);
    outb(NE2K_RX_STOP_PAGE - 1, NE2K_BOUNDARY);

    /* Reset the next tx buffer */
    next_tx_buffer = 0;

    /* Copy MAC address to physical address registers, set curr page */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE1 | NE2K_CMD_STOP, NE2K_CMD);
    for (i = 0; i < 6; ++i) {
        outb(mac_addr[i], NE2K_PHYS(i));
    }
    outb(NE2K_RX_START_PAGE, NE2K_CURPAG);
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);

    /* Unmask interrupts */
    outb(0xff, NE2K_ISR);
    outb(NE2K_ISR_ALL, NE2K_IMR);

    /* Enable packet reception */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_START, NE2K_CMD);

    /* Re-enable tx and rx */
    outb(NE2K_RXCR_ON, NE2K_RXCR);
    outb(NE2K_TXCR_ON, NE2K_TXCR);

    return true;
}

/* Packet receive handler */
static void
ne2k_handle_rx(void)
{
    while (1) {
        /* Read the current page (aka the tail of the ring buffer) */
        outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE1 | NE2K_CMD_STOP, NE2K_CMD);
        uint8_t tail_pg = inb(NE2K_CURPAG);
        outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);

        /* Dequeue the first page from the ring buffer */
        uint8_t head_pg = inb(NE2K_BOUNDARY) + 1;
        if (head_pg >= NE2K_RX_STOP_PAGE) {
            head_pg = NE2K_RX_START_PAGE;
        }

        /* Stop if there are no more packets to read */
        if (head_pg == tail_pg) {
            break;
        }

        /* Dump packet header */
        uint8_t buf[4];
        ne2k_read_mem(buf, head_pg << 8, 4);
        uint16_t len = buf[2] | (buf[3] << 8);
        printf("status=%x next=%d count=%d\n", buf[0], buf[1], len);

        /* Dump packet body */
        uint8_t pkt[1514];
        ne2k_read_mem(pkt, (head_pg << 8) + 4, len);
        int i, j;
        for (i = 0; i < len; i += 16) {
            for (j = 0; j < 16; ++j) {
                printf("%*x ", pkt[j + 16 * i]);
            }
            printf("\n");
        }

        break;
    }
}

/* Packet transmit handler */
static void
ne2k_handle_tx(void)
{
    printf("TX!\n");
}

/* NE2k interrupt handler */
static void
ne2k_handle_irq(void)
{
    /* Select page 0 to read ISR and temporarily disable device */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);

    /* Handle interrupts */
    uint8_t isr;
    while ((isr = inb(NE2K_ISR)) != 0) {
        /* Received a good packet */
        if (isr & NE2K_ISR_RX) {
            ne2k_handle_rx();
        }

        /* Transmitted a packet */
        if (isr & NE2K_ISR_TX) {
            ne2k_handle_tx();
        }

        /*
         * Since we are running an emulated card, it is impossible
         * to get corrupted packets or have our transmission fail
         * (if it does, it will be on the actual card, not in QEMU).
         * Hence, we can ignore all error conditions. Here, we just
         * acknowledge ALL the interrupts!
         *
         * Okay fine, this is an incredibly lazy approach. If you
         * mail me a legacy computer that's actually running a NE2k
         * card, maybe I'll actually implement error handling. Until
         * then...
         */
        outb(isr, NE2K_ISR);
    }

    /* Re-enable device */
    outb(NE2K_CMD_NODMA | NE2K_CMD_PAGE0 | NE2K_CMD_START, NE2K_CMD);
}

/* Initializes the NE2k device */
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
