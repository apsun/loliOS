#include "sched.h"
#include "paging.h"
#include "terminal.h"
#include "x86_desc.h"
#include "debug.h"

void
sched_switch(void)
{
	pcb_t *curr = get_executing_pcb();
	pcb_t *next = get_next_pcb();
	/* Save current stack pointer after interrupt happened */
	asm volatile("movl %%esp, %0;" 		/* Assign curr->esp the value of stack pointer */
				 "movl %%ebp, %1;"		/* Assign curr->ebp the value of base pointer */
				 : "=g"(curr->esp),
				   "=g"(curr->ebp)
				 );

	/* If the process has not yet started we call process_run(pcb_t *pcb) */
	if (next->status == PROCESS_SCHED) {
		asm volatile("movl %0, %%eax;"
					 "movl %1, %%esp;" 		/* Switch kernel stack of the new process */
					 "movl %1, %%ebp;"
					 "pushl %%eax;" 		/* Execute the process */
					 "call process_run;"
					 :
					 : "r"(next),
					   "r"(next->kernel_stack));
	} else if (next->status == PROCESS_RUN) {
		/* update paging */
		paging_update_process_page(next->pid);

		/* Update vidmap status */
	    terminal_update_vidmap(next->terminal, next->vidmap);

	    /* upadate tss */
	    tss.esp0 = next->kernel_stack;

	    /* switch to next process */
	    asm volatile("movl %0, %%esp;" 		/* Switch kernel stack frame of the new process */
					 "movl %1, %%ebp;"
					 :
					 : "r"(next->esp),
					   "r"(next->ebp));
	}
}

