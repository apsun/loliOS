#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

#define STDIN_NONBLOCK 1

#define DNS_SERVER IP(10, 0, 2, 3)
#define DNS_PORT 53
#define DNS_TIMEOUT 1 /* Sadly seconds is the best resolution we have */

typedef struct {
    uint16_t be_id;
    uint16_t be_flags;
    uint16_t be_qdcount;
    uint16_t be_ancount;
    uint16_t be_nscount;
    uint16_t be_arcount;
} __attribute__((packed)) dns_hdr_t;

typedef struct {
    uint16_t be_qtype;
    uint16_t be_qclass;
} __attribute__((packed)) dns_q_ftr_t;

typedef struct {
    uint16_t be_type;
    uint16_t be_class;
    uint32_t be_ttl;
    uint16_t be_rdlength;
} __attribute__((packed)) dns_a_hdr_t;

static inline uint16_t
bswap16(uint16_t x)
{
    return (
        (x & 0x00ff) << 8 |
        (x & 0xff00) >> 8);
}

#define ntohs(x) bswap16(x)
#define htons(x) bswap16(x)

static uint8_t *
dns_skip_hostname(uint8_t *bufp)
{
    while (1) {
        /* Find length of next segment */
        uint8_t len = *bufp++;
        if (len == 0) {
            break;
        }

        /* Remainder of hostname is a compressed segment */
        if ((len & 0xc0) == 0xc0) {
            bufp++;
            break;
        }

        /* Skip over segment */
        bufp += len;
    }

    return bufp;
}

static bool
dns_parse_reply(uint8_t *buf, int cnt, ip_addr_t *ip)
{
    uint8_t *bufp = buf;

    /* Validate header */
    if (cnt < (int)sizeof(dns_hdr_t)) {
        return false;
    }

    dns_hdr_t *hdr = (dns_hdr_t *)bufp;

    /* Check that it's a recursive DNS reply (we're too lazy to do it ourselves) */
    if ((ntohs(hdr->be_flags) & 0x8080) != 0x8080) {
        return false;
    }

    /* Check that it's a reply for our single question */
    if (ntohs(hdr->be_qdcount) != 1 || ntohs(hdr->be_ancount) == 0) {
        return false;
    }

    bufp += sizeof(dns_hdr_t);

    /* Skip query */
    bufp = dns_skip_hostname(bufp);
    bufp += sizeof(dns_q_ftr_t);

    /* Find first A record in result */
    int i;
    for (i = 0; i < ntohs(hdr->be_ancount); ++i) {
        bool valid = true;

        /* Only look for compressed records referring to query hostname */
        uint8_t len = *bufp;
        if ((len & 0xc0) != 0xc0) {
            bufp = dns_skip_hostname(bufp);
            valid = false;
        } else {
            uint16_t off = ntohs(*(uint16_t *)bufp) & ~0xc000;
            bufp += sizeof(uint16_t);
            if (off != sizeof(dns_hdr_t)) {
                valid = false;
            }
        }

        /* Check answer header fields */
        dns_a_hdr_t *ahdr = (dns_a_hdr_t *)bufp;
        if (ntohs(ahdr->be_type) != 0x0001 ||
            ntohs(ahdr->be_class) != 0x0001 ||
            ntohs(ahdr->be_rdlength) != 4) {
            valid = false;
        }

        /* Move to data field */
        bufp += sizeof(dns_a_hdr_t);

        /* Valid? Great! Get the first entry. */
        if (valid) {
            memcpy(&ip->bytes, bufp, 4);
            return true;
        }

        /* Move to next answer */
        bufp += ntohs(ahdr->be_rdlength);
    }

    return false;
}

static int
dns_fill_query(uint8_t *buf, const char *hostname)
{
    /*
     * Query is in format [3]www[6]google[3]com[0],
     * where [N] represents (char)N.
     */
    uint8_t *bufp = buf;
    const char *p = hostname;
    int i;
    while (1) {
        /* Copy until . or end-of-string */
        i = 0;
        while (*p && *p != '.') {
            /* Hyphen only allowed if not first/last character in segment */
            char c = *p++;
            if (isalnum(c) || (c == '-' && i > 0 && (*p && *p != '.'))) {
                bufp[++i] = c;
            } else {
                return -1;
            }

            /* Each segment must be < 64B, overall < 256B */
            if (i == 64 || (bufp - buf + i) == 256) {
                return -1;
            }
        }

        /* Empty segments are invalid */
        if (i == 0) {
            return -1;
        }

        /* Prepend length */
        bufp[0] = i;
        bufp += i + 1;

        /* If . then move to next segment, otherwise stop */
        if (*p == '.') {
            p++;
        } else {
            *bufp++ = '\0';
            break;
        }
    }

    /* Fill in footer */
    dns_q_ftr_t *ftr = (dns_q_ftr_t *)bufp;
    ftr->be_qtype = htons(0x0001); /* A records */
    ftr->be_qclass = htons(0x0001); /* Internet addr */
    bufp += sizeof(*ftr);

    return bufp - buf;
}

static void
dns_fill_header(uint8_t *buf)
{
    /* Fill out header */
    dns_hdr_t *hdr = (dns_hdr_t *)buf;
    hdr->be_id = htons(rand() & 0xffff);
    hdr->be_flags = htons(0x0100); /* Recursive query: YES */
    hdr->be_qdcount = htons(1); /* 1 question */
    hdr->be_ancount = htons(0);
    hdr->be_nscount = htons(0);
    hdr->be_arcount = htons(0);
}

static bool
dns_resolve(const char *hostname, ip_addr_t *ip)
{
    bool ret = false;
    int sockfd = -1;

    /* FQDN is at most 255 characters + NUL, 512B is definitely enough */
    uint8_t qbuf[512];
    dns_fill_header(qbuf);
    uint8_t *qquery = &qbuf[sizeof(dns_hdr_t)];
    int qlen = dns_fill_query(qquery, hostname);
    if (qlen < 0) {
        goto cleanup;
    }

    /* DNS over UDP */
    sockfd = socket(SOCK_UDP);
    if (sockfd < 0) {
        goto cleanup;
    }

    /* Send request */
    sock_addr_t addr = {.ip = DNS_SERVER, .port = DNS_PORT};
    if (sendto(sockfd, qbuf, sizeof(dns_hdr_t) + qlen, &addr) < 0) {
        goto cleanup;
    }

    /* Wait for reply (<= in case clock ticks over riiiight after we start) */
    int start = time();
    uint8_t rbuf[0x600];
    while (time() <= start + DNS_TIMEOUT) {
        int rcnt;
        if ((rcnt = recvfrom(sockfd, rbuf, sizeof(rbuf), NULL)) != 0) {
            if (rcnt == -EINTR || rcnt == -EAGAIN) {
                continue;
            } else if (rcnt < 0) {
                goto cleanup;
            } else {
                ret = dns_parse_reply(rbuf, rcnt, ip);
                break;
            }
        }
    }

cleanup:
    if (sockfd >= 0) close(sockfd);
    return ret;
}

static bool
ip_parse(const char *str, ip_addr_t *ip)
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
nc_loop(ip_addr_t ip, uint16_t port, bool passive, bool udp)
{
    int ret;
    int sockfd = -1;
    int listenfd = -1;
    char recv_buf[0x600];
    char send_buf[256];
    int send_buf_count = 0;
    bool bound = false;
    bool connected = false;
    sock_addr_t local_addr;
    sock_addr_t remote_addr;

#define CALL(expr) do {        \
    ret = expr;                \
    if (ret == -1) {           \
        puts(#expr " failed"); \
        goto cleanup;          \
    }                          \
} while (0)

    if (passive) {
        local_addr.ip = ip;
        local_addr.port = port;
        if (udp) {
            CALL(sockfd = socket(SOCK_UDP));
            CALL(bind(sockfd, &local_addr));
            bound = true;
        } else {
            CALL(listenfd = socket(SOCK_TCP));
            CALL(bind(listenfd, &local_addr));
            CALL(listen(listenfd, 128));
            bound = false;
        }
        connected = false;
    } else {
        remote_addr.ip = ip;
        remote_addr.port = port;
        if (udp) {
            CALL(sockfd = socket(SOCK_UDP));
            bound = false;
        } else {
            CALL(sockfd = socket(SOCK_TCP));
            bound = true;
        }
        CALL(connect(sockfd, &remote_addr));
        connected = true;
    }

    while (1) {
        /* If passive TCP socket, wait for a connection */
        if (sockfd < 0) {
            CALL(sockfd = accept(listenfd, &remote_addr));
            if (sockfd == -EINTR || sockfd == -EAGAIN) {
                continue;
            }
            connected = true;
            bound = true;
        }

        /* Read data from stdin */
        CALL(read(0, &send_buf[send_buf_count], sizeof(send_buf) - send_buf_count));
        if (ret == -EINTR || ret == -EAGAIN) {
            ret = 0;
        }
        send_buf_count += ret;

        /* Scan for a newline in the buffer */
        int line_len = 0;
        int i;
        for (i = 0; i < send_buf_count; ++i) {
            if (send_buf[i] == '\n') {
                line_len = i + 1;
                break;
            }
        }

        /* If we found an entire line, send it */
        if (connected && line_len > 0) {
            CALL(write(sockfd, send_buf, line_len));
            memmove(&send_buf[0], &send_buf[line_len], send_buf_count - line_len);
            send_buf_count -= line_len;
            bound = true;
        }

        /* Read data from socket */
        if (bound) {
            CALL(recvfrom(sockfd, recv_buf, sizeof(recv_buf), &remote_addr));
            if (ret == -EINTR || ret == -EAGAIN) {
                continue;
            } else if (ret == 0) {
                break;
            }
            CALL(write(1, recv_buf, ret));
            if (!connected) {
                CALL(connect(sockfd, &remote_addr));
                connected = true;
            }
        }
    }

#undef CALL

    ret = 0;

cleanup:
    if (listenfd >= 0) close(listenfd);
    if (sockfd >= 0) close(sockfd);
    return ret;
}

int
main(void)
{
    char args_buf[128];
    if (getargs(args_buf, sizeof(args_buf)) < 0) {
        puts("Failed to read args");
        return 1;
    }

    bool listen = false;
    bool udp = false;
    char *args = args_buf;
    while (1) {
        if (*args == ' ') {
            args++;
        } else if (*args == '-') {
            char c;
            while ((c = *++args) && c != ' ') {
                switch (c) {
                case 'l':
                    listen = true;
                    break;
                case 'u':
                    udp = true;
                    break;
                default:
                    printf("Unknown option: %c\n", c);
                    return 1;
                }
            }
        } else {
            break;
        }
    }

    char *space = strchr(args, ' ');
    if (space == NULL) {
        puts("No port specified");
        return 1;
    }
    *space = '\0';

    ip_addr_t ip;
    if (!ip_parse(args, &ip)) {
        if (!listen) {
            if (!dns_resolve(args, &ip)) {
                puts("Could not resolve address");
                return 1;
            }
        } else {
            puts("Invalid interface IP address");
            return 1;
        }
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

    return nc_loop(ip, port, listen, udp);
}
