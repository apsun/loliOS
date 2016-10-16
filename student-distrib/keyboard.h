#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "i8259.h"
#include "idt.h"
//#include "rtc.h"
#include "types.h"
#include "lib.h"

#define ESC 0x1
//#define BACKSPACE
//#define TAB
//#define ENTER 
//#define LCTRL 
#define LSHIFT 0x2A
#define RSHIFT 0x36
//#define SPACE
#define CAPS_LOCK 0x3A

#define KEYBOARD_PORT 0x60
#define CAPS_OFF 0x74 //hex of 58*2
#define SHIFT_OFF 0x3A //hex of 58

void keyboard_init();
void keyboard_input_handler();

#endif