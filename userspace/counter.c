#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

int32_t
main(void)
{
    puts("Enter the Test Number: (0): 100, (1): 10000, (2): 100000");

    char buf[1024];
    if (gets(buf, sizeof(buf)) == NULL) {
        puts("Can't read the number from keyboard.");
        return 3;
    }

    int32_t max;
    if (strcmp(buf, "0") == 0) {
        max = 100;
    } else if (strcmp(buf, "1") == 0) {
        max = 10000;
    } else if (strcmp(buf, "2") == 0) {
        max = 100000;
    } else {
        puts("Wrong Choice!");
        return 0;
    }

    int32_t i;
    for (i = 0; i < max; ++i) {
        printf("%d\n", i);
    }

    return 0;
}
