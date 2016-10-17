#include "i8259.h"
#include "lib.h"
#include "debug.h"

/*
 * Interrupt masks to determine which interrupts
 * are enabled and disabled. Master always has the
 * slave IRQ line enabled since we treat the slave
 * as part of the master PIC.
 */
static uint8_t master_mask = MASK_ALL & ~(1 << IRQ_SLAVE);
static uint8_t slave_mask  = MASK_ALL;

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
    outb(master_mask, MASTER_8259_PORT_DATA);
    outb(slave_mask,  SLAVE_8259_PORT_DATA);
}

/* Enable (unmask) the specified IRQ */
void
i8259_enable_irq(uint32_t irq_num)
{
    ASSERT(irq_num >= 0 && irq_num < 16);
    debugf("Enabling IRQ#%d\n", irq_num);
    if (irq_num < 8) {
        master_mask &= ~(1 << irq_num);
        outb(master_mask, MASTER_8259_PORT_DATA);
    } else {
        uint32_t slave_irq_num = irq_num - 8;
        slave_mask &= ~(1 << slave_irq_num);
        outb(slave_mask, SLAVE_8259_PORT_DATA);
    }
}

/* Disable (mask) the specified IRQ */
void
i8259_disable_irq(uint32_t irq_num)
{
    ASSERT(irq_num >= 0 && irq_num < 16);
    debugf("Disabling IRQ#%d\n", irq_num);
    if (irq_num < 8) {
        master_mask |= (1 << irq_num);
        outb(master_mask, MASTER_8259_PORT_DATA);
    } else {
        uint32_t slave_irq_num = irq_num - 8;
        slave_mask |= (1 << slave_irq_num);
        outb(slave_mask, SLAVE_8259_PORT_DATA);
    }
}

/* Send end-of-interrupt signal for the specified IRQ */
void
i8259_send_eoi(uint32_t irq_num)
{
    ASSERT(irq_num >= 0 && irq_num < 16);
    debugf("Sending EOI for IRQ#%d\n", irq_num);
    if (irq_num < 8) {
        outb(irq_num | EOI, MASTER_8259_PORT_CMD);
    } else {
        uint32_t slave_irq_num = irq_num - 8;
        outb(slave_irq_num | EOI, SLAVE_8259_PORT_CMD);
        outb(IRQ_SLAVE | EOI, MASTER_8259_PORT_CMD);
    }
}
