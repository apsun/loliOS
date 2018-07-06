#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>
#include <assert.h>

static int
execute_command(const char *cmd)
{
    int orig_tcpgrp = tcgetpgrp();
    int pid = fork();
    if (pid < 0) {
        return 255;
    } else if (pid == 0) {
        setpgrp(0, 0);
        tcsetpgrp(0);
        exec(cmd);
        exit(127);
        return -1;
    } else {
        setpgrp(pid, pid);
        int exit_code = wait(&pid);
        tcsetpgrp(orig_tcpgrp);
        return exit_code;
    }
}

int
main(void)
{
    char command[128];
    while (1) {
        printf("mash> ");

        if (gets(command, sizeof(command)) == NULL) {
            puts("gets() failed");
            return 1;
        }

        if (command[0] == '\0') {
            continue;
        } else if (strcmp(command, "exit") == 0) {
            return 0;
        }

        int exit_code = execute_command(command);
        if (exit_code < 0) {
            return exit_code;
        } else if (exit_code == 127) {
            printf("%s: command not found\n", command);
        } else if (exit_code == 255) {
            printf("Reached max number of processes\n");
        } else if (exit_code > 0) {
            printf("%s: program terminated with exit code %d\n", command, exit_code);
        }
    }
}
