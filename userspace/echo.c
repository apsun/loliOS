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

    puts(args);
    return 0;
}
