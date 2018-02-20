#include <stddef.h>
#include <stdio.h>
#include <syscall.h>

int
main(void)
{
    char args[1024];
    if (getargs(args, sizeof(args)) != 0) {
        puts("could not read arguments");
        return 1;
    }

    puts(args);
    return 0;
}
