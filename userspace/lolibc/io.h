#ifndef _LOLIBC_IO_H
#define _LOLIBC_IO_H

#include "types.h"

void putc(char c);
void puts(const char *s);
char getc(void);
char *gets(char *buf, int32_t n);
void printf(const char *format, ...);

#endif /* _LOLIBC_IO_H */
