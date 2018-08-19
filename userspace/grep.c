#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

/*
 * Ugly hack: avoid reading these files since they can
 * block indefinitely waiting for data to arrive. In the
 * future we can add a stat syscall to determine the file
 * type for a given file descriptor which is what GNU grep
 * does, but for now we'll just take the lazy way out.
 */
static const char *special[] = {
    ".",
    "rtc",
    "mouse",
    "taux",
    "sound",
    "tty",
    "null",
};

static bool
is_regular_file(const char *fname)
{
    int i;
    for (i = 0; i < (int)(sizeof(special) / sizeof(special[0])); ++i) {
        if (strcmp(fname, special[i]) == 0) {
            return false;
        }
    }
    return true;
}

static int
grep_file(const char *needle, const char *fname, int fd)
{
    FILE *fp = (fd >= 0) ? fdopen(fd) : fopen(fname, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s: could not open file\n", fname);
        if (fd >= 0) close(fd);
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        /*
         * Chop off \n if it exists. We can't just preserve
         * it since in binary mode fgets will stop on \0 too,
         * and then we have no idea whether there's a newline
         * or not.
         */
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (strstr(line, needle) != NULL) {
            if (fname != NULL) {
                printf("%s:%s\n", fname, line);
            } else {
                printf("%s\n", line);
            }
        }
    }

    fclose(fp);
    return 0;
}

static int
grep_all(const char *needle)
{
    int ret = 1;
    int fd = create(".", OPEN_READ);
    if (fd < 0) {
        fprintf(stderr, "Could not open directory for reading\n");
        goto cleanup;
    }

    char fname[33];
    int cnt;
    while ((cnt = read(fd, fname, sizeof(fname) - 1)) != 0) {
        if (cnt < 0) {
            fprintf(stderr, "Failed to read file name\n");
            goto cleanup;
        }

        fname[cnt] = '\0';
        if (is_regular_file(fname)) {
            if (grep_file(needle, fname, -1) < 0) {
                goto cleanup;
            }
        }
    }

    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}

static int
grep_one(const char *needle, const char *fname)
{
    int fd;
    if (strcmp(fname, "-") == 0) {
        fd = STDIN_FILENO;
    } else {
        fd = create(fname, OPEN_READ);
        if (fd < 0) {
            fprintf(stderr, "%s: Failed to open file for reading\n", fname);
            return 1;
        }
    }

    /* grep_file() will close fd for us */
    if (grep_file(needle, NULL, fd) < 0) {
        return 1;
    }

    return 0;
}

int
main(void)
{
    /* Get the arguments */
    char args[128];
    if (getargs(args, sizeof(args)) < 0) {
        fprintf(stderr, "usage: grep <needle> [file|-|.]\n");
        return 1;
    }

    /*
     * First argument is what to search for, second
     * is the file to search (if none specified,
     * defaults to stdin).
     */
    char *space = strchr(args, ' ');
    char *fname;
    if (space == NULL) {
        fname = "-";
    } else {
        *space = '\0';
        fname = space + 1;
    }

    if (strcmp(fname, ".") == 0) {
        return grep_all(args);
    } else {
        return grep_one(args, fname);
    }
}
