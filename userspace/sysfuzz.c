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
    /* These may cause the fuzzer to die or freeze */
    case SYS_HALT:
    case SYS_EXEC:
    case SYS_KILL:
    case SYS_EXECUTE:

    /* These screw with the terminal or take no args */
    case SYS_FORK:
    case SYS_YIELD:
    case SYS_TCSETPGRP:
    case SYS_TCGETPGRP:
    case SYS_SETPGRP:
    case SYS_GETPGRP:
    case SYS_GETPID:
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

static uint32_t
randfd(void)
{
    return rand() % MAX_FILES;
}

static uint32_t
randx(void)
{
    if (rand() & 1) {
        return randfd();
    } else {
        return rand32();
    }
}

static void
close_all_files(void)
{
    int fd;
    for (fd = 0; fd < MAX_FILES; ++fd) {
        close(fd);
    }
}

static void
fuzz(void)
{
    srand(time());
    while (1) {
        int eax = (rand() % NUM_SYSCALL) + 1;
        if (is_blacklisted(eax)) {
            continue;
        }

        uint32_t ebx = randx();
        uint32_t ecx = randx();
        uint32_t edx = randx();
        uint32_t esi = randx();
        uint32_t edi = randx();
        asm volatile(
            "int $0x80;"
            :
            : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx), "S"(esi), "D"(edi));

        /* Periodically close files so we don't run out of descriptors */
        if (rand() % 100 == 0) {
            close_all_files();
        }
    }
}

int
main(void)
{
    while (1) {
        int pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork\n");
            return 1;
        } else if (pid > 0) {
            wait(&pid);
        } else {
            fuzz();
        }
    }
}
