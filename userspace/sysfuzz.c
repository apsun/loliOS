#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define MAX_FILES 8

static bool
is_blacklisted(int num)
{
    switch (num) {
    /* These may cause the fuzzer to die */
    case SYS_HALT:
    case SYS_FORK:
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_KILL:

    /* These screw with the terminal or take no args */
    case SYS_TCSETPGRP:
    case SYS_TCGETPGRP:
    case SYS_SETPGRP:
    case SYS_GETPGRP:
    case SYS_GETPID:
    case SYS_YIELD:
        return true;
    default:
        return false;
    }
}

static uint32_t
rand32(void)
{
    uint32_t ret = 0;
    ret |= (rand() & 0xff) << 0;
    ret |= (rand() & 0xff) << 8;
    ret |= (rand() & 0xff) << 16;
    ret |= (rand() & 0xff) << 24;
    return ret;
}

static void
close_all_files(void)
{
    int fd;
    for (fd = 0; fd < MAX_FILES; ++fd) {
        close(fd);
    }
}

int
main(void)
{
    srand(time());

    while (1) {
        int eax = (rand() % NUM_SYSCALL) + 1;
        if (is_blacklisted(eax)) {
            continue;
        }

        uint32_t ebx = rand32();
        uint32_t ecx = rand32();
        uint32_t edx = rand32();
        uint32_t esi = rand32();
        uint32_t edi = rand32();
        asm volatile(
            "int $0x80;"
            :
            : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx), "S"(esi), "D"(edi));

        /* Periodically close files so we don't run out of descriptors */
        if (rand() % 10 == 0) {
            close_all_files();
        }
    }
}
