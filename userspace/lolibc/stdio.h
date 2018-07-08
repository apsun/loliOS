#ifndef _LOLIBC_STDIO_H
#define _LOLIBC_STDIO_H

#include <stdarg.h>

#define stdin 0
#define stdout 1
#define stderr 2

int fputc(char c, int fd);
int fputs(const char *s, int fd);
int putchar(char c);
int puts(const char *s);
char getchar(void);
char *gets(char *buf, int size);
int vsnprintf(char *buf, int size, const char *format, va_list args);
int snprintf(char *buf, int size, const char *format, ...);
int vfprintf(int fd, const char *format, va_list args);
int fprintf(int fd, const char *format, ...);
int vprintf(const char *format, va_list args);
int printf(const char *format, ...);

#endif /* _LOLIBC_STDIO_H */
