#include <types.h>
#include <sys.h>
#include <io.h>
#include <string.h>

#define SCREENWIDTH 79
#define ENDING 2
#define BUFMAX (SCREENWIDTH + ENDING)
#define START 1
#define STARTLOOP (START + 1)
#define LOOPMAX (BUFMAX - ENDING - 1)
#define STARTCHAR 'A'
#define ENDCHAR 'Z'

int32_t
main(void)
{
    int32_t ret = 0;
    int32_t rtc_fd = -1;
    char buf[BUFMAX];

    int32_t i;
    for (i = 0; i < BUFMAX; i++) {
        buf[i] = ' ';
    }

    buf[BUFMAX - 1] = '\0';
    buf[BUFMAX - 2] = '\n';
    buf[BUFMAX - 3] = '|';
    buf[START] = '|';

    if ((rtc_fd = open("rtc")) < 0) {
        puts("could not open rtc file");
        ret = 2;
        goto exit;
    }

    int32_t rtc_freq = 32;
    if (write(rtc_fd, &rtc_freq, sizeof(rtc_freq)) < 0) {
        puts("could not set rtc frequency");
        ret = 3;
        goto exit;
    }

    char curchar = STARTCHAR;
    while (1) {
        int32_t j;
        for (j = STARTLOOP; j < LOOPMAX; j++) {
            for(i = STARTLOOP; i < LOOPMAX; i++) {
                buf[i]=' ';
            }

            buf[j] = curchar;
            printf("%s", buf);

            int32_t garbage;
            read(rtc_fd, &garbage, sizeof(garbage));
        }

        for (j = LOOPMAX - 1; j >= STARTLOOP; j--) {
            for(i = STARTLOOP; i < LOOPMAX; i++) {
                buf[i]=' ';
            }

            buf[j] = curchar;
            printf("%s", buf);

            int32_t garbage;
            read(rtc_fd, &garbage, sizeof(garbage));
        }

        if (curchar == ENDCHAR) {
            curchar = STARTCHAR;
        } else {
            curchar++;
        }
    }

exit:
    if (rtc_fd >= 0) close(rtc_fd);
    return ret;
}
