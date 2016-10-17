/* RTC IRQ line = 8
* 
* index port: 0x70
* data port: 0x71
* reg A, B, C: 0x- 8A, 8B, 8C
* 
*
*
* by: Emre
*
*/
#include "rtc.h"
#define index_port 0x70
#define data_port 0x71
#define RTC_IRQ_LINE 8
#define reg_A 0x8A
#define reg_B 0x8B
#define reg_C 0x8C
#define LOW_THIRD_MASK 0x44
#define BITMASK_TOPFOUR 0xF0
int rate = 0xF; //15

void rtc_init(){
	disable_irq(RTC_IRQ_LINE); //RTC IRQ line
	outb(reg_B, index_port); //reg B, index port
	int prev_val = inb(data_port); //data port
	outb(reg_B,index_port); //reset to B
	outb(prev_val | LOW_THIRD_MASK, data_port); //enable periodic interrupt and binary format
	// bit 6(7th) = PIE, bit 2 = DM
	outb(reg_A, index_port); //reg A, index port
	int prev_a = inb(data_port); //data port
	outb(reg_A, index_port); //reset to A
	outb(prev_a | BITMASK_TOPFOUR, data_port); // place rate into LSBs and keep MSBs

	enable_irq(RTC_IRQ_LINE); //IRQ_RTC
	//IRQ_SLAVE??
}

void rtc_interrupt_handler(){
	outb(reg_C, index_port); // reg c, index port
	inb(data_port); //data port
	test_interrupts();
	send_eoi(RTC_IRQ_LINE); //irq rtc
}
