#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

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
    if (index != 3) {
        return false;
    }
    do {
        ip->bytes[index] = octets[index];
    } while (index--);
    return true;
}

static int
nc_listen(ip_addr_t ip, uint16_t port)
{
    int ret = 1;
    int sockfd = -1;
    
    if ((sockfd = socket(SOCK_UDP)) < 0) {
        puts("Failed to allocate socket");
        goto cleanup;
    }

    sock_addr_t server_addr;
    server_addr.ip = ip;
    server_addr.port = port;
    if (bind(sockfd, &server_addr) < 0) {
        puts("Failed to bind socket");
        goto cleanup;
    }

    while (1) {
        char buf[128];
        sock_addr_t client_addr;
        int cnt;
        if ((cnt = recvfrom(sockfd, buf, sizeof(buf), &client_addr)) < 0) {
            puts("Read from socket failed");
            goto cleanup;
        }
        
        if (cnt > 0 && write(1, buf, cnt) < cnt) {
            puts("Terminal write failed");
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    if (sockfd >= 0) close(sockfd);
    return ret;
}

static int
nc_connect(ip_addr_t ip, uint16_t port)
{
    int ret = 1;
    int sockfd = -1;

    if ((sockfd = socket(SOCK_UDP)) < 0) {
        puts("Failed to allocate socket");
        goto cleanup;
    }

    sock_addr_t server_addr;
    server_addr.ip = ip;
    server_addr.port = port;

    while (1) {
        char buf[128];
        int cnt;
        if ((cnt = read(0, buf, sizeof(buf))) < 0) {
            puts("Terminal read failed");
            goto cleanup;
        }

        if (cnt > 0 && sendto(sockfd, buf, cnt, &server_addr) < 0) {
            puts("Socket write failed");
            goto cleanup;
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

    if (listen) {
        return nc_listen(ip, port);
    } else {
        return nc_connect(ip, port);
    }
}
