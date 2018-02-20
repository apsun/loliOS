#ifndef _I8259_H
#define _I8259_H

#include "types.h"

/* Externally-visible functions */

#ifndef ASM

/* Initialize both PICs */
void i8259_init(void);

/* Enable (unmask) the specified IRQ */
void i8259_enable_irq(int irq_num);

/* Disable (mask) the specified IRQ */
void i8259_disable_irq(int irq_num);

/* Send end-of-interrupt signal for the specified IRQ */
void i8259_send_eoi(int irq_num);

#endif /* ASM */

#endif /* _I8259_H */
