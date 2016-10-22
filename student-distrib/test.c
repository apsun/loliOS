#include "test.h"
#include "filesys.h"
#include "debug.h"
#include "terminal.h"
#include "rtc.h"

volatile int32_t stop_rtc_test = 0;

static void
test_print_file(dentry_t *dentry)
{
    uint8_t buf[1234]; /* Purposely not the size of a block */
    int32_t bytes_read;
    int32_t total_bytes_read = 0;

    /* Read some bytes from the file, echo them to terminal */
    do {
        bytes_read = read_data(dentry->inode_idx, total_bytes_read, buf, sizeof(buf));
        total_bytes_read += bytes_read;
        terminal_write(0, buf, bytes_read);
    } while (bytes_read > 0);
}

static void
test_list_all_files(void)
{
    dentry_t dentry;
    int32_t res = 0;
    uint32_t i = 0;

    /* Clear screen first */
    clear();

    while (1) {
        /* Read the dentry */
        res = read_dentry_by_index(i, &dentry);

        /* Stop when no more files */
        if (res < 0) {
            break;
        }

        /* Print file name */
        terminal_write(0, dentry.fname, FNAME_LEN);
        terminal_write(0, " ", 1);

        /* Print file type */
        printf("%d", dentry.ftype);
        terminal_write(0, "\n", 1);

        /* TODO: Print file size */

        /* Next file */
        i++;
    }
}

static void
test_read_file_by_name(const char *name)
{
    dentry_t dentry;
    int32_t res;

    /* Read the dentry */
    res = read_dentry_by_name((const uint8_t *)name, &dentry);

    /* Whoops, can't read that */
    if (res < 0) {
        return;
    }

    /* Clear screen */
    clear();

    /* Echo file to terminal */
    test_print_file(&dentry);
}

static void
test_read_file_by_index(void)
{
    static int32_t index = 0;
    dentry_t dentry;
    int32_t res;

    /* Read the dentry */
    res = read_dentry_by_index(index++, &dentry);

    /* Whoops, no more files, let's start over */
    if (res < 0) {
        index = 0;
        test_read_file_by_index();
        return;
    }

    /* Clear screen */
    clear();

    /* Echo file to terminal */
    test_print_file(&dentry);
}

static void
test_rtc_start(void)
{
    /* If this is >= 0, we're already executing the test */
    static int32_t fd = -1;
    static int32_t freq = 2;

    if (fd >= 0) {
        /* Double the frequency */
        int32_t res = rtc_write(fd, &freq, sizeof(int32_t));
        freq <<= 1;

        /* Frequency is too damn high! Let's go back to 2Hz */
        if (res < 0) {
            freq = 2;
            test_rtc_start();
            return;
        }

        clear();
        return;
    }

    /* Sets fd to 0, frequency to 2Hz */
    fd = rtc_open((uint8_t *)">implying I have a filename for this");

    /* Initialization finished, interrupts are OK now */
    sti();

    /* Clear screen */
    clear();

    while (!stop_rtc_test) {
        /* Wait for clock cycle */
        rtc_read(fd, NULL, 0);

        /* Write to terminal */
        terminal_write(0, "1", 1);
    }

    /* Clear again on exit */
    clear();

    /* Better not interrupt during cleanup */
    cli();

    /* Everyone, get out of here! */
    stop_rtc_test = 0;
    rtc_close(fd);
    fd = -1;
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
    case 0:
        test_list_all_files();
        break;
    case 1:
        test_read_file_by_name("frame0.txt");
        break;
    case 2:
        test_read_file_by_index();
        break;
    case 3:
        test_rtc_start();
        break;
    case 4:
        test_rtc_stop();
        break;
    }
}
