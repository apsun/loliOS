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
    /* Don't let child process kill the parent */
    case SYS_KILL:

    /* These takes no args and make fuzzing slower */
    case SYS_HALT:
    case SYS_FORK:

    /* These screw with the terminal */
    case SYS_TCSETPGRP:
    case SYS_SETPGRP:
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
fuzz(void)
{
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
    }
}

int
main(void)
{
    time_t seed;
    realtime(&seed);
    srand((unsigned int)seed);

    int iter = 0;
    while (1) {
        /* Progress the shared randomness state */
        rand();

        printf("%d\n", iter++);
        int pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork\n");
            return 1;
        } else if (pid > 0) {
            /* Wait up to 3 seconds before killing the fuzzer */
            nanotime_t now;
            monotime(&now);
            nanotime_t target = now + SECONDS(3);
            monosleep(&target);
            kill(pid, SIG_KILL);
            wait(&pid);
        } else {
            fuzz();
        }
    }
}
