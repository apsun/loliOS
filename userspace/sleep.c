#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

int
main(void)
{
    int ret = 1;
    int fd = -1;
    
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
    
    fd = create("rtc", OPEN_READ | OPEN_WRITE);
    if (fd < 0) {
        fprintf(stderr, "Failed to open RTC file\n");
        goto cleanup;
    }

    int freq = 2;
    if (write(fd, &freq, sizeof(freq)) != sizeof(freq)) {
        fprintf(stderr, "Failed to set RTC frequency\n");
        goto cleanup;
    }

    int i;
    for (i = 0; i < delay * freq; ++i) {
        read(fd, NULL, 0);
    }

    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}
