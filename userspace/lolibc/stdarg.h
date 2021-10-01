#ifndef _LOLIBC_STDARG_H
#define _LOLIBC_STDARG_H

typedef __builtin_va_list va_list;
#define va_start(list, last) __builtin_va_start(list, last)
#define va_arg(list, T) __builtin_va_arg(list, T)
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#define va_end(list) __builtin_va_end(list)

#endif /* _LOLIBC_STDARG_H */
