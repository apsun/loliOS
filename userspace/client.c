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
        //.ip = IP(172,31,23,63),
        //.ip = IP(10,0,2,2), 
        .ip = IP(127,0,0,1),
        .port = 4321,
    };

    while (1) {
        printf("Enter message: ");
        char buf[128];
        if (gets(buf, sizeof(buf)) == NULL) {
            break;
        }

        if (sendto(sockfd, buf, strlen(buf), &server_addr) < 0) {
            puts("Failed to send message :-(");
            break;
        }
    }

    ret = 0;

cleanup:
    if (sockfd >= 0) close(sockfd);
    return ret;
}
