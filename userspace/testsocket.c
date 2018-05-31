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

void
test_udp_loopback(void)
{
    int ret;
    int a = socket(SOCK_UDP);
    int b = socket(SOCK_UDP);
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 5555};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 6666};
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));
    
    ret = bind(a, &a_addr);
    assert(ret == 0);
    ret = bind(b, &b_addr);
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
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 5555};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 6666};
    char buf[2000], tmp[2000];
    fill_buffer(buf, sizeof(buf));
    
    ret = bind(a, &a_addr);
    assert(ret == 0);
    ret = bind(b, &b_addr);
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
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 5555};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 6666};
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));

    ret = bind(a, &a_addr);
    assert(ret == 0);
    ret = bind(b, &b_addr);
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
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 5555};
    sock_addr_t b_addr = {.ip = IP(127, 0, 0, 1), .port = 6666};
    sock_addr_t c_addr = {.ip = IP(127, 0, 0, 1), .port = 7777};
    char buf[64], tmp[1500];
    fill_buffer(buf, sizeof(buf));

    ret = bind(a, &a_addr);
    assert(ret == 0);
    ret = bind(b, &b_addr);
    assert(ret == 0);
    ret = bind(c, &c_addr);
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
    assert(ret < 0);

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
    sock_addr_t a_addr = {.ip = IP(127, 0, 0, 1), .port = 5555};
    ret = bind(a, &a_addr);
    assert(ret == 0);

    /* Same interface and port as a -> FAIL */
    sock_addr_t b_addr_1 = {.ip = IP(127, 0, 0, 1), .port = 5555};
    ret = bind(b, &b_addr_1);
    assert(ret < 0);

    /* All interfaces, same port as a -> FAIL */
    sock_addr_t b_addr_2 = {.ip = IP(0, 0, 0, 0), .port = 5555};
    ret = bind(b, &b_addr_2);
    assert(ret < 0);

    /* All interfaces, different port -> OK */
    sock_addr_t b_addr_3 = {.ip = IP(0, 0, 0, 0), .port = 6666};
    ret = bind(b, &b_addr_3);
    assert(ret == 0);

    /* One interface, same port as b -> FAIL */
    sock_addr_t c_addr = {.ip = IP(127, 0, 0, 1), .port = 6666};
    ret = bind(c, &c_addr);
    assert(ret < 0);

    close(a);
    close(b);

    /* b closed, this should work now */
    ret = bind(c, &c_addr);
    assert(ret == 0);

    close(c);
}

int
main(void)
{
    test_udp_loopback();
    test_udp_huge();
    test_udp_queue();
    test_udp_connect();
    test_bind_conflict();
    printf("All tests passed!\n");
    return 0;
}
