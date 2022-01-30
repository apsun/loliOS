#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

/* Must keep this in sync with kernel code */
#define PIPE_CAPACITY 8192

static void
test_pipe_rdwr(void)
{
    int ret;

    int readfd, writefd;
    ret = pipe(&readfd, &writefd);
    assert(ret >= 0);

    ret = write(writefd, "foo", 3);
    assert(ret == 3);

    pollfd_t pfds[2];
    pfds[0].fd = readfd;
    pfds[0].events = OPEN_RDWR;
    pfds[1].fd = writefd;
    pfds[1].events = OPEN_RDWR;
    ret = poll(pfds, 2);
    assert(ret == 2);
    assert(pfds[0].revents == OPEN_READ);
    assert(pfds[1].revents == OPEN_WRITE);

    close(readfd);
    close(writefd);
}

static void
test_pipe_full_empty(void)
{
    int ret;

    int readfd, writefd;
    ret = pipe(&readfd, &writefd);
    assert(ret >= 0);

    pollfd_t pfds[2];
    pfds[0].fd = readfd;
    pfds[0].events = OPEN_RDWR;
    pfds[1].fd = writefd;
    pfds[1].events = OPEN_RDWR;
    ret = poll(pfds, 2);
    assert(ret == 1);
    assert(pfds[0].revents == 0);
    assert(pfds[1].revents == OPEN_WRITE);

    char buf[PIPE_CAPACITY];
    memset(buf, 0x42, sizeof(buf));
    ret = write(writefd, buf, sizeof(buf));
    assert(ret == sizeof(buf));

    ret = poll(pfds, 2);
    assert(ret == 1);
    assert(pfds[0].revents == OPEN_READ);
    assert(pfds[1].revents == 0);

    close(readfd);
    close(writefd);
}

static void
test_invalid_fd(void)
{
    int ret;

    pollfd_t pfds[1];
    pfds[0].fd = 1337;
    pfds[0].events = OPEN_RDWR;
    ret = poll(pfds, 1);
    assert(ret < 0);
}

static void
test_unimplemented(void)
{
    int ret;

    int fd = open("rtc");

    pollfd_t pfds[1];
    pfds[0].fd = fd;
    pfds[0].events = OPEN_RDWR;
    ret = poll(pfds, 1);
    assert(ret < 0);

    close(fd);
}

static void
test_unknown_bits(void)
{
    int ret;

    pollfd_t pfds[1];
    pfds[0].fd = 0;
    pfds[0].events = 9999;
    ret = poll(pfds, 1);
    assert(ret < 0);
}

static void
test_permissions(void)
{
    int ret;

    int fd = create("TEMP_FILE", OPEN_RDWR | OPEN_CREATE);
    assert(fd >= 0);

    ret = write(fd, "foo", 3);
    assert(ret == 3);

    int fd2 = create("TEMP_FILE", OPEN_READ);
    assert(fd2 >= 0);

    pollfd_t pfds[1];
    pfds[0].fd = fd2;
    pfds[0].events = OPEN_RDWR;
    ret = poll(pfds, 1);
    assert(ret == 1);
    assert(pfds[0].revents == OPEN_READ);

    close(fd2);
    close(fd);
    unlink("TEMP_FILE");
}

int
main(void)
{
    test_pipe_rdwr();
    test_pipe_full_empty();
    test_invalid_fd();
    test_unimplemented();
    test_unknown_bits();
    test_permissions();
    printf("All tests passed!\n");
    return 0;
}
