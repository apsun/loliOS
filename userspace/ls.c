#include <stddef.h>
#include <stdio.h>
#include <syscall.h>

static const char *
get_file_type(int type)
{
    switch (type) {
    case FILE_TYPE_RTC: return "rtc";
    case FILE_TYPE_DIR: return "dir";
    case FILE_TYPE_FILE: return "file";
    case FILE_TYPE_MOUSE: return "mouse";
    case FILE_TYPE_TAUX: return "taux";
    case FILE_TYPE_SOUND: return "sound";
    case FILE_TYPE_TTY: return "tty";
    case FILE_TYPE_NULL: return "null";
    case FILE_TYPE_ZERO: return "zero";
    case FILE_TYPE_RANDOM: return "random";
    default: return "unknown";
    }
}

int
main(void)
{
    int ret = 1;
    int fd = -1;
    char fname[33];

    /* Open the directory */
    if ((fd = create(".", OPEN_READ)) < 0) {
        fprintf(stderr, "Cannot open directory for reading\n");
        goto cleanup;
    }

    /* Read dir entries, print to stdout */
    int cnt;
    while ((cnt = read(fd, fname, sizeof(fname))) != 0) {
        if (cnt < 0) {
            fprintf(stderr, "Cannot read directory entry\n");
            goto cleanup;
        }
        fname[cnt] = '\0';

        stat_t st;
        if (stat(fname, &st) >= 0) {
            printf("%-32s %-8s %d\n", fname, get_file_type(st.type), st.length);
        } else {
            printf("%-32s (stat failed)\n", fname);
        }
    }

    ret = 0;

cleanup:
    if (fd >= 0) close(fd);
    return ret;
}
