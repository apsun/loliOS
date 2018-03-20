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

typedef struct {
    uint8_t bytes[4];
} ip_addr_t;

typedef struct {
    ip_addr_t ip;
    uint16_t port;
} sock_addr_t;

int halt(int status);
int execute(const char *command);
int read(int fd, void *buf, int nbytes);
int write(int fd, const void *buf, int nbytes);
int open(const char *filename);
int close(int fd);
int getargs(char *buf, int nbytes);
int vidmap(uint8_t **screen_start);
int sigaction(int signum, void *handler);
int sigreturn(void);
int sigraise(int signum);
int sigmask(int signum, int action);
int ioctl(int fd, int req, int arg);
int time(void);
int sbrk(int delta);
int socket(int type);
int bind(int fd, const sock_addr_t *addr);
int connect(int fd, const sock_addr_t *addr);
int listen(int fd, int backlog);
int accept(int fd, sock_addr_t *addr);
int recvfrom(int fd, void *buf, int nbytes, sock_addr_t *addr);
int sendto(int fd, const void *buf, int nbytes, const sock_addr_t *addr);

#endif /* ASM */

#endif /* _LOLIBC_SYSCALL_H */
