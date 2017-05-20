#ifndef _IRQ_H
#define _IRQ_H

#include "types.h"

/* Number of IRQ lines */
#define NUM_IRQ 16

/* IRQ number constants */
#define IRQ_PIT      0
#define IRQ_KEYBOARD 1
#define IRQ_RTC      8
#define IRQ_MOUSE    12

#ifndef ASM

/* IRQ handler */
typedef struct irq_handler_t
{
    /* Callback to run when the interrupt occurs */
    void (*callback)(void);
} irq_handler_t;

/* IRQ interrupt handler */
void irq_handle_interrupt(uint32_t irq_num);

/* Registers an IRQ handler */
void irq_register_handler(uint32_t irq_num, void (*callback)(void));

/* Unregisters an IRQ handler */
void irq_unregister_handler(uint32_t irq_num);

#endif /* ASM */

#endif /* _IRQ_H */
