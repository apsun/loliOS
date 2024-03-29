#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

/* Must keep this in sync with kernel code */
#define PIPE_CAPACITY 8192

/* How long to wait for async operations */
#define TIMEOUT_MS 50

static int
bind2(int fd, sock_addr_t *addr)
{
    int ret = bind(fd, addr);
    if (ret == 0) {
        ret = getsockname(fd, addr);
    }
    return ret;
}

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
    ret = poll(pfds, 2, -1);
    assert(ret == 2);
    assert(pfds[0].revents == OPEN_READ);
    assert(pfds[1].revents == OPEN_WRITE);

    close(readfd);
    close(writefd);
}

static void
test_pipe_duplicate_fd(void)
{
    int ret;

    int readfd, writefd;
    ret = pipe(&readfd, &writefd);
    assert(ret >= 0);

    ret = write(writefd, "foo", 3);
    assert(ret == 3);

    pollfd_t pfds[3];
    pfds[0].fd = readfd;
    pfds[0].events = OPEN_RDWR;
    pfds[1].fd = readfd;
    pfds[1].events = OPEN_RDWR;
    pfds[2].fd = readfd;
    pfds[2].events = OPEN_RDWR;
    ret = poll(pfds, 3, -1);
    assert(ret == 3);
    assert(pfds[0].revents == OPEN_READ);
    assert(pfds[1].revents == OPEN_READ);
    assert(pfds[2].revents == OPEN_READ);

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
    ret = poll(pfds, 2, -1);
    assert(ret == 1);
    assert(pfds[0].revents == 0);
    assert(pfds[1].revents == OPEN_WRITE);

    char buf[PIPE_CAPACITY];
    memset(buf, 0x42, sizeof(buf));
    ret = write(writefd, buf, sizeof(buf));
    assert(ret == sizeof(buf));

    ret = poll(pfds, 2, -1);
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
    ret = poll(pfds, 1, -1);
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
    ret = poll(pfds, 1, -1);
    assert(ret < 0);

    close(fd);
}

static void
test_unknown_bits(void)
{
    int ret;

    pollfd_t pfds[1];
    pfds[0].fd = 0;
    pfds[0].events = 0xff;
    ret = poll(pfds, 1, -1);
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
    ret = poll(pfds, 1, -1);
    assert(ret == 1);
    assert(pfds[0].revents == OPEN_READ);

    close(fd2);
    close(fd);
    unlink("TEMP_FILE");
}

static void
test_timeout(void)
{
    int ret;

    int readfd, writefd;
    ret = pipe(&readfd, &writefd);
    assert(ret >= 0);

    pollfd_t pfds[1];
    pfds[0].fd = readfd;
    pfds[0].events = OPEN_RDWR;
    ret = poll(pfds, 1, monotime() + TIMEOUT_MS);
    assert(ret == 0);
    assert(pfds[0].revents == 0);

    close(readfd);
    close(writefd);
}

static void
test_pipe_fork(void)
{
    int ret;

    int readfd, writefd;
    ret = pipe(&readfd, &writefd);
    assert(ret >= 0);

    fcntl(readfd, FCNTL_NONBLOCK, 1);
    fcntl(writefd, FCNTL_NONBLOCK, 1);

    int pid = fork();
    if (pid == 0) {
        sleep(monotime() + TIMEOUT_MS);
        ret = write(writefd, "foo", 3);
        assert(ret == 3);

        exit(0);
    }
    assert(pid > 0);

    char buf[3];
    ret = read(readfd, buf, sizeof(buf));
    assert(ret == -EAGAIN);

    pollfd_t pfds[1];
    pfds[0].fd = readfd;
    pfds[0].events = OPEN_RDWR;
    ret = poll(pfds, 1, -1);
    assert(ret == 1);
    assert(pfds[0].revents == OPEN_READ);

    ret = read(readfd, buf, sizeof(buf));
    assert(ret == 3);

    ret = wait(&pid);
    assert(ret == 0);

    close(readfd);
    close(writefd);
}

static void
test_tcp_fork(void)
{
    int ret;

    /* Create listening socket */
    int a = socket(SOCK_TCP);
    assert(a >= 0);
    fcntl(a, FCNTL_NONBLOCK, 1);
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = listen(a, 64);
    assert(ret == 0);

    int pid = fork();
    if (pid == 0) {
        /* Connect to listening socket */
        sleep(monotime() + TIMEOUT_MS);
        int b = socket(SOCK_TCP);
        assert(b >= 0);
        fcntl(b, FCNTL_NONBLOCK, 1);
        sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
        ret = bind2(b, &b_addr);
        assert(ret == 0);
        ret = connect(b, &a_addr);
        assert(ret == 0);

        /* Send some data */
        sleep(monotime() + TIMEOUT_MS);
        ret = write(b, "foo", 3);
        assert(ret == 3);

        exit(0);
    }
    assert(pid > 0);

    /* Poll for incoming connection */
    pollfd_t pfds[1];
    pfds[0].fd = a;
    pfds[0].events = OPEN_RDWR;
    ret = poll(pfds, 1, -1);
    assert(ret == 1);
    assert(pfds[0].revents == OPEN_READ);

    /* Accept incoming connection */
    sock_addr_t tmp_addr;
    int a_conn = accept(a, &tmp_addr);
    assert(a_conn >= 0);

    /* Poll for data */
    pfds[0].fd = a_conn;
    pfds[0].events = OPEN_READ;
    ret = poll(pfds, 1, -1);
    assert(ret == 1);
    assert(pfds[0].revents == OPEN_READ);

    /* Read the data */
    char buf[3];
    ret = read(a_conn, buf, sizeof(buf));
    assert(ret == 3);
    assert(memcmp(buf, "foo", 3) == 0);

    ret = wait(&pid);
    assert(ret == 0);

    close(a_conn);
    close(a);
}

static void
test_udp_fork(void)
{
    int ret;

    /* Create read socket */
    int a = socket(SOCK_UDP);
    assert(a >= 0);
    fcntl(a, FCNTL_NONBLOCK, 1);
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);

    int pid = fork();
    if (pid == 0) {
        /* Connect to socket */
        sleep(monotime() + TIMEOUT_MS);
        int b = socket(SOCK_UDP);
        assert(b >= 0);
        fcntl(b, FCNTL_NONBLOCK, 1);
        sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
        ret = bind2(b, &b_addr);
        assert(ret == 0);

        /* Send some data */
        sleep(monotime() + TIMEOUT_MS);
        ret = sendto(b, "foo", 3, &a_addr);
        assert(ret == 3);

        exit(0);
    }
    assert(pid > 0);

    /* Poll for data */
    pollfd_t pfds[1];
    pfds[0].fd = a;
    pfds[0].events = OPEN_READ;
    ret = poll(pfds, 1, -1);
    assert(ret == 1);
    assert(pfds[0].revents == OPEN_READ);

    /* Read the data */
    char buf[3];
    ret = read(a, buf, sizeof(buf));
    assert(ret == 3);
    assert(memcmp(buf, "foo", 3) == 0);

    ret = wait(&pid);
    assert(ret == 0);

    close(a);
}

int
main(void)
{
    test_pipe_rdwr();
    test_pipe_duplicate_fd();
    test_pipe_full_empty();
    test_invalid_fd();
    test_unimplemented();
    test_unknown_bits();
    test_permissions();
    test_timeout();
    test_pipe_fork();
    test_tcp_fork();
    test_udp_fork();
    printf("All tests passed!\n");
    return 0;
}
