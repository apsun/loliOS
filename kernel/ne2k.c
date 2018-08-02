#include "ne2k.h"
#include "debug.h"
#include "lib.h"
#include "irq.h"
#include "list.h"
#include "skb.h"
#include "net.h"
#include "ethernet.h"

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
 * there may be excess space at the end of some pages.
 *
 * TX slot 0
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
 * and the current page is the tail (plus 1).
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
#define NE2K_ENRSR_RXOK     0x01

/* Common mode bits */
#define NE2K_ISR_ALL  0x3f
#define NE2K_RXCR_OFF NE2K_RXCR_MONITOR
#define NE2K_TXCR_OFF NE2K_TXCR_LOOPBACK
#define NE2K_RXCR_ON  NE2K_RXCR_BROADCAST
#define NE2K_TXCR_ON  0x00

/* Offsets in NE2k memory */
#define NE2K_TX_START_PAGE  0x40
#define NE2K_RX_STOP_PAGE   0x80
#define NE2K_PAGES_PER_PKT  6
#define NE2K_BYTES_PER_PAGE 256
#define NE2K_TX_PAGES       (2 * NE2K_PAGES_PER_PKT)
#define NE2K_RX_START_PAGE  (NE2K_TX_START_PAGE + NE2K_TX_PAGES)

/* Forward declaration */
static int ne2k_send(net_dev_t *dev, skb_t *skb);

/* NE2k device */
static net_dev_t ne2k_dev = {
    .name = "NE2000",
    .send_mac_skb = ne2k_send,
};

/*
 * Ethernet interface, built upon the NE2k device.
 * This probably shouldn't be here, but in some
 * ifconfig.c file that performs DHCP and interface
 * name allocation. However, since we're only
 * supporting QEMU anyways, we can get away with
 * just hard-coding these values.
 */
static net_iface_t eth0 = {
    .name = "eth0",
    .subnet_mask = IP(255, 255, 255, 0),
    .ip_addr = IP(10, 0, 2, 15),
    .gateway_addr = IP(10, 0, 2, 2),
    .dev = &ne2k_dev,
    .send_ip_skb = ethernet_send_ip,
};

/* NE2k frame header */
typedef struct {
    uint8_t status;
    uint8_t next;
    uint16_t size;
} ne2k_hdr_t;

/* Whether we're currently transmitting a packet */
static bool tx_busy = false;

/* Buffer number currently being transmitted */
static int tx_buf = 0;

/* Length of data in each tx buffer, 0 = free buffer */
static int tx_buf_len[2];

/* Packets waiting to be sent */
static list_declare(tx_queue);

/* Sets the remote DMA byte offset and count */
static void
ne2k_config_dma(int offset, int nbytes)
{
    /* Set number of bytes to read */
    outb((nbytes >> 0) & 0xff, NE2K_RCNTLO);
    outb((nbytes >> 8) & 0xff, NE2K_RCNTHI);

    /* Set starting offset */
    outb((offset >> 0) & 0xff, NE2K_RSARLO);
    outb((offset >> 8) & 0xff, NE2K_RSARHI);
}

/* Reads the contents of the NE2k memory */
static void
ne2k_read_mem(void *buf, int offset, int nbytes)
{
    /* Set up transfer */
    ne2k_config_dma(offset, nbytes);
    outb(NE2K_ISR_RDC, NE2K_ISR);
    outb(NE2K_CMD_NODMA | NE2K_CMD_RREAD, NE2K_CMD);

    /* Read the data */
    uint16_t *bufw = buf;
    int i;
    for (i = 0; i < nbytes / 2; ++i) {
        bufw[i] = inw(NE2K_DATA);
    }
    if (nbytes & 1) {
        uint8_t *bufb = buf;
        bufb[nbytes - 1] = inb(NE2K_DATA);
    }

    /* Wait for RDC confirmation */
    while ((inb(NE2K_ISR) & NE2K_ISR_RDC) == 0);
    outb(NE2K_ISR_RDC, NE2K_ISR);
}

/* Writes the contents of the NE2k memory */
static void
ne2k_write_mem(int offset, void *buf, int nbytes)
{
    /* Set up transfer */
    ne2k_config_dma(offset, nbytes);
    outb(NE2K_ISR_RDC, NE2K_ISR);
    outb(NE2K_CMD_NODMA | NE2K_CMD_RWRITE, NE2K_CMD);

    /* Write the data */
    uint16_t *bufw = buf;
    int i;
    for (i = 0; i < nbytes / 2; ++i) {
        outw(bufw[i], NE2K_DATA);
    }
    if (nbytes & 1) {
        uint8_t *bufb = buf;
        outb(bufb[nbytes - 1], NE2K_DATA);
    }

    /* Wait for RDC confirmation */
    while ((inb(NE2K_ISR) & NE2K_ISR_RDC) == 0);
    outb(NE2K_ISR_RDC, NE2K_ISR);
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

    /* Write to page 0, stop device */
    outb(NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);

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

    /* Reset byte counter */
    outb(0x00, NE2K_RCNTLO);
    outb(0x00, NE2K_RCNTHI);

    /* Set up memory regions for tx and rx */
    outb(NE2K_TX_START_PAGE, NE2K_TPSR);
    outb(NE2K_RX_START_PAGE, NE2K_STARTPG);
    outb(NE2K_RX_STOP_PAGE, NE2K_STOPPG);
    outb(NE2K_RX_STOP_PAGE - 1, NE2K_BOUNDARY);

    /* Copy MAC address to physical address registers, set curr page */
    outb(NE2K_CMD_PAGE1 | NE2K_CMD_STOP, NE2K_CMD);
    int i;
    for (i = 0; i < 6; ++i) {
        ne2k_dev.mac_addr.bytes[i] = (uint8_t)prom[i];
        outb(ne2k_dev.mac_addr.bytes[i], NE2K_PHYS(i));
    }
    outb(NE2K_RX_START_PAGE, NE2K_CURPAG);
    outb(NE2K_CMD_PAGE0 | NE2K_CMD_STOP, NE2K_CMD);

    /* Reset tx status */
    tx_busy = false;
    tx_buf_len[0] = 0;
    tx_buf_len[1] = 0;

    /* Unmask interrupts */
    outb(0xff, NE2K_ISR);
    outb(NE2K_ISR_ALL, NE2K_IMR);

    /* Re-enable tx and rx */
    outb(NE2K_RXCR_ON, NE2K_RXCR);
    outb(NE2K_TXCR_ON, NE2K_TXCR);

    /* Enable packet reception */
    outb(NE2K_CMD_START, NE2K_CMD);

    return true;
}

/*
 * Packet receive handler. This will deliver any good
 * packets to the Ethernet level for further processing.
 */
static void
ne2k_handle_rx(void)
{
    while (true) {
        /* Read the current page (aka the tail of the ring buffer) */
        outb(NE2K_CMD_PAGE1, NE2K_CMD);
        uint8_t tail_pg = inb(NE2K_CURPAG);
        outb(NE2K_CMD_PAGE0, NE2K_CMD);

        /* Dequeue the first page from the ring buffer */
        uint8_t head_pg = inb(NE2K_BOUNDARY) + 1;
        if (head_pg >= NE2K_RX_STOP_PAGE) {
            head_pg = NE2K_RX_START_PAGE;
        }

        /* Stop if there are no more packets to read */
        if (head_pg == tail_pg) {
            break;
        }

        /* Read NE2k header */
        int offset = head_pg * NE2K_BYTES_PER_PAGE;
        ne2k_hdr_t hdr;
        ne2k_read_mem(&hdr, offset, sizeof(ne2k_hdr_t));

        /* Check OK flag, drop packet if invalid */
        if ((hdr.status & NE2K_ENRSR_RXOK) != 0) {
            /* Allocate SKB to hold frame */
            int eth_size = hdr.size - sizeof(ne2k_hdr_t);
            skb_t *skb = skb_alloc(eth_size);
            if (skb == NULL) {
                debugf("Failed to allocate SKB for incoming packet\n");
                break;
            }

            /* Read Ethernet frame content */
            void *body = skb_put(skb, eth_size);
            ne2k_read_mem(body, offset + sizeof(ne2k_hdr_t), eth_size);

            /* Deliver packet to Ethernet layer */
            ethernet_handle_rx(&ne2k_dev, skb);
            skb_release(skb);
        } else {
            debugf("Received invalid packet, dropping\n");
        }

        /* Move to next packet */
        uint8_t new_boundary = hdr.next - 1;
        if (new_boundary < NE2K_RX_START_PAGE) {
            new_boundary = NE2K_RX_STOP_PAGE - 1;
        }
        outb(new_boundary, NE2K_BOUNDARY);
    }
}

/*
 * Begins transmission of a packet in the NE2k memory.
 * We will receive a tx interrupt when transmission finishes.
 */
static void
ne2k_begin_tx(void)
{
    int len = tx_buf_len[tx_buf];
    assert(len > 0);
    assert(!tx_busy);

    /* Mask interrupts, busy = ON */
    tx_busy = true;

    /* Set tx length and offset */
    outb((len >> 0) & 0xff, NE2K_TCNTLO);
    outb((len >> 8) & 0xff, NE2K_TCNTHI);
    outb(NE2K_TX_START_PAGE + tx_buf * NE2K_PAGES_PER_PKT, NE2K_TPSR);

    /* Go! */
    outb(NE2K_CMD_TRANS, NE2K_CMD);
}

/*
 * Copies a frame to the NE2k memory so that it can be
 * transmitted.
 */
static void
ne2k_copy_to_tx(int buf, skb_t *skb)
{
    int page = NE2K_TX_START_PAGE + buf * NE2K_PAGES_PER_PKT;
    ne2k_write_mem(page * NE2K_BYTES_PER_PAGE, skb_data(skb), skb_len(skb));
    tx_buf_len[buf] = skb_len(skb);
}

/*
 * Packet transmit handler. Handles completion of a
 * transmission by the device. If there is another
 * packet in NE2k memory ready to be sent, this will
 * re-start the transmission process.
 */
static void
ne2k_handle_tx(void)
{
    /* Mark current tx slot as free */
    tx_busy = false;
    tx_buf_len[tx_buf] = 0;

    /*
     * If we have more packets to send, copy the first
     * one to the slot that just finished. The invariant
     * is that there can only be a packet in the queue
     * if both tx buffers were full.
     */
    if (!list_empty(&tx_queue)) {
        skb_t *skb = list_first_entry(&tx_queue, skb_t, list);
        ne2k_copy_to_tx(tx_buf, skb);
        list_del(&skb->list);
        skb_release(skb);
    }

    /* Begin transmitting the other tx slot */
    tx_buf = !tx_buf;
    if (tx_buf_len[tx_buf] > 0) {
        ne2k_begin_tx();
    }
}

/* NE2k interrupt handler */
static void
ne2k_handle_irq(void)
{
    /* Handle interrupts */
    uint8_t isr;
    while ((isr = inb(NE2K_ISR)) & (NE2K_ISR_RX | NE2K_ISR_RX_ERR | NE2K_ISR_TX)) {
        /*
         * The order of the acknowledgements and ne2k_handle_*
         * calls is important! The interrupt handler may trigger
         * the sending of another packet, so we must first ack
         * the interrupt, THEN call the handler.
         *
         * Since we are running an emulated card, it is impossible
         * to get corrupted packets or have our transmission fail
         * (if it does, it will be on the actual card, not in QEMU).
         * Hence, we can ignore all error conditions.
         */

        /* Received a packet */
        if (isr & (NE2K_ISR_RX | NE2K_ISR_RX_ERR)) {
            outb(NE2K_ISR_RX | NE2K_ISR_RX_ERR, NE2K_ISR);
            ne2k_handle_rx();
        }

        /* Transmitted a packet */
        if (isr & NE2K_ISR_TX) {
            outb(NE2K_ISR_TX, NE2K_ISR);
            ne2k_handle_tx();
        }
    }
}

/*
 * Sends a Ethernet frame. Intended to be called from elsewhere
 * in the kernel. Returns -1 if the packet could not be sent, or
 * 0 if it was sent or was successfully enqueued.
 */
static int
ne2k_send(net_dev_t *dev, skb_t *skb)
{
    /* Find a free tx buffer to store our packet in */
    int buf;
    if (!tx_busy) {
        buf = tx_buf;
    } else if (tx_buf_len[!tx_buf] == 0) {
        buf = !tx_buf;
    } else {
        /*
         * Need to clone this SKB, since it's possible for a higher
         * level to re-transmit the same SKB at a later time which
         * will break stuff. Enqueueing is a pretty rare operation
         * anyways, so this should have minimal performance impact.
         */
        list_add_tail(&skb_clone(skb)->list, &tx_queue);
        return 0;
    }

    /* Copy packet to NE2k memory */
    ne2k_copy_to_tx(buf, skb);

    /* Begin transmission if device is not busy */
    if (!tx_busy) {
        ne2k_begin_tx();
    }

    return 0;
}

/* Initializes the NE2k device */
void
ne2k_init(void)
{
    if (ne2k_reset()) {
        debugf("NE2000 device installed, reset complete\n");
        irq_register_handler(IRQ_NE2K, ne2k_handle_irq);
        net_register_interface(&eth0);
    } else {
        debugf("NE2000 device not installed\n");
    }
}
