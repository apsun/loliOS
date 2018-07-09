#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

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

typedef struct {
    char buf[128];
    bool listen : 1;
    bool udp : 1;
    bool crlf : 1;
    char *argv;
} args_t;

static volatile bool stop = false;

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
lf_to_crlf(char *buf, int start, int count)
{
    /*
     * First count the number of LFs so we know how much
     * we have to shift by, so we can have a O(n) algorithm
     * instead of O(n^2)
     */
    int i;
    int num_lf = 0;
    for (i = 0; i < count; ++i) {
        if (buf[start + i] == '\n') {
            num_lf++;
        }
    }

    /*
     * Now go from the end, replacing \n with \r\n
     */
    int j;
    for (i = count - 1, j = count + num_lf - 1; i >= 0; --i) {
        if (buf[i] == '\n') {
            buf[j--] = '\n';
            buf[j--] = '\r';
        } else {
            buf[j--] = buf[i];
        }
    }

    return num_lf;
}

static int
input(int fd, char *buf, int buf_size, int *offset, bool crlf)
{
    int to_read = buf_size - *offset;

    /*
     * Worst cast scenario: ALL the characters are \n,
     * so we reserve half the buffer for the extra \r chars.
     * Note that we want truncating division here.
     */
    if (crlf) {
        to_read /= 2;
    }

    /* Check if we have space left to read */
    if (to_read == 0) {
        return -EAGAIN;
    }

    /* Read data into buffer */
    int ret = read(fd, &buf[*offset], to_read);
    if (ret <= 0) {
        return ret;
    }

    /* Translate LF to CRLF */
    if (crlf) {
        ret += lf_to_crlf(buf, *offset, ret);
    }

    /* Advance offset */
    *offset += ret;
    return ret;
}

static int
output(int fd, char *buf, int *count)
{
    /* Check if we have anything to write */
    if (*count == 0) {
        return -EAGAIN;
    }

    /* Write out buffer contents */
    int ret = write(fd, buf, *count);
    if (ret <= 0) {
        return ret;
    }

    /* Shift remaining bytes up */
    memmove(&buf[0], &buf[ret], *count - ret);
    *count -= ret;
    return ret;
}

static int
sock_input(int sockfd, char *buf, int buf_size, int *offset, sock_addr_t *addr)
{
    /* Check if we have space left to read */
    int to_read = buf_size - *offset;
    if (to_read == 0) {
        return -EAGAIN;
    }

    /* Read data into buffer */
    int ret = recvfrom(sockfd, &buf[*offset], to_read, addr);
    if (ret <= 0) {
        return ret;
    }

    /* Advance offset */
    *offset += ret;
    return ret;
}

static int
sock_output(int sockfd, char *buf, int *count, sock_addr_t *addr)
{
    /* Check if we have anything to write */
    if (*count == 0) {
        return -EAGAIN;
    }

    /* Write out buffer contents */
    int ret = sendto(sockfd, buf, *count, addr);
    if (ret <= 0) {
        return ret;
    }

    /* Shift remaining bytes up */
    memmove(&buf[0], &buf[ret], *count - ret);
    *count -= ret;
    return ret;
}


static void
sig_interrupt_handler(int signum)
{
    stop = true;
}

static int
nc_loop(ip_addr_t ip, uint16_t port, args_t *args)
{
    int ret;
    int sockfd = -1;
    int listenfd = -1;
    char send_buf[8192];
    char recv_buf[8192];
    int send_offset = 0;
    int recv_offset = 0;
    bool send_done = false;
    bool recv_done = false;
    bool bound = false;
    bool connected = false;
    sock_addr_t local_addr;
    sock_addr_t remote_addr;

#define CALL(expr) do {                     \
    ret = expr;                             \
    if (ret == -1) {                        \
        fprintf(stderr, #expr " failed\n"); \
        goto cleanup;                       \
    }                                       \
} while (0)

    if (args->listen) {
        local_addr.ip = ip;
        local_addr.port = port;
        if (args->udp) {
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
        if (args->udp) {
            CALL(sockfd = socket(SOCK_UDP));
            bound = false;
        } else {
            CALL(sockfd = socket(SOCK_TCP));
            bound = true;
        }
        CALL(connect(sockfd, &remote_addr));
        connected = true;
    }

    stop = false;
    while (!stop && (!send_done || !recv_done)) {
        /* If passive TCP socket, wait for a connection */
        if (sockfd < 0) {
            CALL(sockfd = accept(listenfd, &remote_addr));
            if (sockfd == -EINTR || sockfd == -EAGAIN) {
                continue;
            }
            connected = true;
            bound = true;
        }

        /* Read outbound data from stdin */
        CALL(input(stdin, send_buf, sizeof(send_buf), &send_offset, args->crlf));

        /* If done reading from stdin, send a FIN */
        if (ret == 0 && send_offset == 0) {
            CALL(shutdown(sockfd));
            send_done = true;
        }

        /* If we know the peer and have data to send, do it now! */
        if (connected && send_offset > 0) {
            CALL(sock_output(sockfd, send_buf, &send_offset, &remote_addr));
            bound = true;
        }

        /*
         * If we're bound (UDP active mode is unbound until first
         * sent packet), try to receive some data. Also make sure
         * the socket is connected for UDP, so we don't get packets
         * from other senders.
         */
        if (bound) {
            CALL(sock_input(sockfd, recv_buf, sizeof(recv_buf), &recv_offset, &remote_addr));
            if (ret == 0 && recv_offset == 0) {
                recv_done = true;
            }

            if (!connected) {
                CALL(connect(sockfd, &remote_addr));
                connected = true;
            }
        }

        /* Write inbound data to stdout */
        CALL(output(stdout, recv_buf, &recv_offset));
    }

#undef CALL

    ret = 0;

cleanup:
    if (listenfd >= 0) close(listenfd);
    if (sockfd >= 0) close(sockfd);
    return ret;
}

static bool
parse_args(args_t *args)
{
    args->argv = args->buf;

    if (getargs(args->buf, sizeof(args->buf)) < 0) {
        args->buf[0] = '\0';
        return args;
    }

    while (1) {
        if (*args->argv == ' ') {
            args->argv++;
        } else if (*args->argv == '-') {
            char c;
            while ((c = *++args->argv) && c != ' ') {
                switch (c) {
                case 'l':
                    args->listen = true;
                    break;
                case 'u':
                    args->udp = true;
                    break;
                case 'c':
                    args->crlf = true;
                    break;
                default:
                    fprintf(stderr, "Unknown option: %c\n", c);
                    return NULL;
                }
            }
        } else {
            return args;
        }
    }
}

int
main(void)
{
    /* Set signal handler */
    if (sigaction(SIG_INTERRUPT, sig_interrupt_handler) < 0) {
        fprintf(stderr, "Could not set interrupt handler\n");
        return 1;
    }

    /* Parse arguments */
    args_t args = {.udp = false, .listen = false, .crlf = false};
    if (!parse_args(&args)) {
        return 1;
    }

    /* Find space between host and port */
    char *space = strchr(args.argv, ' ');
    if (space == NULL) {
        fprintf(stderr, "No port specified\n");
        return 1;
    }
    *space = '\0';

    /* Parse hostname/IP address */
    ip_addr_t ip;
    if (!ip_parse(args.argv, &ip)) {
        if (!args.listen) {
            if (!dns_resolve(args.argv, &ip)) {
                fprintf(stderr, "Could not resolve address\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Invalid interface IP address\n");
            return 1;
        }
    }

    /* Parse port number */
    int port = atoi(space + 1);
    if (port <= 0 || port >= 65536) {
        fprintf(stderr, "Invalid port\n");
        return 1;
    }

    /* Put stdin into nonblocking mode */
    int orig_nonblock = fcntl(0, FCNTL_NONBLOCK, 1);
    if (orig_nonblock < 0) {
        fprintf(stderr, "Failed to make stdin non-blocking\n");
        return 1;
    }

    /* Run main loop */
    int ret = nc_loop(ip, port, &args);

    /* Restore original blocking mode */
    if (fcntl(0, FCNTL_NONBLOCK, orig_nonblock) < 0) {
        fprintf(stderr, "Failed to restore stdin blocking mode\n");
        return 1;
    }

    return ret;
}
