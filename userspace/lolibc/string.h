#ifndef _LOLIBC_STRING_H
#define _LOLIBC_STRING_H

#include "types.h"

int32_t strlen(const char *s);
int32_t strcmp(const char *s1, const char *s2);
int32_t strncmp(const char *s1, const char *s2, int32_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int32_t n);
char *strrev(char *s);
char *strchr(const char *s, char c);
char *strrchr(const char *s, char c);
char *strstr(const char *haystack, const char *needle);
char *utoa(uint32_t value, char *buf, int32_t radix);
char *itoa(int32_t value, char *buf, int32_t radix);
int32_t atoi(const char *str);
int32_t memcmp(const void *s1, const void *s2, int32_t n);
void *memset(void *s, uint8_t c, int32_t n);
void *memcpy(void *dest, const void *src, int32_t n);
void *memmove(void *dest, const void *src, int32_t n);

#endif /* _LOLIBC_STRING_H */
