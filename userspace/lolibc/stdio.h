#ifndef _LOLIBC_STDIO_H
#define _LOLIBC_STDIO_H

#include <stdarg.h>

void putchar(char c);
void puts(const char *s);
char getchar(void);
char *gets(char *buf, int size);
int vsnprintf(char *buf, int size, const char *format, va_list args);
int snprintf(char *buf, int size, const char *format, ...);
int vprintf(const char *format, va_list args);
int printf(const char *format, ...);

#endif /* _LOLIBC_STDIO_H */
