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
#define SYS_SBRK        15
#define SYS_SOCKET      16
#define SYS_BIND        17
#define SYS_CONNECT     18
#define SYS_LISTEN      19
#define SYS_ACCEPT      20
#define SYS_RECVFROM    21
#define SYS_SENDTO      22
#define SYS_SHUTDOWN    23
#define SYS_GETSOCKNAME 24
#define SYS_GETPEERNAME 25
#define SYS_DUP         26
#define SYS_FORK        27
#define SYS_EXEC        28
#define SYS_WAIT        29
#define SYS_GETPID      30
#define SYS_GETPGRP     31
#define SYS_SETPGRP     32
#define SYS_TCGETPGRP   33
#define SYS_TCSETPGRP   34
#define SYS_PIPE        35
#define SYS_CREATE      36
#define SYS_FCNTL       37
#define SYS_YIELD       38
#define SYS_SEEK        39
#define SYS_TRUNCATE    40
#define SYS_UNLINK      41
#define SYS_STAT        42
#define SYS_REALTIME    43
#define SYS_MONOTIME    44
#define SYS_MONOSLEEP   45
#define SYS_FBMAP       46
#define SYS_FBUNMAP     47
#define SYS_FBFLIP      48
#define NUM_SYSCALL     48

#ifndef ASM

#include <stdint.h>

/* syscall.h */
#define EINTR  2
#define EAGAIN 3
#define EPIPE  4

/* signal.h */
#define SIGFPE 0
#define SIGSEGV 1
#define SIGINT 2
#define SIGALRM 3
#define SIGUSR1 4
#define SIGKILL 5
#define SIGPIPE 6
#define SIGABRT 7

/* signal.h */
#define SIGMASK_NONE    0
#define SIGMASK_BLOCK   1
#define SIGMASK_UNBLOCK 2

/* signal.h */
typedef void (__attribute__((cdecl)) *sighandler_t)(int);
#define SIG_IGN ((sighandler_t)1)
#define SIG_DFL ((sighandler_t)0)

/* socket.h */
#define SOCK_TCP 1
#define SOCK_UDP 2
#define SOCK_IP  3

/* sb16.h */
#define SOUND_SET_BITS_PER_SAMPLE 1
#define SOUND_SET_NUM_CHANNELS 2
#define SOUND_SET_SAMPLE_RATE 3

/* file.h */
#define FILE_TYPE_RTC 0
#define FILE_TYPE_DIR 1
#define FILE_TYPE_FILE 2
#define FILE_TYPE_MOUSE 3
#define FILE_TYPE_TAUX 4
#define FILE_TYPE_SOUND 5
#define FILE_TYPE_TTY 6
#define FILE_TYPE_NULL 7
#define FILE_TYPE_ZERO 8
#define FILE_TYPE_RANDOM 9

/* file.h */
#define OPEN_NONE 0
#define OPEN_READ (1 << 0)
#define OPEN_WRITE (1 << 1)
#define OPEN_RDWR (OPEN_READ | OPEN_WRITE)
#define OPEN_CREATE (1 << 2)
#define OPEN_TRUNC (1 << 3)
#define OPEN_APPEND (1 << 4)

/* file.h */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* file.h */
#define FCNTL_NONBLOCK 1

/* net.h */
typedef struct {
    uint8_t bytes[4];
} ip_addr_t;

/* net.h */
typedef struct {
    ip_addr_t ip;
    uint16_t port;
} sock_addr_t;

/* file.h */
typedef struct {
    int type;
    int length;
} stat_t;

/* net.h */
#define IP(a, b, c, d) ((ip_addr_t){.bytes = {(a), (b), (c), (d)}})

__attribute__((cdecl, noreturn)) void halt(int status);
__attribute__((cdecl)) int execute(const char *command);
__attribute__((cdecl)) int read(int fd, void *buf, int nbytes);
__attribute__((cdecl)) int write(int fd, const void *buf, int nbytes);
__attribute__((cdecl)) int open(const char *filename);
__attribute__((cdecl)) int close(int fd);
__attribute__((cdecl)) int getargs(char *buf, int nbytes);
__attribute__((cdecl)) int vidmap(uint8_t **screen_start);
__attribute__((cdecl)) int sigaction(int signum, sighandler_t handler);
__attribute__((cdecl)) int sigreturn(int signum, void *user_regs);
__attribute__((cdecl)) int sigmask(int signum, int action);
__attribute__((cdecl)) int kill(int pid, int signum);
__attribute__((cdecl)) int ioctl(int fd, int req, intptr_t arg);
__attribute__((cdecl)) int sbrk(int delta, void **orig_brk);
__attribute__((cdecl)) int socket(int type);
__attribute__((cdecl)) int bind(int fd, const sock_addr_t *addr);
__attribute__((cdecl)) int connect(int fd, const sock_addr_t *addr);
__attribute__((cdecl)) int listen(int fd, int backlog);
__attribute__((cdecl)) int accept(int fd, sock_addr_t *addr);
__attribute__((cdecl)) int recvfrom(int fd, void *buf, int nbytes, sock_addr_t *addr);
__attribute__((cdecl)) int sendto(int fd, const void *buf, int nbytes, const sock_addr_t *addr);
__attribute__((cdecl)) int shutdown(int fd);
__attribute__((cdecl)) int getsockname(int fd, sock_addr_t *addr);
__attribute__((cdecl)) int getpeername(int fd, sock_addr_t *addr);
__attribute__((cdecl)) int dup(int srcfd, int destfd);
__attribute__((cdecl)) int fork(void);
__attribute__((cdecl)) int exec(const char *command);
__attribute__((cdecl)) int wait(int *pid);
__attribute__((cdecl)) int getpid(void);
__attribute__((cdecl)) int getpgrp(void);
__attribute__((cdecl)) int setpgrp(int pid, int pgrp);
__attribute__((cdecl)) int tcgetpgrp(void);
__attribute__((cdecl)) int tcsetpgrp(int pgrp);
__attribute__((cdecl)) int pipe(int *readfd, int *writefd);
__attribute__((cdecl)) int create(const char *filename, int mode);
__attribute__((cdecl)) int fcntl(int fd, int req, intptr_t arg);
__attribute__((cdecl)) int yield(void);
__attribute__((cdecl)) int seek(int fd, int offset, int mode);
__attribute__((cdecl)) int truncate(int fd, int length);
__attribute__((cdecl)) int unlink(const char *filename);
__attribute__((cdecl)) int stat(const char *filename, stat_t *buf);
__attribute__((cdecl)) int realtime(void);
__attribute__((cdecl)) int monotime(void);
__attribute__((cdecl)) int monosleep(int target);
__attribute__((cdecl)) int fbmap(void **ptr, int xres, int yres, int bpp);
__attribute__((cdecl)) int fbunmap(void *ptr);
__attribute__((cdecl)) int fbflip(void *ptr);

#endif /* ASM */

#endif /* _LOLIBC_SYSCALL_H */
