#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

#define DNS_SERVER IP(10, 0, 2, 3)
#define DNS_PORT 53

#define DNS_TYPE_A 0x0001
#define DNS_CLASS_IN 0x0001

/* Maximum number of bytes in a DNS name */
#define DNS_MAX_NAME_LEN 256

/* How long to wait for a DNS reply */
#define DNS_TIMEOUT 1000

/* UDP and TCP MSS sizes */
#define UDP_MAX_LEN 1472
#define TCP_MAX_LEN 1460

typedef struct {
    uint16_t be_id;
    union {
        struct {
            uint16_t rd     : 1;
            uint16_t tc     : 1;
            uint16_t aa     : 1;
            uint16_t opcode : 4;
            uint16_t qr     : 1;
            uint16_t rcode  : 4;
            uint16_t zero   : 3;
            uint16_t ra     : 1;
        };
        uint16_t be_flags;
    };
    uint16_t be_qdcount;
    uint16_t be_ancount;
    uint16_t be_nscount;
    uint16_t be_arcount;
} __attribute__((packed)) dns_hdr_t;

typedef struct {
    uint16_t be_qtype;
    uint16_t be_qclass;
} __attribute__((packed)) dns_qhdr_t;

typedef struct {
    uint16_t be_type;
    uint16_t be_class;
    uint32_t be_ttl;
    uint16_t be_rdlength;
} __attribute__((packed)) dns_ahdr_t;

typedef struct {
    char buf[128];
    bool listen : 1;
    bool udp : 1;
    bool crlf : 1;
    char *argv;
} args_t;

typedef struct {
    uint8_t data[UDP_MAX_LEN];
    int length;
    int offset;
} dns_buf_t;

/*
 * Represents a binary-encoded domain name using the
 * length-prefixed format, e.g. [3]www[6]google[3]com[0]
 * where [N] represents (char)N.
 */
typedef struct {
    uint8_t data[DNS_MAX_NAME_LEN];
    int length;
} dns_name_t;

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

static bool
dns_buf_overflow(dns_buf_t *buf, int n)
{
    return buf->offset + n > buf->length;
}

static bool
dns_name_equals(const dns_name_t *a, const dns_name_t *b)
{
    if (a->length != b->length) {
        return false;
    }

    int offset = 0;
    while (1) {
        uint8_t alen = a->data[offset];
        uint8_t blen = b->data[offset];
        if (alen != blen) {
            return false;
        }
        offset++;

        if (alen == 0) {
            return true;
        }

        if (memcmp(&a->data[offset], &b->data[offset], alen) != 0) {
            return false;
        }
        offset += alen;
    }
}

static bool
dns_name_parse_text(const char *hostname, dns_name_t *out_name)
{
    out_name->length = 0;

    const char *p = hostname;
    while (1) {
        /* Copy until . or end-of-string */
        int seglen = 0;
        while (*p && *p != '.') {
            char c = *p++;

            /* Hyphen only allowed if not first/last character in segment */
            if (!isalnum(c) && !(c == '-' && seglen > 0 && (*p && *p != '.'))) {
                fprintf(stderr, "DNS invalid segment format\n");
                return false;
            }

            seglen++;
            out_name->data[out_name->length + seglen] = c;

            /*
             * Each segment must be < 64B, and < 256B overall. We also need
             * to leave space for the trailing 0 byte, hence the - 1.
             */
            if (seglen == 64 || out_name->length + seglen == DNS_MAX_NAME_LEN - 1) {
                fprintf(stderr, "DNS segment or overall length too long\n");
                return false;
            }
        }

        /* Empty segments are invalid */
        if (seglen == 0) {
            fprintf(stderr, "DNS empty segment\n");
            return false;
        }

        /* Prepend length */
        out_name->data[out_name->length] = seglen;
        out_name->length += seglen + 1;

        /* If . then move to next segment, otherwise stop */
        if (*p == '.') {
            p++;
        } else {
            out_name->data[out_name->length++] = 0;
            return true;
        }
    }
}

static bool
dns_name_parse_compressed(dns_buf_t *buf, dns_name_t *out_name)
{
    out_name->length = 0;

    int pos = buf->offset;
    bool compressed = false;
    while (1) {
        if (pos + 1 > buf->length) {
            fprintf(stderr, "DNS domain name overflows input buffer\n");
            return false;
        }

        uint8_t seglen = buf->data[pos];
        if ((seglen & 0xc0) == 0xc0) {
            if (pos + 2 > buf->length) {
                fprintf(stderr, "DNS compressed length overflows input buffer\n");
                return false;
            }

            int newpos = ((seglen & ~0xc0) << 8) | buf->data[pos + 1];
            if (newpos >= pos) {
                fprintf(stderr, "DNS compressed pointer points forward\n");
                return false;
            }

            pos = newpos;
            if (!compressed) {
                buf->offset += 2;
                compressed = true;
            }
        } else {
            if (pos + seglen + 1 > buf->length) {
                fprintf(stderr, "DNS domain name overflows input buffer\n");
                return false;
            }

            if (out_name->length + seglen + 1 > DNS_MAX_NAME_LEN) {
                fprintf(stderr, "DNS domain name is too long\n");
                return false;
            }

            memcpy(&out_name->data[out_name->length], &buf->data[pos], seglen + 1);
            out_name->length += seglen + 1;
            pos += seglen + 1;
            if (!compressed) {
                buf->offset += seglen + 1;
            }

            if (seglen == 0) {
                return true;
            }
        }
    }
}

static bool
dns_read_response_header(dns_buf_t *buf, uint16_t id, const dns_hdr_t **out_hdr)
{
    /* Read DNS header */
    if (dns_buf_overflow(buf, sizeof(dns_hdr_t))) {
        fprintf(stderr, "DNS header overflows input buffer\n");
        return false;
    }
    dns_hdr_t *hdr = (dns_hdr_t *)&buf->data[buf->offset];
    buf->offset += sizeof(dns_hdr_t);

    /* Check that ID matches what we sent in the query */
    if (ntohs(hdr->be_id) != id) {
        fprintf(stderr, "DNS response ID mismatch\n");
        return false;
    }

    /* Check that flags are good */
    if (hdr->qr != 1 ||      /* Is a response */
        hdr->opcode != 0 ||  /* Is a standard query */
        hdr->tc != 0 ||      /* Was not truncated */
        hdr->ra != 1 ||      /* Was recursive */
        hdr->rcode != 0)     /* No error */
    {
        fprintf(stderr, "DNS flags are not good\n");
        return false;
    }

    *out_hdr = hdr;
    return true;
}

static bool
dns_read_response_question(
    dns_buf_t *buf,
    const dns_hdr_t *hdr,
    const dns_name_t *name)
{
    int i;
    for (i = 0; i < ntohs(hdr->be_qdcount); ++i) {
        /* Check that the question is for the domain name we queried */
        dns_name_t qname;
        if (!dns_name_parse_compressed(buf, &qname)) {
            return false;
        }
        if (!dns_name_equals(name, &qname)) {
            fprintf(stderr, "DNS question does not match queried domain name\n");
            return false;
        }

        /* Read rest of question header */
        if (dns_buf_overflow(buf, sizeof(dns_qhdr_t))) {
            fprintf(stderr, "DNS question header overflows input buffer\n");
            return false;
        }
        dns_qhdr_t *qhdr = (dns_qhdr_t *)&buf->data[buf->offset];
        buf->offset += sizeof(dns_qhdr_t);

        /*
         * Check that the question matches the type (A record) and class
         * (internet addr) we queried for.
         */
        if (ntohs(qhdr->be_qtype) != DNS_TYPE_A ||
            ntohs(qhdr->be_qclass) != DNS_CLASS_IN)
        {
            fprintf(stderr, "DNS question header is not A record for IN addr\n");
            return false;
        }

        return true;
    }

    return false;
}

static bool
dns_read_response_answer(
    dns_buf_t *buf,
    const dns_hdr_t *hdr,
    const dns_name_t *name,
    ip_addr_t *out_ip)
{
    int i;
    for (i = 0; i < ntohs(hdr->be_ancount); ++i) {
        bool skip = false;

        /* Check that the answer is for the domain name we queried */
        dns_name_t aname;
        if (!dns_name_parse_compressed(buf, &aname)) {
            return false;
        }
        if (!dns_name_equals(name, &aname)) {
            skip = true;
        }

        /* Read rest of answer header */
        if (dns_buf_overflow(buf, sizeof(dns_ahdr_t))) {
            fprintf(stderr, "DNS answer header overflows input buffer\n");
            return false;
        }
        dns_ahdr_t *ahdr = (dns_ahdr_t *)&buf->data[buf->offset];
        buf->offset += sizeof(dns_ahdr_t);

        /*
         * Check that the answer matches the type (A record) and class
         * (internet addr) we queried for.
         */
        if (ntohs(ahdr->be_type) != DNS_TYPE_A ||
            ntohs(ahdr->be_class) != DNS_CLASS_IN ||
            ntohs(ahdr->be_rdlength) != sizeof(ip_addr_t))
        {
            skip = true;
        }

        /* Read IP address data */
        if (dns_buf_overflow(buf, sizeof(ip_addr_t))) {
            fprintf(stderr, "DNS answer data overflows input buffer\n");
            return false;
        }
        void *ip = &buf->data[buf->offset];
        buf->offset += sizeof(ip_addr_t);

        if (!skip) {
            memcpy(&out_ip->bytes, ip, sizeof(ip_addr_t));
            return true;
        }
    }

    return false;
}

static bool
dns_read_response(
    dns_buf_t *buf,
    const dns_name_t *name,
    uint16_t id,
    ip_addr_t *out_ip)
{
    const dns_hdr_t *hdr;
    return
        dns_read_response_header(buf, id, &hdr) &&
        dns_read_response_question(buf, hdr, name) &&
        dns_read_response_answer(buf, hdr, name, out_ip);
}

static bool
dns_write_query_header(dns_buf_t *buf, uint16_t id)
{
    if (dns_buf_overflow(buf, sizeof(dns_hdr_t))) {
        fprintf(stderr, "DNS query data overflows output buffer\n");
        return false;
    }
    dns_hdr_t *hdr = (dns_hdr_t *)&buf->data[buf->offset];
    buf->offset += sizeof(dns_hdr_t);

    hdr->be_id = htons(id);
    hdr->be_flags = htons(0);
    hdr->qr = 0;
    hdr->opcode = 0;
    hdr->rd = 1;
    hdr->be_qdcount = htons(1);
    hdr->be_ancount = htons(0);
    hdr->be_nscount = htons(0);
    hdr->be_arcount = htons(0);
    return true;
}

static bool
dns_write_query_question(dns_buf_t *buf, const dns_name_t *name)
{
    /* Emit binary domain name */
    if (dns_buf_overflow(buf, name->length)) {
        fprintf(stderr, "DNS query name overflows output buffer\n");
        return false;
    }
    memcpy(&buf->data[buf->offset], name->data, name->length);
    buf->offset += name->length;

    /* Emit question header */
    if (dns_buf_overflow(buf, sizeof(dns_qhdr_t))) {
        fprintf(stderr, "DNS query header overflows output buffer\n");
        return false;
    }
    dns_qhdr_t *qhdr = (dns_qhdr_t *)&buf->data[buf->offset];
    buf->offset += sizeof(dns_qhdr_t);
    qhdr->be_qtype = htons(DNS_TYPE_A);
    qhdr->be_qclass = htons(DNS_CLASS_IN);
    return true;
}

static bool
dns_write_query(dns_buf_t *buf, const dns_name_t *name, uint16_t id)
{
    return
        dns_write_query_header(buf, id) &&
        dns_write_query_question(buf, name);
}

static bool
dns_resolve(const char *hostname, ip_addr_t *out_ip)
{
    bool ret = false;
    int sockfd = -1;
    dns_buf_t buf;

    /* DNS over UDP */
    sockfd = socket(SOCK_UDP);
    if (sockfd < 0) {
        goto cleanup;
    }

    /* Make socket non-blocking */
    if (fcntl(sockfd, FCNTL_NONBLOCK, 1) < 0) {
        goto cleanup;
    }

    /* Generate ID to match query with response */
    uint16_t id = urand() & 0xffff;

    /* Convert hostname from text to binary format */
    dns_name_t name;
    if (!dns_name_parse_text(hostname, &name)) {
        goto cleanup;
    }

    /* Emit query */
    buf.length = sizeof(buf.data);
    buf.offset = 0;
    if (!dns_write_query(&buf, &name, id)) {
        goto cleanup;
    }

    /* Send request */
    sock_addr_t addr = {.ip = DNS_SERVER, .port = DNS_PORT};
    if (sendto(sockfd, buf.data, buf.offset, &addr) < 0) {
        goto cleanup;
    }

    /* Compute timeout */
    int start = monotime();
    int end = start + DNS_TIMEOUT;

    /* Wait for response */
    while (monotime() < end) {
        int rcnt = recvfrom(sockfd, &buf.data, sizeof(buf.data), NULL);
        if (rcnt == -EINTR || rcnt == -EAGAIN) {
            continue;
        } else if (rcnt < 0) {
            goto cleanup;
        } else {
            buf.length = rcnt;
            buf.offset = 0;
            ret = dns_read_response(&buf, &name, id, out_ip);
            break;
        }
    }

cleanup:
    if (sockfd >= 0) close(sockfd);
    return ret;
}

static bool
ip_parse(const char *str, ip_addr_t *out_ip)
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
        out_ip->bytes[index] = octets[index];
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
     * Worst case scenario: ALL the characters are \n,
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

__attribute__((cdecl)) static void
sigint_handler(int signum)
{
    stop = true;
}

static int
nc_loop(ip_addr_t ip, uint16_t port, args_t *args)
{
    int ret = 1;
    int sockfd = -1;
    int listenfd = -1;
    char send_buf[TCP_MAX_LEN];
    char recv_buf[8192];
    int send_offset = 0;
    int recv_offset = 0;
    bool send_done = false;
    bool recv_done = false;
    bool bound = false;
    bool connected = false;
    sock_addr_t local_addr;
    sock_addr_t remote_addr;

#define CALL(expr) do {                                     \
    ret = expr;                                             \
    if (ret == -1) {                                        \
        fprintf(stderr, #expr " failed at %d\n", __LINE__); \
        ret = 1;                                            \
        goto cleanup;                                       \
    }                                                       \
} while (0)

    if (args->listen) {
        local_addr.ip = ip;
        local_addr.port = port;
        if (args->udp) {
            CALL(sockfd = socket(SOCK_UDP));
            CALL(bind(sockfd, &local_addr));
            CALL(fcntl(sockfd, FCNTL_NONBLOCK, 1));
            bound = true;
        } else {
            CALL(listenfd = socket(SOCK_TCP));
            CALL(bind(listenfd, &local_addr));
            CALL(listen(listenfd, 128));
            CALL(fcntl(listenfd, FCNTL_NONBLOCK, 1));
            bound = false;
        }
        connected = false;
    } else {
        remote_addr.ip = ip;
        remote_addr.port = port;
        if (args->udp) {
            CALL(sockfd = socket(SOCK_UDP));
            CALL(fcntl(sockfd, FCNTL_NONBLOCK, 1));
            bound = false;
        } else {
            CALL(sockfd = socket(SOCK_TCP));
            CALL(fcntl(sockfd, FCNTL_NONBLOCK, 1));
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
            CALL(fcntl(sockfd, FCNTL_NONBLOCK, 1));
            connected = true;
            bound = true;
        }

        /* Read outbound data from stdin */
        CALL(input(STDIN_FILENO, send_buf, sizeof(send_buf), &send_offset, args->crlf));

        /* If done reading from stdin, send a FIN in TCP mode */
        if (ret == 0 && send_offset == 0 && !args->udp && !send_done) {
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

            if (!connected && ret > 0) {
                CALL(connect(sockfd, &remote_addr));
                connected = true;
            }
        }

        /* Write inbound data to stdout */
        CALL(output(STDOUT_FILENO, recv_buf, &recv_offset));
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
        return true;
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
                    return false;
                }
            }
        } else {
            return true;
        }
    }
}

int
main(void)
{
    /* Set signal handler */
    if (sigaction(SIGINT, sigint_handler) < 0) {
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
    int orig_nonblock = fcntl(STDIN_FILENO, FCNTL_NONBLOCK, 1);
    if (orig_nonblock < 0) {
        fprintf(stderr, "Failed to make stdin non-blocking\n");
        return 1;
    }

    /* Run main loop */
    int ret = nc_loop(ip, port, &args);

    /* Restore original blocking mode */
    if (fcntl(STDIN_FILENO, FCNTL_NONBLOCK, orig_nonblock) < 0) {
        fprintf(stderr, "Failed to restore stdin blocking mode\n");
        return 1;
    }

    return ret;
}
