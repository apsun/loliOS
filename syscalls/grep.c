#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define BUFSIZE 1024

int32_t
do_one_file(const char *s, const char *fname)
{
    int32_t ret = 0;
    int32_t fd = -1;
    int32_t s_len = strlen(s);
    char data[BUFSIZE + 1];

    if ((fd = open(fname)) < 0) {
        puts("file open failed");
        ret = -1;
        goto exit;
    }

    int32_t last = 0;
    int32_t cnt;
    do {
        if ((cnt = read(fd, &data[last], BUFSIZE - last)) < 0) {
            puts("file read filed");
            ret = -1;
            goto exit;
        }

        last += cnt;
        int32_t line_start = 0;
        while (1) {
            int32_t line_end = line_start;
            while (line_end < last && data[line_end] != '\n') {
                line_end++;
            }

            if (data[line_end] != '\n' && cnt > 0 && line_start != 0) {
                data[line_end] = '\0';
                strcpy(data, &data[line_start]);
                last -= line_start;
                break;
            }

            data[line_end] = '\0';
            int32_t check;
            for (check = line_start; check < line_end; check++) {
                if (strncmp(&data[check], s, s_len) == 0) {
                    printf("%s:%s\n", fname, &data[line_start]);
                    break;
                }
            }

            line_start = line_end + 1;
            if (line_start >= last) {
                last = 0;
                break;
            }
        }
    } while (cnt > 0);

exit:
    if (fd >= 0) close(fd);
    return ret;
}

int32_t
main(void)
{
    int32_t ret = 0;
    int32_t fd = -1;

    char args[BUFSIZE];
    if (getargs(args, sizeof(args)) < 0) {
        puts("could not read argument");
        ret = 3;
        goto exit;
    }

    if ((fd = open(".")) < 0) {
        puts("directory open failed");
        ret = 2;
        goto exit;
    }

    char fname[33];
    int32_t cnt;
    while ((cnt = read(fd, fname, sizeof(fname) - 1)) != 0) {
        if (cnt < 0) {
            puts("directory entry read failed");
            ret = 3;
            goto exit;
        }
        fname[cnt] = '\0';

        if (fname[0] == '.') {
            continue;
        }

        if (do_one_file(args, fname) != 0) {
            ret = 3;
            goto exit;
        }
    }

exit:
    if (fd >= 0) close(fd);
    return ret;
}
