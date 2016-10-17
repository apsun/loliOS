#include "rtc.h"
#include "i8259.h"
#include "idt.h"
#include "lib.h"
#include "debug.h"

static uint8_t
read_reg(uint8_t reg)
{
    outb(reg, RTC_PORT_INDEX);
    return inb(RTC_PORT_DATA);
}

static void
write_reg(uint8_t reg, uint8_t value)
{
    outb(reg, RTC_PORT_INDEX);
    outb(value, RTC_PORT_DATA);
}

static void
handle_rtc_irq(void)
{
    /* Read from register C, ignore value */
    read_reg(RTC_REG_C);

    /* For RTC testing */
    // test_interrupts();
}

void
rtc_init(void)
{
    /* Read RTC registers */
    uint8_t reg_a = read_reg(RTC_REG_A);
    uint8_t reg_b = read_reg(RTC_REG_B);

    debugf("RTC register A: 0x%#x\n", reg_a);
    debugf("RTC register B: 0x%#x\n", reg_b);

    /* Set periodic interrupt rate to 2Hz */
    reg_a &= ~RTC_A_RS;
    reg_a |= RTC_A_RS_2;

    /* Enable periodic interrupts */
    reg_b |= RTC_B_PIE;

    /* Use binary format */
    reg_b |= RTC_B_DM;

    /* Write RTC registers */
    write_reg(RTC_REG_A, reg_a);
    write_reg(RTC_REG_B, reg_b);

    /* Register RTC IRQ handler and enable interrupts */
    register_irq_handler(IRQ_RTC, handle_rtc_irq);
}
