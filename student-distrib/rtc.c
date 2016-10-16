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

int rate = 0xF //15

void rtc_init(){
	disable_irq(8); //RTC IRQ line
	outb(0x8B, 0x70); //reg B, index port
	int prev_val = inb(0x71); //data port
	outb(0x8B, 0x70); //reset to B
	outb(prev_val | 0x44, 0x71); //enable periodic interrupt and binary format
	// bit 6(7th) = PIE, bit 2 = DM
	outb(0x8A, 0x70); //reg A, index port
	int prev_a = inb(0x71); //data port
	outb(0x8A, 0x70); //reset to A
	outb(prev_a | 0xF0, 0x71); // place rate into LSBs and keep MSBs

	enable_irq(8); //IRQ_RTC
	//IRQ_SLAVE??
}

void rtc_interrupt_handler(){
	outb(0x8C, 0x70); // reg c, index port
	inb(0x71); //data port
	test_interrupts();
	send_eoi(8); //irq rtc
}
