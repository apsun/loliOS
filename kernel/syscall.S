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
    .long signal_sigaction
    .long signal_sigreturn
    .long signal_sigmask
    .long signal_kill
    .long file_ioctl
    .long rtc_time
    .long process_sbrk
    .long socket_socket
    .long socket_bind
    .long socket_connect
    .long socket_listen
    .long socket_accept
    .long socket_recvfrom
    .long socket_sendto
    .long socket_getsockname
    .long socket_getpeername
    .long file_dup
    .long process_fork
    .long process_exec
    .long process_wait
    .long process_getpid
    .long process_getpgrp
    .long process_setpgrp
    .long terminal_tcgetpgrp
    .long terminal_tcsetpgrp
    .long pipe_pipe
    .long file_create
    .long file_fcntl

.text

/*
 * int syscall_handle(
 *     uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e,
 *     int_regs_t *regs, int num)
 *
 * DO NOT CHANGE THE PARAMETER ORDER, this function (ab)uses cdecl to
 * skip a call instruction, which relies on a, b, c, regs being at the top
 * of the stack!
 *
 * a == ebx
 * b == ecx
 * c == edx
 * d == esi
 * e == edi
 * regs == all registers
 * num == eax
 */
.globl syscall_handle
syscall_handle:
    movl 28(%esp), %eax
    decl %eax
    cmpl $NUM_SYSCALL, %eax
    jae syscall_handle_invalid
    jmp *syscall_jump_table(, %eax, 4)
syscall_handle_invalid:
    movl $-1, %eax
    ret
