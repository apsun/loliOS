#include <stdio.h>
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

    int now = monotime();
    int target = now + delay * 1000;

    while (1) {
        int r = sleep(target);
        if (r == 0) {
            break;
        } else if (r < 0 && r != -EINTR) {
            fprintf(stderr, "sleep() returned %d\n", r);
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    return ret;
}
