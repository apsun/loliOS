#ifndef _LOLIBC_ARG
#define _LOLIBC_ARG

typedef char *va_list;
#define __va_align(x) (((uint32_t)(x) + 3) & ~3)
#define va_start(list, last) ((list) = (char *)__va_align(&(last) + 1))
#define va_arg(list, T) ((list) += sizeof(T), *(T *)((list) - sizeof(T)))
#define va_copy(dest, src) ((dest) = (src))
#define va_end(list) ((void)0)

#endif /* _LOLIBC_ARG */
