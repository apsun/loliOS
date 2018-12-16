#include <stddef.h>
#include <stdio.h>
#include <syscall.h>

int
main(void)
{
    char args[128];
    if (getargs(args, sizeof(args)) < 0) {
        args[0] = '\0';
    }

    if (unlink(args) < 0) {
        fprintf(stderr, "Failed to delete %s\n", args);
        return 1;
    }

    return 0;
}
