#ifndef _LOLIBC_STDARG_H
#define _LOLIBC_STDARG_H

typedef char *va_list;
#define va_start(list, last) ((list) = (char *)(&(last) + 1))
#define va_arg(list, T) ((list) += sizeof(T), *(T *)((list) - sizeof(T)))
#define va_copy(dest, src) ((dest) = (src))
#define va_end(list) ((void)0)

#endif /* _LOLIBC_STDARG_H */
