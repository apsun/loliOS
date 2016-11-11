#define ASM 1

#include "syscall.h"

.data

/*
 * Jump table for the syscall handlers. The order must correspond to
 * the syscall number (syscall number 0 is skipped).
 *
 * Each syscall handler function must be annotated with __cdecl to
 * ensure the order of the arguments!
 */
syscall_jump_table:
    .long process_halt
    .long process_execute
    .long file_read
    .long file_write
    .long file_open
    .long file_close
    .long process_getargs
    .long process_vidmap
    .long process_set_handler
    .long process_sigreturn

.text

/*
 * int32_t syscall_handle(uint32_t a, uint32_t b, uint32_t c, uint32_t num)
 *
 * DO NOT CHANGE THE PARAMETER ORDER, this function (ab)uses cdecl to
 * skip a call instruction, which relies on a, b, c being at the top
 * of the stack!
 *
 * a == ebx
 * b == ecx
 * c == edx
 * num == eax
 */
.globl syscall_handle
syscall_handle:
    movl 16(%esp), %eax
    decl %eax
    cmpl $NUM_SYSCALL, %eax
    jae syscall_handle_invalid
    jmp *syscall_jump_table(, %eax, 4)
syscall_handle_invalid:
    movl $-1, %eax
    ret
