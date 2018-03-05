#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

#define TUX_SET_LED_STR 0x16

int
main(void)
{
    int fd = open("taux");
    while (1) {
        printf("tauxprint> ");

        char buf[128];
        if (gets(buf, sizeof(buf)) == NULL) {
            break;
        }

        if (ioctl(fd, TUX_SET_LED_STR, (int)buf) < 0) {
            puts("Cannot display that string!");
        }
    }
    close(fd);
    return 0;
}
