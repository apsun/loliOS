#include "i8259.h"
#include "types.h"
#include "debug.h"
#include "portio.h"

/* Ports that each PIC sits on */
#define MASTER_8259_PORT_CMD  0x20
#define MASTER_8259_PORT_DATA 0x21
#define SLAVE_8259_PORT_CMD   0xA0
#define SLAVE_8259_PORT_DATA  0xA1

/* Initialization control words to init each PIC */
#define ICW1          0x11
#define ICW2_MASTER   0x20
#define ICW2_SLAVE    0x28
#define ICW3_MASTER   0x04
#define ICW3_SLAVE    0x02
#define ICW4          0x01

/* Constant for masking all interrupts */
#define MASK_ALL 0xff

/* IRQ index of the slave PIC */
#define IRQ_SLAVE 2

/*
 * End-of-interrupt byte. This gets OR'd with
 * the interrupt number and sent out to the PIC
 * to declare the interrupt finished.
 */
#define EOI 0x60

/*
 * Interrupt masks to determine which interrupts
 * are enabled and disabled. Master always has the
 * slave IRQ line enabled since we treat the slave
 * as part of the master PIC.
 */
static uint8_t i8259_master_mask = MASK_ALL & ~(1 << IRQ_SLAVE);
static uint8_t i8259_slave_mask  = MASK_ALL;

/* Initializes the 8259 PIC */
void
i8259_init(void)
{
    /* Mask interrupts */
    outb(MASK_ALL, MASTER_8259_PORT_DATA);
    outb(MASK_ALL, SLAVE_8259_PORT_DATA);

    /* Init master PIC */
    outb(ICW1,        MASTER_8259_PORT_CMD);
    outb(ICW2_MASTER, MASTER_8259_PORT_DATA);
    outb(ICW3_MASTER, MASTER_8259_PORT_DATA);
    outb(ICW4,        MASTER_8259_PORT_DATA);

    /* Init slave PIC */
    outb(ICW1,       SLAVE_8259_PORT_CMD);
    outb(ICW2_SLAVE, SLAVE_8259_PORT_DATA);
    outb(ICW3_SLAVE, SLAVE_8259_PORT_DATA);
    outb(ICW4,       SLAVE_8259_PORT_DATA);

    /* Unmask interrupts */
    outb(i8259_master_mask, MASTER_8259_PORT_DATA);
    outb(i8259_slave_mask,  SLAVE_8259_PORT_DATA);
}

/* Enables (unmasks) the specified IRQ */
void
i8259_enable_irq(int irq_num)
{
    assert(irq_num >= 0 && irq_num < 16);
    debugf("Enabling IRQ#%d\n", irq_num);
    if (irq_num < 8) {
        i8259_master_mask &= ~(1 << irq_num);
        outb(i8259_master_mask, MASTER_8259_PORT_DATA);
    } else {
        int slave_irq_num = irq_num - 8;
        i8259_slave_mask &= ~(1 << slave_irq_num);
        outb(i8259_slave_mask, SLAVE_8259_PORT_DATA);
    }
}

/* Disables (masks) the specified IRQ */
void
i8259_disable_irq(int irq_num)
{
    assert(irq_num >= 0 && irq_num < 16);
    debugf("Disabling IRQ#%d\n", irq_num);
    if (irq_num < 8) {
        i8259_master_mask |= (1 << irq_num);
        outb(i8259_master_mask, MASTER_8259_PORT_DATA);
    } else {
        int slave_irq_num = irq_num - 8;
        i8259_slave_mask |= (1 << slave_irq_num);
        outb(i8259_slave_mask, SLAVE_8259_PORT_DATA);
    }
}

/* Sends end-of-interrupt signal for the specified IRQ */
void
i8259_send_eoi(int irq_num)
{
    assert(irq_num >= 0 && irq_num < 16);
    if (irq_num < 8) {
        outb(irq_num | EOI, MASTER_8259_PORT_CMD);
    } else {
        int slave_irq_num = irq_num - 8;
        outb(slave_irq_num | EOI, SLAVE_8259_PORT_CMD);
        outb(IRQ_SLAVE | EOI, MASTER_8259_PORT_CMD);
    }
}
