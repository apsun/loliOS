#ifndef _SOCKET_H
#define _SOCKET_H

#include "types.h"
#include "net.h"

#define SOCK_TCP 1
#define SOCK_UDP 2
#define SOCK_IP  3

#ifndef ASM

/* Forward declaration */
typedef struct sock_ops_t sock_ops_t;

/* Layer-4 UDP/TCP socket */
typedef struct net_sock_t {
    /* Socket operations table */
    const sock_ops_t *ops_table;

    /* Socket descriptor, -1 if the socket is unallocated */
    int sd;

    /* Whether the interface has been bound */
    bool bound;

    /* Local interface, or NULL if bound to all interfaces */
    net_iface_t *iface;

    /* Local bound port, or 0 if no bound port */
    int port;

    /* Socket type (one of the SOCK_* constants) */
    int type;

    /* Per-type private data */
    void *private;
} net_sock_t;

/* Socket address */
typedef struct {
    ip_addr_t ip;
    uint16_t port;
} sock_addr_t;

/* Socket operations table */
struct sock_ops_t {
    int (*socket)(net_sock_t *sock);
    int (*bind)(net_sock_t *sock, const sock_addr_t *addr);
    int (*connect)(net_sock_t *sock, const sock_addr_t *addr);
    int (*listen)(net_sock_t *sock, int backlog);
    int (*accept)(net_sock_t *sock, sock_addr_t *addr);
    int (*recvfrom)(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr);
    int (*sendto)(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr);
    int (*ioctl)(net_sock_t *sock, int req, int arg);
    int (*close)(net_sock_t *sock);
};

/* Socket syscall functions */
__cdecl int socket_socket(int type);
__cdecl int socket_bind(int fd, const sock_addr_t *addr);
__cdecl int socket_connect(int fd, const sock_addr_t *addr);
__cdecl int socket_listen(int fd, int backlog);
__cdecl int socket_accept(int fd, sock_addr_t *addr);
__cdecl int socket_recvfrom(int fd, void *buf, int nbytes, sock_addr_t *addr);
__cdecl int socket_sendto(int fd, const void *buf, int nbytes, const sock_addr_t *addr);

/* Finds a socket given a local (IP, port) combination */
net_sock_t *get_sock_by_addr(int type, ip_addr_t ip, uint16_t port);

/* Binds a socket to the specified local (IP, port) combination */
int socket_bind_addr(net_sock_t *sock, ip_addr_t ip, uint16_t port);

/* Initializes sockets */
void socket_init(void);

#endif /* ASM */

#endif /* _SOCKET_H */
