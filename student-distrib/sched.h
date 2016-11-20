#ifndef _SCHED_H
#define _SCHED_H

#ifndef ASM

#include "process.h"

void sched_switch(int_regs_t *regs);

#endif

#endif
