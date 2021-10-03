#ifndef _I8259_H
#define _I8259_H

#ifndef ASM

/* Initializes both PICs */
void i8259_init(void);

/* Enables (unmasks) the specified IRQ */
void i8259_enable_irq(int irq_num);

/* Disables (masks) the specified IRQ */
void i8259_disable_irq(int irq_num);

/* Sends end-of-interrupt signal for the specified IRQ */
void i8259_send_eoi(int irq_num);

#endif /* ASM */

#endif /* _I8259_H */
