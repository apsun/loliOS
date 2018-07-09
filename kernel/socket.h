#ifndef _SOCKET_H
#define _SOCKET_H

#include "types.h"
#include "list.h"
#include "file.h"
#include "net.h"

#define SOCK_TCP 1
#define SOCK_UDP 2
#define SOCK_IP  3

#ifndef ASM

/* Forward declaration */
typedef struct sock_ops sock_ops_t;

/* Socket address */
typedef struct {
    ip_addr_t ip;
    uint16_t port;
} sock_addr_t;

/* Layer-4 UDP/TCP socket */
typedef struct {
    /* Used to maintain a global list of sockets in socket.c */
    list_t list;

    /* Socket operations table */
    const sock_ops_t *ops_table;

    /* Socket type (one of the SOCK_* constants) */
    int type;

    /* Reference count of this socket */
    int refcnt;

    /* Whether bind(), connect(), listen() have been called */
    bool bound     : 1;
    bool connected : 1;
    bool listening : 1;

    /* Local bound interface, or NULL if bound to all interfaces */
    net_iface_t *iface;

    /* Local (bound) and remote (connected) addresses */
    sock_addr_t local;
    sock_addr_t remote;

    /* Per-type private data */
    void *private;
} net_sock_t;

/*
 * Socket operations table. All functions are optional.
 */
struct sock_ops {
    int (*ctor)(net_sock_t *sock);
    void (*dtor)(net_sock_t *sock);
    int (*bind)(net_sock_t *sock, const sock_addr_t *addr);
    int (*connect)(net_sock_t *sock, const sock_addr_t *addr);
    int (*listen)(net_sock_t *sock, int backlog);
    int (*accept)(net_sock_t *sock, sock_addr_t *addr);
    int (*recvfrom)(net_sock_t *sock, void *buf, int nbytes, sock_addr_t *addr);
    int (*sendto)(net_sock_t *sock, const void *buf, int nbytes, const sock_addr_t *addr);
    int (*shutdown)(net_sock_t *sock);
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
__cdecl int socket_shutdown(int fd);
__cdecl int socket_getsockname(int fd, sock_addr_t *addr);
__cdecl int socket_getpeername(int fd, sock_addr_t *addr);

/* Returns the socket corresponding to the specified fd */
net_sock_t *get_executing_sock(int fd);

/* Socket object alloc/free/retain/release functions */
net_sock_t *socket_obj_alloc(int type);
void socket_obj_free(net_sock_t *sock);
net_sock_t *socket_obj_retain(net_sock_t *sock);
void socket_obj_release(net_sock_t *sock);

/* Binds a socket object to a file */
int socket_obj_bind_file(file_obj_t **files, net_sock_t *sock);

/* Finds a socket given a local (IP, port) combination */
net_sock_t *get_sock_by_local_addr(int type, ip_addr_t ip, uint16_t port);

/* Finds a socket given a local and remote (IP, port) combination */
net_sock_t *get_sock_by_addr(int type,
    ip_addr_t local_ip, uint16_t local_port,
    ip_addr_t remote_ip, uint16_t remote_port);

/* Binds a socket to the specified local (IP, port) combination */
int socket_bind_addr(net_sock_t *sock, ip_addr_t ip, uint16_t port);

/* Connects a socket to the specified remote (IP, port) combination */
int socket_connect_addr(net_sock_t *sock, ip_addr_t ip, uint16_t port);

#endif /* ASM */

#endif /* _SOCKET_H */
