#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <syscall.h>

/* Must keep this in sync with kernel code */
#define PIPE_CAPACITY 8192

void
test_invalid_args(void)
{
    int ret;
    int readfd, writefd;
    ret = pipe(&readfd, NULL);
    assert(ret == -1);
    ret = pipe(NULL, &writefd);
    assert(ret == -1);
    ret = pipe(NULL, NULL);
    assert(ret == -1);
}

void
test_circular_queue(void)
{
    int ret;
    int readfd, writefd;

    ret = pipe(&readfd, &writefd);
    assert(ret == 0);

    char buf[PIPE_CAPACITY + 1];
    int i;
    for (i = 0; i < (int)sizeof(buf); ++i) {
        buf[i] = i;
    }

    ret = write(writefd, buf, PIPE_CAPACITY / 2);
    assert(ret == PIPE_CAPACITY / 2);

    char tmp[PIPE_CAPACITY + 1];
    ret = read(readfd, tmp, sizeof(tmp));
    assert(ret == PIPE_CAPACITY / 2);
    assert(memcmp(buf, tmp, ret) == 0);

    ret = write(writefd, buf, sizeof(buf));
    assert(ret == PIPE_CAPACITY);

    ret = read(readfd, tmp, sizeof(tmp));
    assert(ret == PIPE_CAPACITY);
    assert(memcmp(buf, tmp, ret) == 0);

    close(readfd);
    close(writefd);
}

void
test_half_duplex_write(void)
{
    int ret;
    int readfd, writefd;
    int x = 42;

    ret = pipe(&readfd, &writefd);
    assert(ret == 0);

    close(readfd);
    ret = write(writefd, &x, sizeof(x));
    assert(ret == -1);

    close(writefd);
}

void
test_half_duplex_read(void)
{
    int ret;
    int readfd, writefd;
    int x;

    ret = pipe(&readfd, &writefd);
    assert(ret == 0);

    close(writefd);
    ret = read(readfd, &x, sizeof(x));
    assert(ret == 0);

    close(readfd);
}

void
test_permissions(void)
{
    int ret;
    int readfd, writefd;
    int x = 42;

    ret = pipe(&readfd, &writefd);
    assert(ret == 0);

    ret = write(readfd, &x, sizeof(x));
    assert(ret == -1);

    ret = read(writefd, &x, sizeof(x));
    assert(ret == -1);

    close(readfd);
    close(writefd);
}

int
main(void)
{
    test_invalid_args();
    test_circular_queue();
    test_half_duplex_write();
    test_half_duplex_read();
    test_permissions();
    printf("All tests passed!\n");
    return 0;
}
