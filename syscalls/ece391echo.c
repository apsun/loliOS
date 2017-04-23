#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

int main(void)
{
    uint8_t args[4096];
    if (ece391_getargs(args, sizeof(args)) != 0) {
        ece391_fdputs(1, (uint8_t *)"Could not read args\n");
        return 1;
    }

    ece391_fdputs(1, args);
    ece391_fdputs(1, (uint8_t *)"\n");
    return 0;
}
