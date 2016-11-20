#include "sched.h"
#include "paging.h"
#include "terminal.h"
#include "x86_desc.h"
#include "debug.h"

/* 
 * Switch to the next process. 
 */
static void 
sched_switch_to_kernel(pcb_t *next) {
	int_regs_kernel_t *next_regs = NULL;
	int_regs_kernel_t *next_content = (int_regs_kernel_t*)(&next->context);

	asm volatile("movl %1, %%esp;" 		/* Switch to the kernel stack of next process */
				 "subl $60, %%esp;" 	/* Make room for int_regs_kernel_t */
				 "subl $12, %%esp;"
				 "movl %%esp, %0;" 		/* Assign next_regs the address of stack */
				 : "=g"(next_regs)
				 : "g"(next_content->esp)
				 : "memory");
	
	/* Copy the context to next process's kernel stack */
	*next_regs = *next_content;
	
	asm volatile("jmp sched_kernel_interrupt_thunk;");
}

/* 
 * Replace regs with the context of next process and
 * go back idt_handle_common_thunk and IRET to user space
 */
static void 
sched_switch_to_user(pcb_t *next, int_regs_t *regs) {
	*regs = next->context;
}

void
sched_switch(int_regs_t *regs)
{
	pcb_t *curr = get_executing_pcb();
	pcb_t *next = get_next_pcb();
	/* save current status of all regs */
	curr->context = *regs;
	/* If PIT */
	if (regs->cs == KERNEL_CS) {
		curr->context.padding0 = 0;
		curr->context.padding1 = 0;
		curr->context.ss = KERNEL_DS;		
		curr->context.esp = regs->error_code;
	}
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
		if (next->context.cs == KERNEL_CS) {
			sched_switch_to_kernel(next);
		} else {
			sched_switch_to_user(next, regs);
		}
	}
}

