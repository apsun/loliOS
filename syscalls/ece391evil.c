#include <stdint.h>
#include "ece391support.h"
#include "ece391syscall.h"

#define START_OF_KERNEL 0x400000
#define END_OF_USER 0x8400000

/*
 * Tries getting the kernel to read a string which extends
 * past the end of the user page
 *
 * ...evil|XXXXXX
 *        ^
 *  end of user page 
 */
int open_invalid_string(void)
{
    int result = 0;

    /*
     * Back up original value to avoid corrupting stack
     * This is just to make sure it's not a bunch of \0s
     */
    uint32_t *addr = (uint32_t *)(END_OF_USER - 4);
    uint32_t orig = *addr;
    *addr = 0x6c697665;
    int fd = ece391_open((uint8_t *)addr);
    if (fd >= 0) {
        result = 1;
    }
    ece391_close(fd);
    *addr = orig;
    return result;
}

/*
 * Tries executing a string which extends past the end
 * of the user page, and some really long strings
 */
int execute_invalid_string(void)
{
    int result = 0;

    /* Try executing an invalid string */
    uint32_t *addr = (uint32_t *)(END_OF_USER - 4);
    uint32_t orig = *addr;
    *addr = 0x6c697665;
    if (ece391_execute((uint8_t *)addr) >= 0) {
        result = 1;
    }
    *addr = orig;

    /* Try executing a really long string */
    uint8_t buf[8192];
    int i;
    for (i = 0; i < 8191; ++i) {
        buf[i] = 'A';
    }
    buf[8191] = '\0';
    if (ece391_execute((uint8_t *)buf) >= 0) {
        result = 1;
    }

    /* Try really long args */
    buf[10] = ' ';
    if (ece391_execute((uint8_t *)buf) >= 0) {
        result = 1;
    }

    /* Try a really short string */
    buf[0] = '\0';
    if (ece391_execute((uint8_t *)buf) >= 0) {
        result = 1;
    }

    return result;
}

/*
 * Tries writing to a buffer which extends past the
 * end of the user page (and various variants of it)
 *
 * addr      addr + size
 *  v             v
 *  [      |      ]
 *         ^
 *   end of user page
 */
int read_invalid_buffer(void)
{
    int result = 0;
    uint8_t *addr = (uint8_t *)(END_OF_USER - 4);
    int fd = ece391_open((uint8_t *)"shell");

    /* Checks upper bound at all? */
    if (ece391_read(fd, addr, 8) >= 0) {
        result = 1;
    }

    /* Integer overflow? */
    if (ece391_read(fd, addr, 0xffffffff) >= 0) {
        result = 1;
    }

    /* Signed integer overflow? */
    if (ece391_read(fd, addr, 0x7fffffff) >= 0) {
        result = 1;
    }

    /* Extra bound check evasion */
    if (ece391_read(fd, (uint8_t *)0xffff0000, 0x7fffffff) >= 0) {
        result = 1;
    }

    /*
     * I guess it's debatable whether -1 or 0 should
     * be returned, but a positive value is definitely wrong.
     */
    if (ece391_read(fd, addr, 0) > 0) {
        result = 1;
    }

    ece391_close(fd);
    return result;
}

/*
 * Same as read_invalid_buffer, but with write.
 */
int write_invalid_buffer(void)
{
    int result = 0;
    uint8_t *addr = (uint8_t *)(END_OF_USER - 4);

    /* Checks upper bound at all? */
    if (ece391_write(1, addr, 8) >= 0) {
        result = 1;
    }

    /* Integer overflow? */
    if (ece391_write(1, addr, 0xffffffff) >= 0) {
        result = 1;
    }

    /* Signed integer overflow? */
    if (ece391_write(1, addr, 0x7fffffff) >= 0) {
        result = 1;
    }

    /* Extra bound check evasion */
    if (ece391_write(1, (uint8_t *)0xffff0000, 0x7fffffff) >= 0) {
        result = 1;
    }

    /*
     * Same as read, accept 0 or -1.
     */
    if (ece391_write(1, addr, 0) > 0) {
        result = 1;
    }

    return result;
}

/*
 * Tries corrupting the kernel page by pointing the
 * read output buffer to kernel memory.
 */
int read_kernel_buffer(void)
{
    int result = 0;
    int i;
    for (i = 0; i < 1024; ++i) {
        int fd = ece391_open((uint8_t *)"shell");
        if (ece391_read(fd, (uint8_t *)(START_OF_KERNEL + i * 4096), 4096) >= 0) {
            result = 1;
        }
        ece391_close(fd);
    }
    return result;
}

/*
 * Tries reading a buffer slightly larger than the
 * filesystem block size, with size not a multiple of 4.
 */
int read_large_buffer(void)
{
    uint8_t buf[4097];
    int result = 0;
    int fd = ece391_open((uint8_t *)"fish");
    int count;
    do {
        count = ece391_read(fd, buf, sizeof(buf));
        if (count < 0) {
            result = 1;
        }
    } while (count > 0);
    ece391_close(fd);
    return result;
}

/*
 * Similar to read_invalid_buffer, but with vidmap.
 */
int vidmap_invalid_buffer(void)
{
    int result = 0;
    uint8_t **addr = (uint8_t **)(END_OF_USER - 2);
    if (ece391_vidmap(addr) >= 0) {
        result = 1;
    }
    return result;
}

/*
 * Similar to read_kernel_buffer, but with vidmap.
 */
int vidmap_kernel_buffer(void)
{
    int result = 0;
    int i;
    for (i = 0; i < 1024 * 1024; ++i) {
        if (ece391_vidmap((uint8_t **)(START_OF_KERNEL + i * 4)) >= 0) {
            result = 1;
        }
    }
    return result;
}

/*
 * Attempts to divide by zero. This should cause the
 * program to be aborted, and should not panic the kernel.
 */
int divide_by_zero(void)
{
    volatile int x = 0;
    x = 1 / x;
    return 0;
}

/*
 * Sets the data segment register to a garbage value
 * and tries to invoke a syscall. If not handled properly, this
 * will cause the kernel to crash.
 *
 * Apparently QEMU is buggy and just flat out ignores the
 * null segment descriptor. So this test doesn't really work.
 */
int set_garbage_ds(void)
{
    asm volatile(
        /* Write garbage DS */
        "movw $0x03, %%ax;"
        "movw %%ax, %%ds;"

        /* Let the kernel do some work */
        "movl $6, %%eax;"
        "int $0x80;"

        /* Restore old DS */
        "movw $0x2B, %%ax;"
        "movw %%ax, %%ds;"
        :
        :
        : "eax");
    return 0;
}

int main(void)
{
    int result =
        read_kernel_buffer() +
        vidmap_kernel_buffer() +
        open_invalid_string() +
        execute_invalid_string() +
        read_invalid_buffer() +
        write_invalid_buffer() +
        vidmap_invalid_buffer() +
        read_large_buffer();

    if (result == 0) {
        ece391_fdputs(1, (uint8_t *)"All tests PASSED!\n");
        return 0;
    } else {
        ece391_fdputs(1, (uint8_t *)"One or more tests FAILED!\n");
        return 1;
    }
}
