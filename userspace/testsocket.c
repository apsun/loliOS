#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

void
fill_buffer(char *buf, int len)
{
    int i;
    for (i = 0; i < len; ++i) {
        buf[i] = (char)i;
    }
}

int
bind2(int fd, sock_addr_t *addr)
{
    int ret = bind(fd, addr);
    if (ret == 0) {
        ret = getsockname(fd, addr);
    }
    return ret;
}

void
test_udp_loopback(void)
{
    int ret;
    int a = socket(SOCK_UDP);
    int b = socket(SOCK_UDP);
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));

    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = bind2(b, &b_addr);
    assert(ret == 0);

    /* a -> b */
    ret = sendto(a, buf, sizeof(buf), &b_addr);
    assert(ret == sizeof(buf));
    ret = recvfrom(b, tmp, sizeof(tmp), NULL);
    assert(ret == sizeof(buf));
    assert(memcmp(buf, tmp, sizeof(buf)) == 0);

    /* b -> a */
    ret = sendto(a, buf, sizeof(buf), &a_addr);
    assert(ret == sizeof(buf));
    ret = recvfrom(a, tmp, sizeof(tmp), NULL);
    assert(ret == sizeof(buf));
    assert(memcmp(buf, tmp, sizeof(buf)) == 0);

    close(a);
    close(b);
}

void
test_udp_huge(void)
{
    int ret;
    int a = socket(SOCK_UDP);
    int b = socket(SOCK_UDP);
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    char buf[2000], tmp[2000];
    fill_buffer(buf, sizeof(buf));

    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = bind2(b, &b_addr);
    assert(ret == 0);

    /* Should fail to send */
    ret = sendto(a, buf, sizeof(buf), &b_addr);
    assert(ret < 0);

    /* Shouldn't receive anything */
    ret = recvfrom(b, tmp, sizeof(tmp), NULL);
    assert(ret == -EAGAIN);

    close(a);
    close(b);
}

void
test_udp_queue(void)
{
    int ret;
    int a = socket(SOCK_UDP);
    int b = socket(SOCK_UDP);
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));

    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = bind2(b, &b_addr);
    assert(ret == 0);

    /* Send a few packets at a time */
    int i;
    for (i = 0; i < (int)sizeof(buf); i += 16) {
        ret = sendto(a, &buf[i], 16, &b_addr);
        assert(ret == 16);
    }

    /* Check receive same packets */
    for (i = 0; i < (int)sizeof(buf); i += 16) {
        ret = recvfrom(b, &tmp[i], sizeof(tmp), &b_addr);
        assert(ret == 16);
    }

    /* Check data in order */
    assert(memcmp(buf, tmp, sizeof(buf)) == 0);

    close(a);
    close(b);
}

void
test_udp_connect(void)
{
    int ret;
    int a = socket(SOCK_UDP);
    int b = socket(SOCK_UDP);
    int c = socket(SOCK_UDP);
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    sock_addr_t c_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));

    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = bind2(b, &b_addr);
    assert(ret == 0);
    ret = bind2(c, &c_addr);
    assert(ret == 0);

    ret = connect(a, &b_addr);
    assert(ret == 0);
    ret = connect(b, &a_addr);
    assert(ret == 0);

    /* Test sendto() with NULL destination addr */
    ret = sendto(a, buf, sizeof(buf), NULL);
    assert(ret == sizeof(buf));
    ret = write(a, buf, sizeof(buf));
    assert(ret == sizeof(buf));

    /* Check data is correct */
    ret = recvfrom(b, tmp, sizeof(tmp), NULL);
    assert(ret == sizeof(buf));
    assert(memcmp(buf, tmp, sizeof(buf)) == 0);
    ret = read(b, tmp, sizeof(tmp));
    assert(ret == sizeof(buf));
    assert(memcmp(buf, tmp, sizeof(buf)) == 0);

    /* Check packets from non-connected peers are dropped */
    ret = sendto(c, buf, sizeof(buf), &b_addr);
    ret = recvfrom(b, tmp, sizeof(tmp), NULL);
    assert(ret == -EAGAIN);

    close(a);
    close(b);
    close(c);
}

void
test_bind_conflict(void)
{
    int ret;
    int a = socket(SOCK_UDP);
    int b = socket(SOCK_UDP);
    int c = socket(SOCK_UDP);

    /* Should work */
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);

    /* Same interface and port as a -> FAIL */
    sock_addr_t b_addr_1 = {.ip = IP(127, 0, 0, 1), .port = a_addr.port};
    ret = bind(b, &b_addr_1);
    assert(ret < 0);

    /* All interfaces, same port as a -> FAIL */
    sock_addr_t b_addr_2 = {.ip = IP(0, 0, 0, 0), .port = a_addr.port};
    ret = bind(b, &b_addr_2);
    assert(ret < 0);

    /* All interfaces, different port -> OK */
    sock_addr_t b_addr_3 = {.ip = IP(0, 0, 0, 0), .port = 0};
    ret = bind2(b, &b_addr_3);
    assert(ret == 0);

    /* One interface, same port as b -> FAIL */
    sock_addr_t c_addr = {.ip = IP(127, 0, 0, 1), .port = b_addr_3.port};
    ret = bind(c, &c_addr);
    assert(ret < 0);

    close(a);
    close(b);

    /* b closed, this should work now */
    ret = bind(c, &c_addr);
    assert(ret == 0);

    close(c);
}

void
test_tcp_basic(void)
{
    int ret;
    int a = socket(SOCK_TCP);
    int b = socket(SOCK_TCP);
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));

    /* Create a listening socket */
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = listen(a, 64);
    assert(ret == 0);

    /* Connect to listening socket */
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(b, &b_addr);
    assert(ret == 0);
    ret = connect(b, &a_addr);
    assert(ret == 0);

    /* Accept incoming connection */
    sock_addr_t tmp_addr;
    int a_conn = accept(a, &tmp_addr);
    assert(a_conn >= 0);
    assert(memcmp(&b_addr, &tmp_addr, sizeof(sock_addr_t)) == 0);

    /* Send data */
    ret = write(a_conn, buf, sizeof(buf));
    assert(ret == sizeof(buf));

    /* Receive data */
    ret = read(b, tmp, sizeof(tmp));
    assert(ret == sizeof(buf));
    assert(memcmp(buf, tmp, sizeof(buf)) == 0);

    close(a_conn);
    close(a);
    close(b);
}

void
test_tcp_invalid(void)
{
    int ret;
    int a = socket(SOCK_TCP);

    /* Create listening socket */
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = listen(a, 64);
    assert(ret == 0);

    /* Check that accept() before listen(), connect() after listen() fail */
    int b = socket(SOCK_TCP);
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(b, &b_addr);
    assert(ret == 0);
    ret = accept(b, NULL);
    assert(ret == -1);
    ret = listen(b, 64);
    assert(ret == 0);
    ret = connect(b, &a_addr);
    assert(ret == -1);
    close(b);

    /* Test sendto/recvfrom don't work on unconnected sockets */
    b = socket(SOCK_TCP);
    char buf[16] = {0};
    ret = recvfrom(b, buf, sizeof(buf), NULL);
    assert(ret == -1);
    ret = sendto(b, buf, sizeof(buf), &a_addr);
    assert(ret == -1);

    /* Test accept() w/ null addr fails */
    ret = connect(b, &a_addr);
    assert(ret == 0);
    int aconn = accept(a, NULL);
    assert(aconn >= 0);
    ret = sendto(b, buf, sizeof(buf), NULL);
    assert(ret == sizeof(buf));

    close(aconn);
    close(b);
    close(a);
}

void
test_tcp_close_with_backlog(void)
{
    int ret;
    int a = socket(SOCK_TCP);

    /* Create listening socket */
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = listen(a, 64);
    assert(ret == 0);

    /* Connect to a, but don't accept */
    int b = socket(SOCK_TCP);
    ret = connect(b, &a_addr);
    assert(ret == 0);

    /* Close a before b */
    close(a);
    close(b);
}

void
test_tcp_close_early(void)
{
    int ret;
    int a = socket(SOCK_TCP);
    ret = close(a);
    assert(ret == 0);
}

void
test_tcp_multi_accept(void)
{
    int ret;
    int a = socket(SOCK_TCP);
    int b = socket(SOCK_TCP);
    int c = socket(SOCK_TCP);

    /* Create listening socket */
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = listen(a, 128);
    assert(ret == 0);
    ret = accept(a, NULL);
    assert(ret == -EAGAIN);

    /* Connect 2 sockets */
    ret = connect(b, &a_addr);
    assert(ret == 0);
    ret = connect(c, &a_addr);
    assert(ret == 0);

    /* Check that both can be accepted */
    int b_conn = accept(a, NULL);
    assert(b_conn >= 0);
    int c_conn = accept(a, NULL);
    assert(c_conn >= 0);

    close(c_conn);
    close(b_conn);
    close(c);
    close(b);
    close(a);
}

void
test_tcp_segmentation(void)
{
    int ret;
    int a = socket(SOCK_TCP);
    int b = socket(SOCK_TCP);

    /* Create listening socket */
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = listen(a, 128);
    assert(ret == 0);

    /* Connect to socket */
    ret = connect(b, &a_addr);
    assert(ret == 0);
    int aconn = accept(a, NULL);
    assert(aconn >= 0);

    /* Send huge packet */
    char buf[5000];
    fill_buffer(buf, sizeof(buf));
    ret = write(aconn, buf, sizeof(buf));
    assert(ret == sizeof(buf));

    /* Read huge packet */
    char tmp[5000];
    ret = read(b, tmp, sizeof(tmp));
    assert(ret == sizeof(buf));
    assert(memcmp(tmp, buf, sizeof(buf)) == 0);

    close(aconn);
    close(b);
    close(a);
}

void
test_tcp_shutdown(void)
{
    int ret;
    int a = socket(SOCK_TCP);
    int b = socket(SOCK_TCP);
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));

    /* Create a listening socket */
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(a, &a_addr);
    assert(ret == 0);
    ret = listen(a, 64);
    assert(ret == 0);

    /* Connect to listening socket */
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 0};
    ret = bind2(b, &b_addr);
    assert(ret == 0);
    ret = connect(b, &a_addr);
    assert(ret == 0);

    /* Accept incoming connection */
    sock_addr_t tmp_addr;
    int a_conn = accept(a, &tmp_addr);
    assert(a_conn >= 0);

    /* Shutdown socket b */
    ret = shutdown(b);
    assert(ret >= 0);

    /* Send data a -> b */
    ret = write(a_conn, buf, sizeof(buf));
    assert(ret == sizeof(buf));

    /* Receive data */
    ret = read(b, tmp, sizeof(tmp));
    assert(ret == sizeof(buf));
    assert(memcmp(buf, tmp, sizeof(buf)) == 0);

    /* Send data b -> a fail */
    ret = write(b, buf, sizeof(buf));
    assert(ret < 0);

    close(a_conn);
    close(a);
    close(b);
}

int
main(void)
{
    test_udp_loopback();
    test_udp_huge();
    test_udp_queue();
    test_udp_connect();
    test_bind_conflict();
    test_tcp_basic();
    test_tcp_invalid();
    test_tcp_close_with_backlog();
    test_tcp_close_early();
    test_tcp_multi_accept();
    test_tcp_segmentation();
    test_tcp_shutdown();
    printf("All tests passed!\n");
    return 0;
}
