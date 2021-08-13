#ifndef _PRINTF_H
#define _PRINTF_H

#include "types.h"

#ifndef ASM

int vsnprintf(char *buf, int size, const char *format, va_list args);
int snprintf(char *buf, int size, const char *format, ...);
int vprintf(const char *format, va_list args);
int printf(const char *format, ...);

#endif /* ASM */

#endif /* _PRINTF_H */
