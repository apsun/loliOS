#ifndef _LOLIBC_SYSCALL_H
#define _LOLIBC_SYSCALL_H

#define SYS_HALT        1
#define SYS_EXECUTE     2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_GETARGS     7
#define SYS_VIDMAP      8
#define SYS_SIGACTION   9
#define SYS_SIGRETURN   10
#define SYS_SIGRAISE    11
#define SYS_SIGMASK     12
#define SYS_IOCTL       13
#define SYS_TIME        14
#define SYS_SBRK        15
#define SYS_SOCKET      16
#define SYS_BIND        17
#define SYS_CONNECT     18
#define SYS_LISTEN      19
#define SYS_ACCEPT      20
#define SYS_RECVFROM    21
#define SYS_SENDTO      22

#define EINTR  2
#define EAGAIN 3

#define SIG_DIV_ZERO  0
#define SIG_SEGFAULT  1
#define SIG_INTERRUPT 2
#define SIG_ALARM     3
#define SIG_USER1     4

#define SIGMASK_NONE    0
#define SIGMASK_BLOCK   1
#define SIGMASK_UNBLOCK 2

#define SOCK_TCP 1
#define SOCK_UDP 2
#define SOCK_IP  3

#ifndef ASM

#include <stdint.h>

#define __cdecl __attribute__((cdecl))

typedef struct {
    uint8_t bytes[4];
} ip_addr_t;

typedef struct {
    ip_addr_t ip;
    uint16_t port;
} sock_addr_t;

__cdecl int halt(int status);
__cdecl int execute(const char *command);
__cdecl int read(int fd, void *buf, int nbytes);
__cdecl int write(int fd, const void *buf, int nbytes);
__cdecl int open(const char *filename);
__cdecl int close(int fd);
__cdecl int getargs(char *buf, int nbytes);
__cdecl int vidmap(uint8_t **screen_start);
__cdecl int sigaction(int signum, void *handler);
__cdecl int sigreturn(void);
__cdecl int sigraise(int signum);
__cdecl int sigmask(int signum, int action);
__cdecl int ioctl(int fd, int req, int arg);
__cdecl int time(void);
__cdecl int sbrk(int delta);
__cdecl int socket(int type);
__cdecl int bind(int fd, const sock_addr_t *addr);
__cdecl int connect(int fd, const sock_addr_t *addr);
__cdecl int listen(int fd, int backlog);
__cdecl int accept(int fd, sock_addr_t *addr);
__cdecl int recvfrom(int fd, void *buf, int nbytes, sock_addr_t *addr);
__cdecl int sendto(int fd, const void *buf, int nbytes, const sock_addr_t *addr);

#endif /* ASM */

#endif /* _LOLIBC_SYSCALL_H */