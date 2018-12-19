#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

int
main(void)
{
    int ret = 1;
    
    char args[128];
    if (getargs(args, sizeof(args)) < 0) {
        fprintf(stderr, "usage: sleep <secs>\n");
        goto cleanup;
    }

    int delay = atoi(args);
    if (delay == 0) {
        fprintf(stderr, "Invalid delay: %s\n", args);
        goto cleanup;
    }
    
    int ms = delay * 1000;
    while (ms > 0) {
        ms = sleep(ms);
    }

    ret = 0;

cleanup:
    return ret;
}
