#include "syscall.h"
#include "file.h"
#include "idt.h"

#define cdecl __attribute__((cdecl))

/* halt syscall dispatch function */
static cdecl int32_t
syscall_halt(uint32_t status)
{
    /* Only the least-significant byte is valid */
    status &= 0xff;

    /* TODO: CP3 */
    return -1;
}

/* execute syscall dispatch function */
static cdecl int32_t
syscall_execute(const uint8_t* command)
{
    /* TODO: CP3 */
    return -1;
}

/* read syscall dispatch function */
static cdecl int32_t
syscall_read(int32_t fd, void* buf, int32_t nbytes)
{
    return file_read(fd, buf, nbytes);
}

/* write syscall dispatch function */
static cdecl int32_t
syscall_write(int32_t fd, const void* buf, int32_t nbytes)
{
    return file_write(fd, buf, nbytes);
}

/* open syscall dispatch function */
static cdecl int32_t
syscall_open(const uint8_t* filename)
{
    return file_open(filename);
}

/* close syscall dispatch function */
static cdecl int32_t
syscall_close(int32_t fd)
{
    return file_close(fd);
}

/* getargs syscall dispatch function */
static cdecl int32_t
syscall_getargs(uint8_t* buf, int32_t nbytes)
{
    /* TODO: CP4 */
    return -1;
}

/* vidmap syscall dispatch function */
static cdecl int32_t
syscall_vidmap(uint8_t** screen_start)
{
    /* TODO: CP4 */
    return -1;
}

/* set_handler syscall dispatch function */
static cdecl int32_t
syscall_set_handler(int32_t signum, void* handler)
{
    /* TODO: CP4 */
    return -1;
}

/* sigreturn syscall dispatch function */
static cdecl int32_t
syscall_sigreturn(void)
{
    /* TODO: CP4 */
    return -1;
}

/*
 * Syscall dispatch table.
 *
 * This relies on the x86 cdecl calling convention, so that
 * excess paramters are ignored.
 */
typedef cdecl int32_t (*syscall_func)(uint32_t, uint32_t, uint32_t);
static syscall_func syscall_funcs[10] = {
    (syscall_func)syscall_halt,
    (syscall_func)syscall_execute,
    (syscall_func)syscall_read,
    (syscall_func)syscall_write,
    (syscall_func)syscall_open,
    (syscall_func)syscall_close,
    (syscall_func)syscall_getargs,
    (syscall_func)syscall_vidmap,
    (syscall_func)syscall_set_handler,
    (syscall_func)syscall_sigreturn,
};

/*
 * Syscall dispatch function.
 *
 * num - the syscall number (1~10, inclusive)
 * a, b, c - the arguments to the syscall routine
 */
int32_t
syscall_handle(uint32_t num, uint32_t a, uint32_t b, uint32_t c)
{
    return syscall_funcs[num - 1](a, b, c);
}
