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

#include <stdint.h>

int32_t halt(uint8_t status);
int32_t execute(const char *command);
int32_t read(int32_t fd, void *buf, int32_t nbytes);
int32_t write(int32_t fd, const void *buf, int32_t nbytes);
int32_t open(const char *filename);
int32_t close(int32_t fd);
int32_t getargs(char *buf, int32_t nbytes);
int32_t vidmap(uint8_t **screen_start);
int32_t sigaction(int32_t signum, void *handler);
int32_t sigreturn(void);
int32_t sigraise(int32_t signum);
int32_t sigmask(int32_t signum, int32_t action);
int32_t ioctl(int32_t fd, uint32_t req, uint32_t arg);
int32_t time(void);
int32_t sbrk(int32_t delta);

#endif /* ASM */

#endif /* _LOLIBC_SYSCALL_H */
