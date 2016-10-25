#include "test.h"
#include "file.h"
#include "lib.h"

static volatile int32_t rtc_fd = -1;
static int32_t rtc_freq = 2;
static volatile int32_t stop_rtc_test = 0;

static void
test_print_file(int32_t fd)
{
    uint8_t buf[1234]; /* Purposely not the size of a block */
    int32_t bytes_read;

    /* Read some bytes from the file, echo them to terminal */
    do {
        bytes_read = file_read(fd, buf, sizeof(buf));
        file_write(FD_STDOUT, buf, bytes_read);
    } while (bytes_read > 0);

    file_write(FD_STDOUT, "\n", 1);
}

static void
test_list_all_files(void)
{
    int32_t count;
    uint8_t buf[33] = {0};
    int32_t fd;

    /* Open directory */
    fd = file_open((uint8_t *)".");
    if (fd < 0) {
        printf("Could not open directory\n");
        return;
    }

    while (1) {
        /* Read next file name */
        count = file_read(fd, buf, 32);
        if (count == 0) {
            break;
        }

        /* Print file name */
        file_write(FD_STDOUT, buf, count);
        file_write(FD_STDOUT, "\n", 1);
    }

    /* Close directory */
    file_close(fd);
}

static void
test_read_file_by_name(const char *fname)
{
    int32_t fd;

    /* Open file */
    fd = file_open((const uint8_t *)fname);
    if (fd < 0) {
        printf("File not found: %s\n", fname);
        return;
    }

    /* Echo file to terminal */
    test_print_file(fd);

    /* Close file */
    file_close(fd);
}

static void
test_rtc_start(void)
{
    /* Maybe someone hit CTRL-5 before we started */
    stop_rtc_test = 0;

    /* Sets fd to 0, frequency to 2Hz */
    rtc_fd = file_open((uint8_t *)"rtc");

    /* Clear screen */
    clear();

    /* Initialization finished, interrupts are OK now */
    sti();

    while (!stop_rtc_test) {
        /* Wait for clock cycle */
        file_read(rtc_fd, NULL, 0);

        /* Write to terminal */
        file_write(FD_STDOUT, "1", 1);
    }

    /* Better not interrupt during cleanup */
    cli();

    /* Clear again on exit */
    clear();

    /* Everyone, get out of here! */
    stop_rtc_test = 0;
    file_close(rtc_fd);
    rtc_fd = -1;
    rtc_freq = 2;

    /* Re-enable interrupts */
    sti();
}

static void
test_rtc_faster(void)
{
    int32_t res;

    /* Not running test */
    if (rtc_fd < 0) {
        return;
    }

    /* Double the frequency */
    rtc_freq <<= 1;
    res = file_write(rtc_fd, &rtc_freq, sizeof(int32_t));

    /* Frequency is too damn high! Let's go back to 2Hz */
    if (res < 0) {
        rtc_freq = 1;
        test_rtc_faster();
        return;
    }

    /* Clear screen */
    clear();
}

static void
test_rtc_stop(void)
{
    stop_rtc_test = 1;
}

void
test_execute(int32_t test_num)
{
    switch (test_num) {
    case 3:
        test_rtc_faster();
        break;
    case 4:
        test_rtc_stop();
        break;
    }
}

void
test_shell(void)
{
    uint8_t cmd_buf[129];
    int32_t count;

    while (1) {
        /* Write prompt */
        file_write(FD_STDOUT, "loliOS> ", 8);

        /* Read command */
        count = file_read(FD_STDIN, cmd_buf, 128);

        /* Chop off newline if we didn't read full 32 chars */
        if (cmd_buf[count - 1] == '\n') {
            cmd_buf[count - 1] = '\0';
        }

        if (strcmp("ls", (int8_t *)cmd_buf) == 0) {
            test_list_all_files();
        } else if (strncmp("cat ", (int8_t *)cmd_buf, 4) == 0) {
            test_read_file_by_name((int8_t *)cmd_buf + 4);
        } else if (strcmp("rtc", (int8_t *)cmd_buf) == 0) {
            test_rtc_start();
        } else {
            printf("Unknown command!\n"
                   "Commands:\n"
                   "  ls          - list all files\n"
                   "  cat <fname> - print contents of file by name\n"
                   "  rtc         - begin rtc test (CTRL-4 increases speed, CTRL-5 stops)\n");
        }
    }
}
