#include <stddef.h>
#include <stdio.h>
#include <syscall.h>

#define BUFSIZE 1024

int
main(void)
{
    printf("Hi, what's your name? ");

    char buf[1024];
    if (gets(buf, sizeof(buf)) == NULL) {
        puts("Can't read name from keyboard.");
        return 3;
    }

    printf("Hello, %s\n", buf);
    return 0;
}