#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

#define TUX_SET_LED_STR 0x16

int
main(void)
{
    int fd = create("taux", OPEN_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open taux file\n");
        return 1;
    }

    while (1) {
        fprintf(stderr, "tauxprint> ");

        char buf[129];
        if (gets(buf, sizeof(buf)) == NULL) {
            break;
        }

        if (ioctl(fd, TUX_SET_LED_STR, (int)buf) < 0) {
            fprintf(stderr, "Cannot display that string!\n");
        }
    }

    close(fd);
    return 0;
}
