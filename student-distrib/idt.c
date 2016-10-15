#include "idt.h"
#include "x86_desc.h"
#include "lib.h"


#define WRITE_IDT_ENTRY(i, id)                     \
do {                                               \
    extern uint32_t handle_int_thunk_##id;         \
    SET_IDT_ENTRY(idt[i], handle_int_thunk_##id);  \
} while (0)


void
handle_int(int_regs_t *regs)
{
    if (regs->int_num == INT_SYSCALL) {
        printf("Syscall!\n");
    } else if (regs->int_num < NUM_EXC) {
        printf("%s\n", exception_table[regs->int_num]);
        while (1) {
            // Freeze on exception
        }
    } else {
        printf("Unknown interrupt: %d\n", regs->int_num);
    }
}


void
idt_init(void)
{
    idt_desc_t desc;
    int i;

    /* Initialize template interrupt descriptor */
    desc.present      = 1;
    desc.dpl          = 0;
    desc.reserved0    = 0;
    desc.size         = 1;
    desc.reserved1    = 1;
    desc.reserved2    = 1;
    desc.reserved3    = 1;
    desc.reserved4    = 0;
    desc.seg_selector = KERNEL_CS;
    desc.offset_15_00 = 0;
    desc.offset_31_16 = 0;

    /* Load the IDT */
    lidt(idt_desc_ptr);

    /* Initialize exception (trap) gates */
    desc.reserved3 = 1;
    for (i = 0; i < NUM_EXC; ++i) {
        idt[i] = desc;
    }

    /* Write exception handlers */
    WRITE_IDT_ENTRY(0,  EXC_DE);
    WRITE_IDT_ENTRY(1,  EXC_DB);
    WRITE_IDT_ENTRY(2,  EXC_NI);
    WRITE_IDT_ENTRY(3,  EXC_BP);
    WRITE_IDT_ENTRY(4,  EXC_OF);
    WRITE_IDT_ENTRY(5,  EXC_BR);
    WRITE_IDT_ENTRY(6,  EXC_UD);
    WRITE_IDT_ENTRY(7,  EXC_NM);
    WRITE_IDT_ENTRY(8,  EXC_DF);
    WRITE_IDT_ENTRY(9,  EXC_CO);
    WRITE_IDT_ENTRY(10, EXC_TS);
    WRITE_IDT_ENTRY(11, EXC_NP);
    WRITE_IDT_ENTRY(12, EXC_SS);
    WRITE_IDT_ENTRY(13, EXC_GP);
    WRITE_IDT_ENTRY(14, EXC_PF);
    WRITE_IDT_ENTRY(15, EXC_RE);
    WRITE_IDT_ENTRY(16, EXC_MF);
    WRITE_IDT_ENTRY(17, EXC_AC);
    WRITE_IDT_ENTRY(18, EXC_MC);
    WRITE_IDT_ENTRY(19, EXC_XF);

    /* Initialize interrupt gates */
    desc.reserved3 = 0;
    for (; i < NUM_VEC; ++i) {
        idt[i] = desc;
        WRITE_IDT_ENTRY(i, INT_UNKNOWN);
    }

    /* Initialize syscall interrupt gate */
    idt[INT_SYSCALL].dpl = 3;
    idt[INT_SYSCALL].reserved3 = 0;
    WRITE_IDT_ENTRY(INT_SYSCALL, INT_SYSCALL);
}
