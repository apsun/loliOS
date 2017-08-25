#ifndef _LOLIBC_STDIO_H
#define _LOLIBC_STDIO_H

#include <stdint.h>
#include <stdarg.h>

void putc(char c);
void puts(const char *s);
char getc(void);
char *gets(char *buf, int32_t n);
int32_t vsnprintf(char *buf, int32_t size, const char *format, va_list args);
int32_t snprintf(char *buf, int32_t size, const char *format, ...);
int32_t vprintf(const char *format, va_list args);
int32_t printf(const char *format, ...);

#endif /* _LOLIBC_STDIO_H */
