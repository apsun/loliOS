#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

#define STDIN_NONBLOCK 1

static bool
parse_ip(const char *str, ip_addr_t *ip)
{
    int octets[4] = {0};
    int index = 0;
    bool seen_digit = false;
    for (; *str; ++str) {
        if (isdigit(*str)) {
            seen_digit = true;
            octets[index] *= 10;
            octets[index] += *str - '0';
            if (octets[index] > 255) {
                return false;
            }
        } else if (*str == '.') {
            if (!seen_digit || index == 3) {
                return false;
            }
            index++;
            seen_digit = false;
        } else {
            return false;
        }
    }
    if (!seen_digit || index != 3) {
        return false;
    }
    do {
        ip->bytes[index] = octets[index];
    } while (index--);
    return true;
}

static int
nc_loop(ip_addr_t ip, uint16_t port, bool listen)
{
    int ret = 1;
    int sockfd = -1;
    
    if ((sockfd = socket(SOCK_UDP)) < 0) {
        puts("Failed to allocate socket");
        goto cleanup;
    }

    sock_addr_t peer_addr;
    bool know_peer;

    if (listen) {
        sock_addr_t local_addr;
        local_addr.ip = ip;
        local_addr.port = port;
        if (bind(sockfd, &local_addr) < 0) {
            puts("Failed to bind socket");
            goto cleanup;
        }
        know_peer = false;
    } else {
        peer_addr.ip = ip;
        peer_addr.port = port;
        know_peer = true;
    }

    char recv_buf[256];
    char send_buf[256];
    int send_buf_count = 0;
    while (1) {
        int cnt;

        /* Read data from stdin */
        if ((cnt = read(0, &send_buf[send_buf_count], sizeof(send_buf) - send_buf_count)) < 0) {
            puts("Terminal read failed");
            goto cleanup;
        }
        send_buf_count += cnt;

        /* Scan for a newline in the buffer */
        int line_len = -1;
        int i;
        for (i = 0; i < send_buf_count; ++i) {
            if (send_buf[i] == '\n') {
                line_len = i + 1;
                break;
            }
        }

        /* If we found an entire line, send it */
        if (know_peer && line_len > 0) {
            if (sendto(sockfd, send_buf, line_len, &peer_addr) < 0) {
                puts("Socket write failed");
                goto cleanup;
            }
            memmove(&send_buf[0], &send_buf[line_len], send_buf_count - line_len);
            send_buf_count -= line_len;
        }

        /* Read data from socket */
        if ((cnt = recvfrom(sockfd, recv_buf, sizeof(recv_buf), &peer_addr)) < 0) {
            puts("Read from socket failed");
            goto cleanup;
        }

        /* Echo data to terminal */
        if (cnt > 0) {
            if (write(1, recv_buf, cnt) < cnt) {
                puts("Terminal write failed");
                goto cleanup;
            }
            know_peer = true;
        }
    }

    ret = 0;

cleanup:
    if (sockfd >= 0) close(sockfd);
    return ret;
}

int
main(void)
{
    char args_buf[128];
    char *args = args_buf;
    if (getargs(args_buf, sizeof(args_buf)) < 0) {
        puts("Failed to read args");
        return 1;
    }

    bool listen = false;
    if (strncmp("-l ", args, strlen("-l ")) == 0) {
        args += strlen("-l ");
        listen = true;
    }

    char *space = strchr(args, ' ');
    if (space == NULL) {
        puts("No port specified");
        return 1;
    }
    *space = '\0';

    ip_addr_t ip;
    if (!parse_ip(args, &ip)) {
        puts("Invalid IP address");
        return 1;
    }

    int port = atoi(space + 1);
    if (port <= 0 || port >= 65536) {
        puts("Invalid port");
        return 1;
    }

    if (ioctl(0, STDIN_NONBLOCK, 1) < 0) {
        puts("Failed to make stdin non-blocking");
        return 1;
    }

    return nc_loop(ip, port, listen);
}
