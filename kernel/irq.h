#ifndef _IRQ_H
#define _IRQ_H

/* IRQ number constants */
#define IRQ_PIT      0
#define IRQ_KEYBOARD 1
#define IRQ_COM2     3
#define IRQ_COM1     4
#define IRQ_SB16     5
#define IRQ_RTC      8
#define IRQ_NE2K     9
#define IRQ_MOUSE    12

#ifndef ASM

/* IRQ handler */
typedef struct {
    /* Callback to run when the interrupt occurs */
    void (*callback)(void);
} irq_handler_t;

/* IRQ interrupt handler */
void irq_handle_interrupt(int irq_num);

/* Registers an IRQ handler */
void irq_register_handler(int irq_num, void (*callback)(void));

/* Unregisters an IRQ handler */
void irq_unregister_handler(int irq_num);

#endif /* ASM */

#endif /* _IRQ_H */
