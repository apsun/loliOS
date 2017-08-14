#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

void
puts(const char *s)
{
    ece391_fdputs(1, (uint8_t *)s);
}

int
main(void)
{
    int32_t fd = ece391_open((uint8_t *)"taux");
    if (fd < 0) {
        puts("Could not open taux file\n");
        return 1;
    }

    int32_t ret = ece391_ioctl(fd, 0xdeadface, 0xf00ba);
    ece391_close(fd);
    return 0;
}
