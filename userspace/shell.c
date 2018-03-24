#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

int
main(void)
{
    puts("Starting 391 Shell");

    while (1) {
        printf("391OS> ");

        /* Read command */
        char buf[1024];
        if (gets(buf, sizeof(buf)) == NULL) {
            puts("read from keyboard failed");
            return 3;
        }

        /* No action on empty input */
        if (buf[0] == '\0') {
            continue;
        }

        /* Handle exit command */
        if (strcmp(buf, "exit") == 0) {
            return 0;
        }

        /* Execute command */
        int ret = execute(buf);
        if (ret < 0) {
            puts("no such command");
        } else if (ret == 256) {
            puts("program terminated by exception");
        } else if (ret != 0) {
            puts("program terminated abnormally");
        }
    }
}
