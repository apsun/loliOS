#include "socket.h"
#include "lib.h"
#include "debug.h"
#include "myalloc.h"
#include "file.h"
#include "net.h"
#include "tcp.h"
#include "udp.h"

/* Lowest port number used for random local port numbers */
#define EPHEMERAL_PORT_START 49152
#define MAX_PORT 65535

/* Global list of sockets */
static list_declare(socket_list);

/* File operations syscall forward declarations */
static int socket_read(file_obj_t *file, void *buf, int nbytes);
static int socket_write(file_obj_t *file, const void *buf, int nbytes);
static int socket_close(file_obj_t *file);
static int socket_ioctl(file_obj_t *file, int req, int arg);

/* Network socket file ops */
static const file_ops_t fops_socket = {
    .open = NULL, /* Can never be reached */
    .read = socket_read,
    .write = socket_write,
    .close = socket_close,
    .ioctl = socket_ioctl,
};

/* UDP socket operations table */
static const sock_ops_t sops_udp = {
    .ctor = udp_ctor,
    .dtor = udp_dtor,
    .bind = udp_bind,
    .connect = udp_connect,
    .recvfrom = udp_recvfrom,
    .sendto = udp_sendto,
};

/* TCP socket operations table */
static const sock_ops_t sops_tcp = {
    .ctor = tcp_ctor,
    .dtor = tcp_dtor,
    .bind = tcp_bind,
    .connect = tcp_connect,
    .listen = tcp_listen,
    .accept = tcp_accept,
    .recvfrom = tcp_recvfrom,
    .sendto = tcp_sendto,
    .close = tcp_close,
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
    return file->private;
}

/*
 * Returns the socket corresponding to the given descriptor
 * for the currently executing process. Returns null if the
 * descriptor is invalid or does not correspond to a socket.
 */
net_sock_t *
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
    case SOCK_TCP:
        sock->ops_table = &sops_tcp;
        break;
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
 * Allocates and initializes a socket. This does not bind
 * it with a file object. The type should be one of the
 * SOCK_* constants.
 */
net_sock_t *
socket_obj_alloc(int type)
{
    /* Allocate socket */
    net_sock_t *sock = malloc(sizeof(net_sock_t));
    if (sock == NULL) {
        return NULL;
    }

    /* Set socket ops table */
    if (socket_obj_init(sock, type) < 0) {
        free(sock);
        return NULL;
    }

    /* Initialize fields */
    sock->refcnt = 1;
    sock->bound = false;
    sock->connected = false;
    sock->listening = false;
    sock->iface = NULL;
    sock->local.ip = ANY_IP;
    sock->local.port = 0;
    sock->remote.ip = ANY_IP;
    sock->remote.port = 0;
    sock->private = NULL;
    list_add(&sock->list, &socket_list);

    /* Call constructor */
    if (sock->ops_table->ctor != NULL && sock->ops_table->ctor(sock) < 0) {
        list_del(&sock->list);
        free(sock);
        return NULL;
    }

    return sock;
}

/*
 * Increments the reference count of a socket.
 */
net_sock_t *
socket_obj_retain(net_sock_t *sock)
{
    assert(sock->refcnt > 0);
    sock->refcnt++;
    return sock;
}

/*
 * Decrements the reference count of a socket, and frees it
 * once the reference count reaches zero.
 */
void
socket_obj_release(net_sock_t *sock)
{
    assert(sock->refcnt > 0);
    if (--sock->refcnt == 0) {
        if (sock->ops_table->dtor != NULL) {
            sock->ops_table->dtor(sock);
        }
        list_del(&sock->list);
        free(sock);
    }
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

/* Close syscall for socket files. */
static int
socket_close(file_obj_t *file)
{
    net_sock_t *sock = get_sock(file);
    assert(sock != NULL);
    if (sock->ops_table->close != NULL && sock->ops_table->close(sock) < 0) {
        return -1;
    }
    socket_obj_release(sock);
    return 0;
}

/* Ioctl syscall for socket files. */
static int
socket_ioctl(file_obj_t *file, int req, int arg)
{
    net_sock_t *sock = get_sock(file);
    assert(sock != NULL);
    if (sock->ops_table->ioctl == NULL) {
        return -1;
    }
    return sock->ops_table->ioctl(sock, req, arg);
}

/* Socket syscall handler */
__cdecl int
socket_socket(int type)
{
    /* Allocate a file object */
    file_obj_t *file = file_obj_alloc();
    if (file == NULL) {
        debugf("Failed to allocate file\n");
        return -1;
    }

    /* Allocate and initialize socket */
    net_sock_t *sock = socket_obj_alloc(type);
    if (sock == NULL) {
        debugf("Failed to allocate socket\n");
        file_obj_free(file);
        return -1;
    }

    file->ops_table = &fops_socket;
    file->private = sock;
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
 * Checks whether a socket's bound local address matches
 * the specified (IP, port) tuple.
 */
static bool
socket_local_addr_matches(net_sock_t *sock, int type, ip_addr_t ip, uint16_t port)
{
    if (!sock->bound)
        return false;
    if (sock->type != type)
        return false;
    if (sock->local.port != port)
        return false;
    if (sock->iface == NULL)
        return true;
    if (ip_equals(ip, ANY_IP))
        return true;
    return ip_equals(sock->local.ip, ip);
}

/*
 * Checks whether a socket's bound and connected addresses
 * matches the specified (IP, port) combinations. If remote_ip
 * equals ANY_IP and remote_port equals 0, this will match
 * only unconnected sockets.
 */
static bool
socket_addr_matches(
    net_sock_t *sock, int type,
    ip_addr_t local_ip, uint16_t local_port,
    ip_addr_t remote_ip, uint16_t remote_port)
{
    if (!socket_local_addr_matches(sock, type, local_ip, local_port))
        return false;
    if (!sock->connected)
        return ip_equals(remote_ip, ANY_IP) && remote_port == 0;
    if (!ip_equals(sock->remote.ip, remote_ip))
        return false;
    if (sock->remote.port != remote_port)
        return false;
    return true;
}

/*
 * Returns a socket given both the local and remote IP address
 * and port. If there is no socket for the specified (IP, port)
 * combinations, returns null.
 */
net_sock_t *
get_sock_by_addr(
    int type,
    ip_addr_t local_ip, uint16_t local_port,
    ip_addr_t remote_ip, uint16_t remote_port)
{
    list_t *pos;
    list_for_each(pos, &socket_list) {
        net_sock_t *sock = list_entry(pos, net_sock_t, list);
        if (socket_addr_matches(sock, type, local_ip, local_port, remote_ip, remote_port)) {
            return sock;
        }
    }
    return NULL;
}

/*
 * Returns a socket given the local IP address and port.
 * If there is no socket for the (IP, port) combination,
 * returns null.
 */
net_sock_t *
get_sock_by_local_addr(int type, ip_addr_t ip, uint16_t port)
{
    list_t *pos;
    list_for_each(pos, &socket_list) {
        net_sock_t *sock = list_entry(pos, net_sock_t, list);
        if (socket_local_addr_matches(sock, type, ip, port)) {
            return sock;
        }
    }
    return NULL;
}

/*
 * Finds a free port. Returns 0 if no ports are free.
 */
static uint16_t
socket_find_free_port(net_iface_t *iface, int type)
{
    ip_addr_t ip = ANY_IP;
    if (iface != NULL) {
        ip = iface->ip_addr;
    }

    /*
     * A simple bitmap doesn't work here since we can have
     * two sockets listening on different interfaces but the
     * same port. We can also have two sockets on the same
     * interface, but one TCP and one UDP, with the same port.
     * Hence, the slow algorithm it is!
     */
    int start_port = rand() % (MAX_PORT - EPHEMERAL_PORT_START + 1) + EPHEMERAL_PORT_START;
    int port = start_port;
    do {
        /* Try this port */
        if (get_sock_by_local_addr(type, ip, port) == NULL) {
            return port;
        }

        /* Port already taken, try the next one */
        if (++port > MAX_PORT) {
            port = EPHEMERAL_PORT_START;
        }
    } while (port != start_port);

    /* All ports exhausted */
    return 0;
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
        if (port == 0) {
            debugf("All ports already in use\n");
            return -1;
        }
    }

    /* Check for collisions */
    net_sock_t *existing = get_sock_by_local_addr(sock->type, ip, port);
    if (existing != NULL && existing != sock) {
        debugf("Address already bound\n");
        return -1;
    }

    sock->bound = true;
    sock->iface = iface;
    sock->local.ip = ip;
    sock->local.port = port;
    return 0;
}

/*
 * Connects a socket to the specified remote (IP, port).
 * Returns 0 on success, -1 if the destination IP address
 * is not routable or the port is invalid. This does not
 * prevent re-connecting a connected socket.
 */
int
socket_connect_addr(net_sock_t *sock, ip_addr_t ip, uint16_t port)
{
    /* Check remote port is valid */
    if (port == 0) {
        return -1;
    }

    /* Ensure we can actually route to destination */
    ip_addr_t neigh_ip;
    net_iface_t *iface = net_route(sock->iface, ip, &neigh_ip);
    if (iface == NULL) {
        debugf("Destination address not routable\n");
        return -1;
    }

    sock->connected = true;
    sock->remote.ip = ip;
    sock->remote.port = port;
    return 0;
}
