#include "test.h"
#include "filesys.h"
#include "debug.h"
#include "terminal.h"
#include "rtc.h"

static volatile int32_t rtc_fd = -1;
static int32_t rtc_freq = 2;
static volatile int32_t stop_rtc_test = 0;
static int32_t next_findex = 0;

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
test_read_file_by_name(const char *fname)
{
    dentry_t dentry;
    int32_t res;

    /* Read the dentry */
    res = read_dentry_by_name((const uint8_t *)fname, &dentry);

    /* Whoops, can't read that */
    if (res < 0) {
        printf("File not found: %s\n", fname);
        return;
    }

    /* Echo file to terminal */
    test_print_file(&dentry);
}

static int32_t
test_read_file_by_index(int32_t index)
{
    dentry_t dentry;
    int32_t res;

    /* Read the dentry */
    res = read_dentry_by_index(index, &dentry);

    /* Whoops, invalid index */
    if (res < 0) {
        printf("Invalid file index: %d\n", index);
        return 0;
    }

    /* Echo file to terminal */
    test_print_file(&dentry);
    return 1;
}

static void
test_rtc_start(void)
{
    /* Maybe someone hit CTRL-5 before we started */
    stop_rtc_test = 0;

    /* Sets fd to 0, frequency to 2Hz */
    rtc_fd = rtc_open((uint8_t *)"rtc");

    /* Clear screen */
    clear();

    /* Initialization finished, interrupts are OK now */
    sti();

    while (!stop_rtc_test) {
        /* Wait for clock cycle */
        rtc_read(rtc_fd, NULL, 0);

        /* Write to terminal */
        terminal_write(0, "1", 1);
    }

    /* Better not interrupt during cleanup */
    cli();

    /* Clear again on exit */
    clear();

    /* Everyone, get out of here! */
    stop_rtc_test = 0;
    rtc_close(rtc_fd);
    rtc_fd = -1;
    rtc_freq = 2;
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
    res = rtc_write(rtc_fd, &rtc_freq, sizeof(int32_t));

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
    case 0:
        clear();
        test_list_all_files();
        break;
    case 1:
        clear();
        test_read_file_by_name("frame0.txt");
        break;
    case 2:
        clear();
        if (!test_read_file_by_index(next_findex++)) {
            /* Start over */
            clear();
            next_findex = 0;
            test_read_file_by_index(next_findex++);
        }
        break;
    case 3:
        if (rtc_fd < 0) {
            test_rtc_start();
        } else {
            test_rtc_faster();
        }
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
    int32_t index;

    while (1) {
        terminal_write(0, "loliOS> ", 8);

        /* Read command */
        count = terminal_read(0, cmd_buf, 128);

        /* Chop off newline if we didn't read full 32 chars */
        if (cmd_buf[count - 1] == '\n') {
            cmd_buf[count - 1] = '\0';
        }

        if (strcmp("ls", (int8_t *)cmd_buf) == 0) {
            test_list_all_files();
        } else if (strncmp("cat ", (int8_t *)cmd_buf, 4) == 0) {
            test_read_file_by_name((int8_t *)cmd_buf + 4);
        } else if (strncmp("cati ", (int8_t *)cmd_buf, 5) == 0 &&
                   atoi_s((int8_t *)cmd_buf + 5, &index)) {
            test_read_file_by_index(index);
        } else if (strcmp("rtc", (int8_t *)cmd_buf) == 0) {
            test_rtc_start();
        } else {
            printf("Unknown command!\n"
                   "Commands:\n"
                   "  ls           - list all files\n"
                   "  cat  <fname> - print contents of file by name\n"
                   "  cati <index> - print contents of file by index\n"
                   "  rtc          - begin rtc test (CTRL-4 increases speed, CTRL-5 stops)\n");
        }
    }
}
