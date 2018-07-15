#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>
#include <assert.h>

typedef struct cmd {
    struct cmd *next; /* Next command in the pipeline */
    char *name;       /* Program name to run */
    char *in;         /* Name of file to redirect stdin from */
    char *out;        /* Name of file to redirect stdout to */
} cmd_t;

typedef struct proc {
    struct proc *next;
    struct cmd *cmd;
    int pid;
    int exit_code;
} proc_t;

static char *
strtrim(char *s)
{
    for (; isspace(*s); ++s);
    char *e = s;
    for (; *e; ++e);
    while (e > s && isspace(*--e)) {
        *e = '\0';
    }
    return s;
}

static char *
strdup(const char *s)
{
    int len = strlen(s) + 1;
    char *d = malloc(len);
    if (d != NULL) {
        memcpy(d, s, len);
    }
    return d;
}

static char *
pop_redirect(char *s)
{
    char *fname = s + 1 + strspn(s + 1, " ");
    char *end = strchr(fname, ' ');
    if (end != NULL) {
        *end++ = '\0';
    }
    char *word = strdup(fname);
    if (end != NULL) {
        strcpy(s, end);
    } else {
        *s = '\0';
    }
    return word;
}

static void
free_cmd(cmd_t *cmd)
{
    while (cmd != NULL) {
        cmd_t *next = cmd->next;
        free(cmd->in);
        free(cmd->out);
        free(cmd->name);
        free(cmd);
        cmd = next;
    }
}

static cmd_t *
parse_input(char *line)
{
    /*
     * Really lazy command parsing: split string on all pipe
     * characters. I sure hope we won't have to support ||
     * some day.
     */
    cmd_t *cmd = NULL;
    cmd_t **pcmd = &cmd;
    char *cmd_s;
    while ((cmd_s = strsep(&line, "|")) != NULL) {
        cmd_t *curr = malloc(sizeof(cmd_t));
        if (curr == NULL) {
            fprintf(stderr, "Out of memory, cannot allocate command info\n");
            goto cleanup;
        }

        *pcmd = curr;
        pcmd = &curr->next;
        curr->next = NULL;
        curr->name = NULL;
        curr->in = NULL;
        curr->out = NULL;

        char *lt = strchr(cmd_s, '<');
        if (lt != NULL) {
            curr->in = pop_redirect(lt);
            if (curr->in == NULL) {
                fprintf(stderr, "Out of memory, cannot allocate command\n");
                goto cleanup;
            }
        }

        char *gt = strchr(cmd_s, '>');
        if (gt != NULL) {
            curr->out = pop_redirect(gt);
            if (curr->out == NULL) {
                fprintf(stderr, "Out of memory, cannot allocate command\n");
                goto cleanup;
            }
        }

        curr->name = strdup(strtrim(cmd_s));
        if (curr->name == NULL) {
            fprintf(stderr, "Out of memory, cannot allocate command\n");
            goto cleanup;
        }
    }

    return cmd;

cleanup:
    free_cmd(cmd);
    return NULL;
}

static int
execute_command(cmd_t *cmd)
{
    proc_t *root = NULL;
    proc_t **pproc = &root;
    int group_id = 0;
    char error[256];
    int orig_tcpgrp = tcgetpgrp();

    int next_in = -1;
    int curr_in = -1;
    int curr_out = -1;

    error[0] = '\0';
/*
 * Since a child may have already stolen the foreground terminal
 * from us, delay any error messages until the end, after all of
 * the children are dead and we can reclaim the terminal.
 */
#define FAIL(...) do { \
    snprintf(error, sizeof(error), __VA_ARGS__); \
    goto cleanup; \
} while (0)

    for (; cmd != NULL; cmd = cmd->next) {
        proc_t *curr = malloc(sizeof(proc_t));
        if (curr == NULL) {
            FAIL("Out of memory, cannot allocate process info\n");
        }

        bool is_root = (root == NULL);

        /* Immediately initialize so we can properly clean up in case of error */
        *pproc = curr;
        pproc = &curr->next;
        curr->next = NULL;
        curr->cmd = cmd;
        curr->pid = -1;
        curr->exit_code = -1;

        /*
         * Check for "first | second < in" case. This is
         * never a valid combination.
         */
        if (cmd->in != NULL && !is_root) {
            FAIL("Cannot redirect stdin in middle of pipe\n");
        }

        /*
         * Check for "first > out | second" case. Note that normally
         * 2>&1 would be a valid construct, but we don't allow redirecting
         * stderr, so let's just avoid redirection conflicts altogether.
         */
        if (cmd->out != NULL && cmd->next != NULL) {
            FAIL("Cannot redirect stdout in middle of pipe\n");
        }

        /* Create a pipe for the next process in line */
        if (cmd->next != NULL) {
            if (pipe(&curr_in, &curr_out) < 0) {
                FAIL("Failed to create pipe\n");
            }
        }

        /* Okay, now let's fork! */
        int pid = fork();
        if (pid < 0) {
            FAIL("Reached max number of processes\n");
        } else if (pid == 0) {
            /* Don't need this one */
            close(curr_in);

            /* Set process group and grab terminal foreground */
            setpgrp(0, group_id);
            tcsetpgrp(group_id);

            /*
             * Handle redirected streams. We do this in the child instead
             * of the parent so we don't have to deal with closing the files
             * used by other child processes in each child.
             */
            if (cmd->in != NULL) {
                assert(next_in < 0);
                next_in = create(cmd->in, OPEN_READ);
                if (next_in < 0) {
                    fprintf(stderr, "Failed to open '%s' for reading\n", cmd->in);
                    halt(127);
                }
            }
            if (next_in >= 0) {
                dup(next_in, STDIN_FILENO);
                close(next_in);
            }

            /* Do the same, this time for stdout */
            if (cmd->out != NULL) {
                assert(curr_out < 0);
                curr_out = create(cmd->out, OPEN_WRITE);
                if (curr_out < 0) {
                    fprintf(stderr, "Failed to open '%s' for writing\n", cmd->out);
                    halt(127);
                }
            }
            if (curr_out >= 0) {
                dup(curr_out, STDOUT_FILENO);
                close(curr_out);
            }

            /* And finally, execute the command! */
            exec(cmd->name);
            fprintf(stderr, "%s: command not found\n", cmd->name);
            halt(127);
        } else {
            curr->pid = pid;

            /* Use the PID of the root process as the group ID */
            if (group_id == 0) {
                group_id = pid;
            }

            /*
             * Set group on behalf of child to handle the
             * race condition where the parent first waits
             * on the group before the child has executed yet.
             */
            setpgrp(pid, group_id);

            /*
             * Close the pipe output from the last iteration, and
             * out current output becomes the output for the next
             * iteration.
             */
            if (next_in >= 0) {
                close(next_in);
            }
            next_in = curr_in;
            curr_in = -1;

            /* Close the pipe input from the current iteration */
            if (curr_out >= 0) {
                close(curr_out);
                curr_out = -1;
            }
        }
    }

cleanup:

    /* Close all open pipe endpoints */
    if (next_in >= 0) {
        close(next_in);
        next_in = -1;
    }
    if (curr_in >= 0) {
        close(curr_in);
        curr_in = -1;
    }
    if (curr_out >= 0) {
        close(curr_out);
        curr_out = -1;
    }

    /*
     * Wait for all children to exit. As on Linux, final
     * exit code is the code of the last process in the pipe.
     * If nothing managed to execute, return 255 (OOM).
     */
    proc_t *curr;
    for (curr = root; curr != NULL; curr = curr->next) {
        if (curr->pid >= 0) {
            curr->exit_code = wait(&curr->pid);
        }
    }

    /* Restore the foreground terminal, so we can print stuff again */
    tcsetpgrp(orig_tcpgrp);

    /* Clean up process state and print exit codes */
    int exit_code = 255;
    while (root != NULL) {
        proc_t *next = root->next;
        if (root->pid >= 0) {
            exit_code = root->exit_code;
            if (exit_code != 0 && exit_code != 127) {
                fprintf(stderr, "%s finished with exit code %d\n", root->cmd->name, exit_code);
            }
        }

        free(root);
        root = next;
    }

    /* Print any delayed error messages */
    if (error[0] != '\0') {
        fprintf(stderr, "%s", error);
    }

    return exit_code;
}

int
main(void)
{
    char buf[128];
    while (1) {
        fprintf(stderr, "mash> ");

        if (gets(buf, sizeof(buf)) == NULL) {
            return 0;
        }

        char *line = strtrim(buf);
        if (line[0] == '\0') {
            continue;
        } else if (strcmp(line, "exit") == 0) {
            return 0;
        }

        cmd_t *cmd = parse_input(line);
        if (cmd == NULL) {
            fprintf(stderr, "Failed to parse command\n");
            return 1;
        }

        int exit_code = execute_command(cmd);
        free_cmd(cmd);
        if (exit_code < 0) {
            fprintf(stderr, "Fatal error %d, exiting\n", exit_code);
            return exit_code;
        }
    }
}
