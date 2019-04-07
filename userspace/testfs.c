#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <syscall.h>

static int
mktemp(int flags)
{
    int fd = create("TEST_FILE", OPEN_CREATE | OPEN_RDWR | flags);
    assert(fd >= 0);
    int ret = unlink("TEST_FILE");
    assert(ret >= 0);
    return fd;
}

static void
test_seek(void)
{
    int fd = mktemp(0);
    int ret;
    char buf[128];

    ret = write(fd, "foobar", 6);
    assert(ret == 6);

    ret = seek(fd, 3, SEEK_SET);
    assert(ret == 3);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 3);
    assert(memcmp(buf, "bar", ret) == 0);

    ret = seek(fd, -5, SEEK_CUR);
    assert(ret == 1);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 5);
    assert(memcmp(buf, "oobar", ret) == 0);

    close(fd);
}

static void
test_truncate_shrink(void)
{
    int fd = mktemp(0);
    int ret;
    char buf[128];

    ret = write(fd, "foobar", 6);
    assert(ret == 6);

    ret = truncate(fd, 3);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 0);

    ret = seek(fd, 0, SEEK_SET);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 3);
    assert(memcmp(buf, "foo", ret) == 0);

    ret = truncate(fd, 0);
    assert(ret == 0);

    ret = seek(fd, 0, SEEK_SET);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 0);

    close(fd);
}

static void
test_truncate_grow(void)
{
    int fd = mktemp(0);
    int ret;
    char buf[128];

    ret = write(fd, "foobar", 6);
    assert(ret == 6);

    ret = truncate(fd, 10);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 4);
    assert(memcmp(buf, "\0\0\0\0", ret) == 0);

    ret = truncate(fd, 14);
    assert(ret == 0);

    ret = write(fd, "x", 1);
    assert(ret == 1);

    ret = seek(fd, -1, SEEK_CUR);
    assert(ret == 10);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 4);
    assert(memcmp(buf, "x\0\0\0", ret) == 0);

    close(fd);
}

static void
test_partial_write(void)
{
    int fd = mktemp(0);
    int ret;

    ret = write(fd, (uint8_t *)0x8400000 - 0x1000, 0x2000);
    assert(ret == 0x1000);

    ret = seek(fd, 0, SEEK_CUR);
    assert(ret == 0x1000);

    ret = seek(fd, 0, SEEK_END);
    assert(ret == 0x1000);

    close(fd);
}

static void
test_failed_write(void)
{
    int fd = mktemp(0);
    int ret;
    char buf[128];

    ret = write(fd, (void *)0xffffffff, 1000);
    assert(ret < 0);

    ret = seek(fd, 0, SEEK_CUR);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 0);

    ret = seek(fd, 10, SEEK_SET);
    assert(ret == 10);

    ret = write(fd, (void *)0xffffffff, 1000);
    assert(ret < 0);

    ret = seek(fd, 0, SEEK_CUR);
    assert(ret == 10);

    ret = seek(fd, 0, SEEK_SET);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 0);
}

static void
test_write_gap(void)
{
    int fd = mktemp(0);
    int ret;
    char buf[128];

    ret = seek(fd, 2, SEEK_END);
    assert(ret == 2);

    ret = write(fd, "foo", 3);
    assert(ret == 3);

    ret = seek(fd, 0, SEEK_SET);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 5);
    assert(memcmp(buf, "\0\0foo", ret) == 0);

    close(fd);
}

static void
test_write_fill_block(void)
{
    int fd = mktemp(0);
    int ret;
    char buf[4096] = {'a'};

    ret = write(fd, buf, sizeof(buf) - 1);
    assert(ret == sizeof(buf) - 1);

    ret = write(fd, "b", 1);
    assert(ret == 1);

    ret = seek(fd, 0, SEEK_SET);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == sizeof(buf));
    assert(buf[sizeof(buf) - 1] == 'b');
}

static void
test_write_large_file(void)
{
    int fd = mktemp(0);
    int ret;
    char buf[8192] = {'a'};

    ret = write(fd, buf, sizeof(buf));
    assert(ret == sizeof(buf));

    ret = seek(fd, 0, SEEK_CUR);
    assert(ret == 8192);

    close(fd);
}

static void
test_open_trunc(void)
{
    int fd;
    int ret;
    char buf[128];

    fd = create("foo", OPEN_CREATE | OPEN_RDWR);
    assert(fd >= 0);

    ret = write(fd, "foobar", 6);
    assert(ret == 6);

    close(fd);

    fd = create("foo", OPEN_RDWR | OPEN_TRUNC);
    assert(fd >= 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 0);

    close(fd);

    ret = unlink("foo");
    assert(ret == 0);
}

static void
test_open_append(void)
{
    int fd, fd2;
    int ret;
    char buf[128];

    fd = create("foo", OPEN_CREATE | OPEN_RDWR);
    assert(fd >= 0);

    ret = write(fd, "foo", 3);
    assert(ret == 3);

    fd2 = create("foo", OPEN_RDWR | OPEN_APPEND);
    assert(fd2 >= 0);

    ret = write(fd2, "bar", 3);
    assert(ret == 3);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 3);
    assert(memcmp(buf, "bar", ret) == 0);

    ret = write(fd, "baz", 3);
    assert(ret == 3);

    ret = write(fd2, "qux", 3);
    assert(ret == 3);

    ret = seek(fd, -3, SEEK_CUR);
    assert(ret == 6);
    
    ret = read(fd, buf, sizeof(buf));
    assert(ret == 6);
    assert(memcmp(buf, "bazqux", ret) == 0);

    close(fd2);
    close(fd);

    ret = unlink("foo");
    assert(ret == 0);
}

static void
test_unlink_lazy_delete(void)
{
    int fd;
    int ret;
    char buf[128];

    fd = create("foo", OPEN_CREATE | OPEN_RDWR);
    assert(fd >= 0);

    ret = unlink("foo");
    assert(ret == 0);

    ret = write(fd, "abc", 3);
    assert(ret == 3);

    ret = seek(fd, 0, SEEK_SET);
    assert(ret == 0);

    ret = read(fd, buf, sizeof(buf));
    assert(ret == 3);
    assert(memcmp(buf, "abc", ret) == 0);

    close(fd);

    fd = create("foo", OPEN_RDWR);
    assert(fd < 0);
}

static void
test_stdio_file(void)
{
    FILE *f;
    int ret;
    char buf[128];

    f = fopen("TEST_FILE", "w+");
    assert(f != NULL);

    ret = fwrite(f, "foobar", 6);
    assert(ret == 6);

    ret = fread(f, buf, sizeof(buf));
    assert(ret == 0);

    ret = fseek(f, 0, SEEK_SET);
    assert(ret == 0);

    ret = fread(f, buf, 1);
    assert(ret == 1);
    assert(buf[0] == 'f');

    ret = fseek(f, 3, SEEK_SET);
    assert(ret == 3);

    ret = fread(f, buf, 1);
    assert(ret == 1);
    assert(buf[0] == 'b');

    ret = fwrite(f, "x", 1);
    assert(ret == 1);

    ret = fread(f, buf, 1);
    assert(ret == 1);
    assert(buf[0] == 'r');

    ret = fseek(f, 0, SEEK_SET);
    assert(ret == 0);

    ret = fread(f, buf, sizeof(buf));
    assert(ret == 6);
    assert(memcmp(buf, "foobxr", ret) == 0);

    fclose(f);

    ret = unlink("TEST_FILE");
    assert(ret == 0);
}

static void
test_stdio_file_append(void)
{
    FILE *f;
    int ret;
    char buf[128];

    f = fopen("TEST_FILE", "a+");
    assert(f != NULL);

    ret = fwrite(f, "foobar", 6);
    assert(ret == 6);

    ret = fseek(f, 0, SEEK_SET);
    assert(ret == 0);

    ret = fread(f, buf, 1);
    assert(ret == 1);
    assert(buf[0] == 'f');

    ret = fwrite(f, "baz", 3);
    assert(ret == 3);

    ret = fread(f, buf, sizeof(buf));
    assert(ret == 0);

    ret = fseek(f, 0, SEEK_SET);
    assert(ret == 0);

    ret = fread(f, buf, sizeof(buf));
    assert(ret == 9);
    assert(memcmp(buf, "foobarbaz", ret) == 0);

    fclose(f);

    ret = unlink("TEST_FILE");
    assert(ret == 0);
}

static void
test_stdio_fseek_relative(void)
{
    FILE *f;
    int ret;
    char buf[128];

    f = fopen("TEST_FILE", "w+");
    assert(f != NULL);

    ret = fwrite(f, "foobar", 6);
    assert(ret == 6);

    ret = fseek(f, 0, SEEK_SET);
    assert(ret == 0);

    ret = fread(f, buf, 4);
    assert(ret == 4);

    ret = fseek(f, -1, SEEK_CUR);
    assert(ret == 3);

    ret = fread(f, buf, 1);
    assert(ret == 1);
    assert(buf[0] == 'b');

    fclose(f);

    ret = unlink("TEST_FILE");
    assert(ret == 0);
}

int
main(void)
{
    test_seek();
    test_truncate_shrink();
    test_truncate_grow();
    test_partial_write();
    test_failed_write();
    test_write_gap();
    test_write_fill_block();
    test_write_large_file();
    test_open_trunc();
    test_open_append();
    test_unlink_lazy_delete();
    test_stdio_file();
    test_stdio_file_append();
    test_stdio_fseek_relative();
    printf("All tests passed!\n");
    return 0;
}
