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

#define SIG_DIV_ZERO  0
#define SIG_SEGFAULT  1
#define SIG_INTERRUPT 2
#define SIG_ALARM     3
#define SIG_USER1     4

#define SIGMASK_NONE    0
#define SIGMASK_BLOCK   1
#define SIGMASK_UNBLOCK 2

#ifndef ASM

int halt(unsigned char status);
int execute(const char *command);
int read(int fd, void *buf, int nbytes);
int write(int fd, const void *buf, int nbytes);
int open(const char *filename);
int close(int fd);
int getargs(char *buf, int nbytes);
int vidmap(unsigned char **screen_start);
int sigaction(int signum, void *handler);
int sigreturn(void);
int sigraise(int signum);
int sigmask(int signum, int action);
int ioctl(int fd, int req, int arg);
int time(void);
int sbrk(int delta);

#endif /* ASM */

#endif /* _LOLIBC_SYSCALL_H */
