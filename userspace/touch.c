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

    int fd = create(args, OPEN_WRITE | OPEN_CREATE);
    if (fd < 0) {
        fprintf(stderr, "Failed to create %s\n", args);
        return 1;
    }

    close(fd);
    return 0;
}
