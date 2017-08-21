#ifndef _LOLIBC_SYS_H
#define _LOLIBC_SYS_H

#define SYS_HALT        1
#define SYS_EXECUTE     2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_GETARGS     7
#define SYS_VIDMAP      8
#define SYS_SET_HANDLER 9
#define SYS_SIGRETURN   10
#define SYS_IOCTL       11
#define SYS_TIME        12

#define SIG_DIV_ZERO  0
#define SIG_SEGFAULT  1
#define SIG_INTERRUPT 2
#define SIG_ALARM     3
#define SIG_USER1     4

#ifndef ASM

#include "types.h"

int32_t halt(uint8_t status);
int32_t execute(const char *command);
int32_t read(int32_t fd, void *buf, int32_t nbytes);
int32_t write(int32_t fd, const void *buf, int32_t nbytes);
int32_t open(const char *filename);
int32_t close(int32_t fd);
int32_t getargs(char *buf, int32_t nbytes);
int32_t vidmap(uint8_t **screen_start);
int32_t set_handler(int32_t signum, void *handler);
int32_t sigreturn(void);
int32_t ioctl(int32_t fd, uint32_t req, uint32_t arg);
int32_t time(void);

#endif /* ASM */

#endif /* _LOLIBC_SYS_H */
