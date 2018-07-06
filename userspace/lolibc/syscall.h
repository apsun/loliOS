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
#define SYS_SIGMASK     11
#define SYS_KILL        12
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
#define SYS_GETSOCKNAME 23
#define SYS_GETPEERNAME 24
#define SYS_DUP         25
#define SYS_FORK        26
#define SYS_EXEC        27
#define SYS_WAIT        28
#define SYS_GETPID      29
#define SYS_GETPGRP     30
#define SYS_SETPGRP     31
#define SYS_TCGETPGRP   32
#define SYS_TCSETPGRP   33
#define SYS_PIPE        34

/* syscall.h */
#define EINTR  2
#define EAGAIN 3

/* signal.h */
#define SIG_DIV_ZERO  0
#define SIG_SEGFAULT  1
#define SIG_INTERRUPT 2
#define SIG_ALARM     3
#define SIG_USER1     4

/* signal.h */
#define SIGMASK_NONE    0
#define SIGMASK_BLOCK   1
#define SIGMASK_UNBLOCK 2

/* socket.h */
#define SOCK_TCP 1
#define SOCK_UDP 2
#define SOCK_IP  3

/* terminal.h */
#define STDIN_NONBLOCK 1

/* sb16.h */
#define SOUND_SET_BITS_PER_SAMPLE 1
#define SOUND_SET_NUM_CHANNELS 2
#define SOUND_SET_SAMPLE_RATE 3

#ifndef ASM

#include <stdint.h>

/* net.h */
typedef struct {
    uint8_t bytes[4];
} ip_addr_t;

/* net.h */
typedef struct {
    ip_addr_t ip;
    uint16_t port;
} sock_addr_t;

/* net.h */
#define IP(a, b, c, d) ((ip_addr_t){.bytes = {(a), (b), (c), (d)}})

#define __cdecl __attribute__((cdecl))
__cdecl int halt(int status);
__cdecl int execute(const char *command);
__cdecl int read(int fd, void *buf, int nbytes);
__cdecl int write(int fd, const void *buf, int nbytes);
__cdecl int open(const char *filename);
__cdecl int close(int fd);
__cdecl int getargs(char *buf, int nbytes);
__cdecl int vidmap(uint8_t **screen_start);
__cdecl int sigaction(int signum, void (*handler)(int signum));
__cdecl int sigreturn(int signum, void *user_regs);
__cdecl int sigmask(int signum, int action);
__cdecl int kill(int pid, int signum);
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
__cdecl int getsockname(int fd, sock_addr_t *addr);
__cdecl int getpeername(int fd, sock_addr_t *addr);
__cdecl int dup(int srcfd, int destfd);
__cdecl int fork(void);
__cdecl int exec(const char *command);
__cdecl int wait(int *pid);
__cdecl int getpid(void);
__cdecl int getpgrp(void);
__cdecl int setpgrp(int pid, int pgrp);
__cdecl int tcgetpgrp(void);
__cdecl int tcsetpgrp(int pgrp);
__cdecl int pipe(int *readfd, int *writefd);

#endif /* ASM */

#endif /* _LOLIBC_SYSCALL_H */
