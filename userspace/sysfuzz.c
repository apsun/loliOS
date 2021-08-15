#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define MAX_FILES 8

static uint32_t
globalrand(int fd)
{
    /*
     * Randomness source needs to be shared globally, or else we end up
     * repeating the same tests over and over.
     */
    uint32_t ret;
    int n = read(fd, &ret, sizeof(ret));
    if (n != sizeof(ret)) {
        /* We probably closed the random file, no point continuing */
        exit(1);
    }
    return ret;
}

static uint32_t
randfd(int fd)
{
    return globalrand(fd) % MAX_FILES;
}

static uint32_t
randx(int fd)
{
    if (globalrand(fd) & 1) {
        return randfd(fd);
    } else {
        return globalrand(fd);
    }
}

static void
fuzz(int fd)
{
    while (1) {
        int eax = (globalrand(fd) % NUM_SYSCALL) + 1;
        uint32_t ebx = randx(fd);
        uint32_t ecx = randx(fd);
        uint32_t edx = randx(fd);
        uint32_t esi = randx(fd);
        uint32_t edi = randx(fd);

        /* Don't let child process kill the parent */
        if (eax == SYS_KILL) {
            continue;
        }

        /* These takes no args and make fuzzing slower */
        if (eax == SYS_HALT || eax == SYS_FORK) {
            continue;
        }

        /* These screw with the terminal */
        if (eax == SYS_TCSETPGRP || eax == SYS_SETPGRP) {
            continue;
        }

        /* Limit duration of monosleep() */
        if (eax == SYS_MONOSLEEP && ebx > 100) {
            continue;
        }

        /* read(stdin) wastes a lot of time */
        if (eax == SYS_READ && ebx == STDIN_FILENO) {
            continue;
        }

        asm volatile(
            "int $0x80;"
            :
            : "a"(eax), "b"(ebx), "c"(ecx), "d"(edx), "S"(esi), "D"(edi));
    }
}

int
main(void)
{
    int randfd = open("random");
    if (randfd < 0) {
        fprintf(stderr, "Failed to get randomness\n");
        return 1;
    }

    int iter = 0;
    while (1) {
        printf("%d\n", iter++);
        int pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork\n");
            return 1;
        } else if (pid > 0) {
            /* Wait up to 3 seconds before killing the fuzzer */
            monosleep(monotime() + 3000);
            kill(pid, SIG_KILL);
            wait(&pid);
        } else {
            fuzz(randfd);
        }
    }
}
