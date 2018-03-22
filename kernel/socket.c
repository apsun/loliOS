#include "socket.h"
#include "lib.h"
#include "debug.h"
#include "file.h"
#include "net.h"
#include "udp.h"

/* Lowest port number used for random local port numbers */
#define EPHEMERAL_PORT_START 49152

/* Global socket list */
static net_sock_t socks[36];

/* File operations syscall forward declarations */
static int socket_open(const char *filename, file_obj_t *file);
static int socket_read(file_obj_t *file, void *buf, int nbytes);
static int socket_write(file_obj_t *file, const void *buf, int nbytes);
static int socket_close(file_obj_t *file);
static int socket_ioctl(file_obj_t *file, int req, int arg);

/* Network socket file ops */
static const file_ops_t fops_socket = {
    .open = socket_open,
    .read = socket_read,
    .write = socket_write,
    .close = socket_close,
    .ioctl = socket_ioctl,
};

/* UDP socket operations table */
static const sock_ops_t sops_udp = {
    .socket = udp_socket,
    .bind = udp_bind,
    .recvfrom = udp_recvfrom,
    .sendto = udp_sendto,
    .close = udp_close,
};

/*
 * Returns the socket for the given file object. Returns
 * null if the file is not a socket file.
 */
static net_sock_t *
get_sock(file_obj_t *file)
{
    if (file->ops_table != &fops_socket) {
        return NULL;
    }
    return &socks[file->private];
}

/*
 * Returns the socket corresponding to the given descriptor
 * for the currently executing process. Returns null if the
 * descriptor is invalid or does not correspond to a socket.
 */
static net_sock_t *
get_executing_sock(int fd)
{
    file_obj_t *file = get_executing_file(fd);
    if (file == NULL) {
        return NULL;
    }
    return get_sock(file);
}

/*
 * Initializes the specified socket's ops table for
 * the given socket type.
 */
static int
socket_obj_init(net_sock_t *sock, int type)
{
    switch (type) {
    case SOCK_UDP:
        sock->ops_table = &sops_udp;
        break;
    default:
        debugf("Unknown socket type: %d\n", type);
        return -1;
    }
    sock->type = type;
    return 0;
}

/*
 * Allocates a socket. This does not bind it with a file
 * object.
 */
static net_sock_t *
socket_obj_alloc(void)
{
    int i;
    for (i = 0; i < array_len(socks); ++i) {
        net_sock_t *sock = &socks[i];
        if (sock->sd < 0) {
            sock->sd = i;
            sock->bound = false;
            sock->iface = NULL;
            sock->port = 0;
            sock->type = 0;
            sock->private = NULL;
            return sock;
        }
    }
    return NULL;
}

/* Open syscall for socket files. This should never be called! */
static int
socket_open(const char *filename, file_obj_t *file)
{
    PANIC("Socket object open() called");
    return -1;
}

/* Read syscall for socket files. Wrapper around recvfrom(). */
static int
socket_read(file_obj_t *file, void *buf, int nbytes)
{
    return socket_recvfrom(file->fd, buf, nbytes, NULL);
}

/* Write syscall for socket files. Wrapper around sendto(). */
static int
socket_write(file_obj_t *file, const void *buf, int nbytes)
{
    return socket_sendto(file->fd, buf, nbytes, NULL);
}

/* Close syscall for socket files. Frees the associated socket. */
static int
socket_close(file_obj_t *file)
{
    net_sock_t *sock = get_sock(file);
    ASSERT(sock != NULL);
    if (sock->ops_table->close != NULL &&
        sock->ops_table->close(sock) < 0) {
        return -1;
    }
    sock->sd = -1;
    return 0;
}

/* Ioctl syscall for socket files. */
static int
socket_ioctl(file_obj_t *file, int req, int arg)
{
    net_sock_t *sock = get_sock(file);
    ASSERT(sock != NULL);
    if (sock->ops_table->ioctl == NULL) {
        return -1;
    }
    return sock->ops_table->ioctl(sock, req, arg);
}

/* Socket syscall handler */
__cdecl int
socket_socket(int type)
{
    /* Allocate a socket */
    net_sock_t *sock = socket_obj_alloc();
    if (sock == NULL) {
        debugf("Failed to allocate socket\n");
        return -1;
    }

    /* Allocate a file object */
    file_obj_t *file = file_obj_alloc();
    if (file == NULL) {
        debugf("Failed to allocate file\n");
        sock->sd = -1;
        return -1;
    }

    /* Initialize socket and file objects */
    if (socket_obj_init(sock, type) < 0) {
        debugf("Failed to initialize socket\n");
        file->fd = -1;
        sock->sd = -1;
        return -1;
    }

    /* Call type-specific constructor, if any */
    if (sock->ops_table->socket != NULL &&
        sock->ops_table->socket(sock) < 0) {
        debugf("Socket constructor returned error\n");
        file->fd = -1;
        sock->sd = -1;
        return -1;
    }

    file->ops_table = &fops_socket;
    file->private = sock->sd;
    return file->fd;
}

/*
 * Generates the body of the syscall handlers.
 * Will return -1 if the corresponding function
 * is not implemented; otherwise will delegate
 * to it.
 */
#define FORWARD_SOCKETCALL(fn, ...) do {               \
    net_sock_t *sock = get_executing_sock(fd);         \
    if (sock == NULL) {                                \
        debugf("Not a socket file\n");                 \
        return -1;                                     \
    }                                                  \
    if (sock->ops_table->fn == NULL) {                 \
        debugf("Socket: %s() not implemented\n", #fn); \
        return -1;                                     \
    }                                                  \
    return sock->ops_table->fn(sock, __VA_ARGS__);     \
} while (0)

/* Bind syscall handler */
__cdecl int
socket_bind(int fd, const sock_addr_t *addr)
{
    FORWARD_SOCKETCALL(bind, addr);
}

/* Connect syscall handler */
__cdecl int
socket_connect(int fd, const sock_addr_t *addr)
{
    FORWARD_SOCKETCALL(connect, addr);
}

/* Listen syscall handler */
__cdecl int
socket_listen(int fd, int backlog)
{
    FORWARD_SOCKETCALL(listen, backlog);
}

/* Accept syscall handler */
__cdecl int
socket_accept(int fd, sock_addr_t *addr)
{
    FORWARD_SOCKETCALL(accept, addr);
}

/* Recvfrom syscall handler */
__cdecl int
socket_recvfrom(int fd, void *buf, int nbytes, sock_addr_t *addr)
{
    FORWARD_SOCKETCALL(recvfrom, buf, nbytes, addr);
}

/* Sendto syscall handler */
__cdecl int
socket_sendto(int fd, const void *buf, int nbytes, const sock_addr_t *addr)
{
    FORWARD_SOCKETCALL(sendto, buf, nbytes, addr);
}

/*
 * Checks whether a socket's bound address matches the
 * specified (IP, port) combination.
 */
static bool
socket_addr_matches(net_sock_t *sock, int type, ip_addr_t ip, int port)
{
    if (sock->sd < 0)
        return false;
    if (!sock->bound)
        return false;
    if (sock->type != type)
        return false;
    if (sock->port != port)
        return false;
    if (sock->iface == NULL)
        return true;
    if (ip_equals(ip, ANY_IP))
        return true;
    return ip_equals(sock->iface->ip_addr, ip);
}

/*
 * Returns a socket given the bound IP address and port.
 * If there is no socket for the (IP, port) combination,
 * returns null.
 */
net_sock_t *
get_sock_by_addr(int type, ip_addr_t ip, uint16_t port)
{
    int i;
    for (i = 0; i < array_len(socks); ++i) {
        net_sock_t *sock = &socks[i];
        if (socket_addr_matches(sock, type, ip, port)) {
            return sock;
        }
    }
    return NULL;
}

/*
 * Finds a free port. By the pidgeonhole principle, if we just
 * try array_len(socks) + 1 distinct ports, there must be one
 * that is unused. Since we only allow a limited number of sockets,
 * this algorithm is sufficient.
 */
static int
socket_find_free_port(net_iface_t *iface, int type)
{
    ip_addr_t ip = ANY_IP;
    if (iface != NULL) {
        ip = iface->ip_addr;
    }

    int i;
    for (i = 0; i <= array_len(socks); ++i) {
        int port = EPHEMERAL_PORT_START + i;
        if (get_sock_by_addr(type, ip, port) == NULL) {
            return port;
        }
    }

    PANIC("Failed to find a free port for binding");
    return -1;
}

/*
 * Binds a socket to the specified (IP, port) combination.
 * Returns 0 on success, and -1 if the address/port is invalid
 * or is already bound to another socket. The IP may be set
 * to ANY_IP (0.0.0.0) to bind to all interfaces, and the port
 * may be set to 0 to automatically choose a free port. This
 * does NOT check whether the socket is already bound; to
 * prevent re-binding, don't call this function.
 */
int
socket_bind_addr(net_sock_t *sock, ip_addr_t ip, uint16_t port)
{
    /* Validate IP address */
    net_iface_t *iface = NULL;
    if (!ip_equals(ip, ANY_IP)) {
        iface = net_find(ip);
        if (iface == NULL) {
            debugf("Couldn't find interface for given IP address\n");
            return -1;
        }
    }

    /* If port is 0, pick one at random */
    if (port == 0) {
        port = socket_find_free_port(iface, sock->type);
    }

    /* Check for collisions */
    int i;
    for (i = 0; i < array_len(socks); ++i) {
        net_sock_t *tmp = &socks[i];
        if (tmp != sock && socket_addr_matches(tmp, sock->type, ip, port)) {
            debugf("Address already bound\n");
            return -1;
        }
    }

    sock->bound = true;
    sock->iface = iface;
    sock->port = port;
    return 0;
}

/* Initializes all sockets to an invalid state */
void
socket_init(void)
{
    int i;
    for (i = 0; i < array_len(socks); ++i) {
        socks[i].sd = -1;
    }
}
