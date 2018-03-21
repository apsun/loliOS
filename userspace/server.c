#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define IP(a, b, c, d) ((ip_addr_t){.bytes = {(a), (b), (c), (d)}})

int
main(void)
{
    int ret = 1;
    int sockfd = -1;
    
    if ((sockfd = socket(SOCK_UDP)) < 0) {
        puts("Failed to allocate socket");
        goto cleanup;
    }

    sock_addr_t server_addr = {
        .ip = IP(0,0,0,0),
        .port = 4321,
    };

    if (bind(sockfd, &server_addr) < 0) {
        puts("Failed to bind socket");
        goto cleanup;
    }

    while (1) {
        int cnt;
        char buf[128];
        sock_addr_t client_addr;
        if ((cnt = recvfrom(sockfd, buf, sizeof(buf) - 1, &client_addr)) > 0) {
            buf[cnt] = '\0';
            printf("Client says: %s\n", buf);
        }
    }

    ret = 0;

cleanup:
    if (sockfd >= 0) close(sockfd);
    return ret;
}
