#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>

static int
execute_command(const char *cmd)
{
    int orig_tcpgrp = tcgetpgrp();
    int pid = fork();
    if (pid < 0) {
        printf("fork() failed\n");
        return -1;
    } else if (pid == 0) {
        setpgrp(0, 0);
        tcsetpgrp(0);
        if (exec(cmd) < 0) {
            printf("exec() failed\n");
            exit(1);
        }
        return -1;
    } else {
        setpgrp(pid, pid);
        int exit_code = wait(&pid);
        if (exit_code < 0) {
            printf("wait() failed\n");
            return -1;
        }
        tcsetpgrp(orig_tcpgrp);
        return exit_code;
    }
}

static int
read_line(char *buf, int buf_size)
{
    static char internal_buf[256];
    static int internal_total = 0;

    /* How much space do we have left? */
    int remaining = sizeof(internal_buf) - internal_total;
    if (remaining == 0) {
        return -1;
    }

    /* Read more characters from stdin */
    int ret = read(0, &internal_buf[internal_total], remaining);
    if (ret <= 0) {
        return ret;
    }
    internal_total += ret;

    /* Look for a newline */
    int lf;
    for (lf = 0; lf < internal_total; ++lf) {
        if (internal_buf[lf] == '\n') {
            break;
        }
    }

    /* Check that we have a newline and buffer can hold entire line */
    if (lf == internal_total) {
        return -EAGAIN;
    } else if (lf >= buf_size) {
        return -1;
    }

    /* Copy up to newline (which becomes a NUL) */
    internal_buf[lf] = '\0';
    strcpy(buf, internal_buf);

    /* Shift remaining chars over */
    int consumed = lf + 1;
    memmove(&internal_buf[0], &internal_buf[consumed], internal_total - consumed);
    internal_total -= consumed;
    return consumed;
}

int
main(void)
{
    char command[128];
    while (1) {
        printf("mash> ");

        /* Wait for a new line */
        while (1) {
            int ret = read_line(command, sizeof(command));
            if (ret == -EAGAIN || ret == -EINTR) {
                continue;
            } else if (ret < 0) {
                printf("read() failed\n");
                return 1;
            } else if (ret == 0) {
                return 0;
            } else {
                break;
            }
        }

        if (command[0] == '\0') {
            continue;
        } else if (strcmp(command, "exit") == 0) {
            return 0;
        }

        int exit_code = execute_command(command);
        if (exit_code < 0) {
            return exit_code;
        } else if (exit_code > 0) {
            printf("program terminated abnormally, ret = %d\n", exit_code);
        }
    }
}
