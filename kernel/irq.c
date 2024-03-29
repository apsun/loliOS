#include "irq.h"
#include "types.h"
#include "i8259.h"
#include "debug.h"

/* IRQ handler array */
static irq_handler_t irq_handlers[16];

/* IRQ handler */
void
irq_handle_interrupt(int irq_num)
{
    assert(irq_num >= 0 && irq_num < 16);
    irq_handler_t handler = irq_handlers[irq_num];

    /* Clear interrupt flag on PIC */
    i8259_send_eoi(irq_num);

    /* Run callback if it's registered */
    if (handler.callback != NULL) {
        handler.callback();
    }
}

/*
 * Registers an IRQ handler.
 *
 * irq_num should be one of the IRQ_* constants, NOT the
 * INT_IRQ* constants!
 *
 * Currently only one handler can be registered per IRQ line.
 */
void
irq_register_handler(int irq_num, void (*callback)(void))
{
    assert(irq_num >= 0 && irq_num < 16);
    assert(callback != NULL);
    irq_handlers[irq_num].callback = callback;
    i8259_enable_irq(irq_num);
}

/*
 * Unregisters a IRQ handler.
 *
 * irq_num should be one of the IRQ_* constants, NOT the
 * INT_IRQ* constants!
 */
void
irq_unregister_handler(int irq_num)
{
    assert(irq_num >= 0 && irq_num < 16);
    i8259_disable_irq(irq_num);
    irq_handlers[irq_num].callback = NULL;
}
