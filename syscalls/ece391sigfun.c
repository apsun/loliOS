#include <stdint.h>
#include "ece391support.h"
#include "ece391syscall.h"

#define MAX_TICKS 4

static volatile int exit_requested = 0;
static volatile int elapsed_ticks = 0;
static const char *spinner_chars = "|/-\\";

void sig_alarm_handler(void)
{
    elapsed_ticks++;
}

void sig_interrupt_handler(void)
{
    exit_requested = 1;
}

void sig_interrupt_exit_handler(void)
{
    ece391_fdputs(1, (uint8_t*)"Bye!\n");
    ece391_halt(0);
}

int main(void)
{
    /* Register signal handlers */
    ece391_set_handler(ALARM, (void *)sig_alarm_handler);
    ece391_set_handler(INTERRUPT, (void *)sig_interrupt_handler);

    /* Spinner buffer */
    char spinner_buf[2];
    spinner_buf[1] = '\0';

    /* Progress bar buffer */
    char progbar_buf[MAX_TICKS + 2 + 1];
    progbar_buf[0] = '[';
    progbar_buf[sizeof(progbar_buf) - 2] = ']';
    progbar_buf[sizeof(progbar_buf) - 1] = '\0';
    int i;
    for (i = 0; i < MAX_TICKS; ++i) {
        progbar_buf[i + 1] = ' ';
    }

    /* Open and initialize RTC */
    int rtc_fd = ece391_open((uint8_t *)"rtc");
    int freq = 8;
    ece391_write(rtc_fd, &freq, sizeof(freq));

    /* Update progress loop */
    ece391_fdputs(1, (uint8_t*)"Loading, please wait...\n");
    int rtc_ticks = 0;
    int alarm_ticks;
    while ((alarm_ticks = elapsed_ticks) < MAX_TICKS) {
        if (exit_requested) {
            ece391_fdputs(1, (uint8_t *)"\nAborted!\n");
            return 0;
        }

        /* Update buffers */
        spinner_buf[0] = spinner_chars[rtc_ticks & 3];
        progbar_buf[alarm_ticks + 1] = '=';

        /* Draw to screen */
        ece391_fdputs(1, (uint8_t *)"\rProgress: ");
        ece391_fdputs(1, (uint8_t *)progbar_buf);
        ece391_fdputs(1, (uint8_t *)" ");
        ece391_fdputs(1, (uint8_t *)spinner_buf);

        /* Wait for RTC tick */
        int ignored;
        ece391_read(rtc_fd, &ignored, sizeof(ignored));
        rtc_ticks++;
    }

    /* Replace interrupt handler with halt code */
    ece391_fdputs(1, (uint8_t *)"\nLoading complete!\n");
    ece391_fdputs(1, (uint8_t *)"Press CTRL-C to exit.\n");
    ece391_set_handler(INTERRUPT, (void *)sig_interrupt_exit_handler);
    while (1);

    /* Should never reach this... */
    return 1;
}
