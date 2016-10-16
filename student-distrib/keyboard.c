/*
* keyboard input handler
* "Copyright (c) 2016 by Emre Ulusoy."
*/
#include "keyboard.h"

int caps_flag;  //both are binary
int shift_flag;

char keyboard_reference[232] = { //58 each
	'\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
	'\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
	'\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	'\0', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '\0', '*',
	'\0', ' ',
	'\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', //shift
	'\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
	'\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
	'\0', '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '\0', '*',
	'\0', ' ',
	'\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', //caps
	'\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']',
	'\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
	'\0', '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '\0', '*',
	'\0', ' ',
	'\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', //shift and caps
	'\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}',
	'\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~',
	'\0', '|', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', '\0', '*',
	'\0', ' ',
};

void keyboard_init(){
	//or use cli() ??
	enable_irq(1); //should match a previous disable_irq
	//not really because cli() already disabled interrupts for ALL processors
	// we enabled line 1 (maybe fix magic #)
}

void keyboard_input_handler(){
	/* Clear interrupt flag - disables interrupts on this processor */
	cli();
	
	int print_char = '\0';
	int input = inb(KEYBOARD_PORT);
	if(input == '\0')
		return;
	if(input > 58){
		printf("Out of range.\n");
		return;
	}
	switch(input){
		case ESC:
			printf("Successfully escaped!\n");
			break;
		//case BACKSPACE:

		//case TAB: //    '\t' means tab

		//case ENTER:

		//case LCTRL:
		//case RCTRL:

		case LSHIFT:
		case RSHIFT:
			shift_flag = ~shift_flag;
		//case SPACE:

		case CAPS_LOCK:
			~caps_flag;
		default:
			print_char = keyboard_reference[input + (shift_flag*SHIFT_OFF) + (caps_flag*CAPS_OFF)];

	}
	send_eoi(1);
	sti();
	//asm??
}