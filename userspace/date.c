#include <stdio.h>
#include <syscall.h>

int
main(void)
{
    time_t now;
    realtime(&now);
    printf("%d\n", (int)now);
    return 0;
}
