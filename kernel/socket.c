#include "socket.h"
#include "lib.h"
#include "debug.h"
#include "file.h"
#include "net.h"
#include "udp.h"

/* Global socket list (at most 6 for each process) */
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
    .bind = udp_bind,
    .recvfrom = udp_recvfrom,
    .sendto = udp_sendto,
};

/*
 * Initializes the specified socket's ops table for
 * the given socket type.
 */
static int
socket_init(net_sock_t *sock, int type)
{
    switch (type) {
    case SOCK_UDP:
        sock->ops_table = &sops_udp;
        break;
    default:
        debugf("Unknown socket type: %d\n", type);
        return -1;
    }
    return 0;
}

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
 * Allocates a socket. This does not bind it with a file
 * object.
 */
static net_sock_t *
socket_alloc(void)
{
    int i;
    for (i = 0; i < array_len(socks); ++i) {
        net_sock_t *sock = &socks[i];
        if (sock->sd < 0) {
            sock->sd = i;
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
    net_sock_t *sock = socket_alloc();
    if (sock == NULL) {
        return -1;
    }

    /* Allocate a file object */
    file_obj_t *file = file_obj_alloc();
    if (file == NULL) {
        sock->sd = -1;
        return -1;
    }

    /* Initialize socket and file objects */
    if (socket_init(sock, type) < 0) {
        file->fd = -1;
        sock->sd = -1;
        return -1;
    }

    /* Call type-specific constructor, if any */
    if (sock->ops_table->socket != NULL &&
        sock->ops_table->socket(sock) < 0) {
        file->fd = -1;
        sock->sd = -1;
        return -1;
    }

    file->ops_table = &fops_socket;
    file->private = sock->sd;
    return 0;
}

/*
 * Generates the body of the syscall handlers.
 * Will return -1 if the corresponding function
 * is not implemented; otherwise will delegate
 * to it.
 */
#define FORWARD_OPS(fn, ...) do {                  \
    net_sock_t *sock = get_executing_sock(fd);     \
    if (sock == NULL) {                            \
        return -1;                                 \
    }                                              \
    if (sock->ops_table->fn == NULL) {             \
        return -1;                                 \
    }                                              \
    return sock->ops_table->fn(sock, __VA_ARGS__); \
} while (0)

/* Bind syscall handler */
__cdecl int
socket_bind(int fd, const sock_addr_t *addr)
{
    FORWARD_OPS(bind, addr);
}

/* Connect syscall handler */
__cdecl int
socket_connect(int fd, const sock_addr_t *addr)
{
    FORWARD_OPS(connect, addr);
}

/* Listen syscall handler */
__cdecl int
socket_listen(int fd, int backlog)
{
    FORWARD_OPS(listen, backlog);
}

/* Accept syscall handler */
__cdecl int
socket_accept(int fd, sock_addr_t *addr)
{
    FORWARD_OPS(accept, addr);
}

/* Recvfrom syscall handler */
__cdecl int
socket_recvfrom(int fd, void *buf, int nbytes, sock_addr_t *addr)
{
    FORWARD_OPS(recvfrom, buf, nbytes, addr);
}

/* Sendto syscall handler */
__cdecl int
socket_sendto(int fd, const void *buf, int nbytes, const sock_addr_t *addr)
{
    FORWARD_OPS(sendto, buf, nbytes, addr);
}
