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

    /* Print file name */
    printf("\nfile_name: ");
    terminal_write(0, dentry->fname, FNAME_LEN);
    printf("\n");
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
        printf("file_name: ");
        terminal_write(0, dentry.fname, FNAME_LEN);

        /* Print file type */
        printf("  file_type: %d", dentry.ftype);

        /* Print file size */
        printf("  file_size: %d\n", filesys_get_fsize(&dentry));

        /* Next file */
        i++;
    }
}

static void
test_read_file_by_name(const char *name)
{
    dentry_t dentry;
    int32_t res;
    uint8_t fname[33] = {0};
    int32_t count;

    /* Clear screen */
    clear();

    while (1) {
        /* Prompt for file name */
        printf("Enter file name: ");
        count = terminal_read(0, fname, 32);

        /* Chop off newline if we didn't read full 32 chars */
        if (fname[count - 1] == '\n') {
            fname[count - 1] = '\0';
        }

        /* Stop when no filename entered */
        if (fname[0] == '\0') {
            break;
        }

        /* Read the dentry */
        res = read_dentry_by_name(fname, &dentry);

        /* Whoops, can't read that */
        if (res < 0) {
            printf("File not found: %s\n", fname);
            continue;
        }

        /* Echo file to terminal */
        test_print_file(&dentry);
    }
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
    static int32_t fd = -1;
    static int32_t freq = 2;

    /* If this is >= 0, we're already executing the test */
    if (fd >= 0) {
        /* Double the frequency */
        freq <<= 1;
        int32_t res = rtc_write(fd, &freq, sizeof(int32_t));

        /* Frequency is too damn high! Let's go back to 2Hz */
        if (res < 0) {
            freq = 1;
            test_rtc_start();
            return;
        }

        clear();
        return;
    }

    /* Sets fd to 0, frequency to 2Hz */
    fd = rtc_open((uint8_t *)">implying I have a filename for this");

    /* Clear screen */
    clear();

    /* Initialization finished, interrupts are OK now */
    sti();

    while (!stop_rtc_test) {
        /* Wait for clock cycle */
        rtc_read(fd, NULL, 0);

        /* Write to terminal */
        terminal_write(0, "1", 1);
    }

    /* Better not interrupt during cleanup */
    cli();

    /* Clear again on exit */
    clear();

    /* Everyone, get out of here! */
    stop_rtc_test = 0;
    rtc_close(fd);
    fd = -1;
    freq = 2;
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
