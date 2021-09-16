#ifndef _STRING_H
#define _STRING_H

#include "types.h"

#ifndef ASM

int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int n);
char *strcpy(char *dest, const char *src);
char *stpcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int n);
int strscpy(char *dest, const char *src, int n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, int n);
char *strrev(char *s);
char *strchr(const char *s, char c);
char *strrchr(const char *s, char c);
char *strstr(const char *haystack, const char *needle);
int strspn(const char *s, const char *needle);
int strcspn(const char *s, const char *needle);
char *strpbrk(const char *s, const char *needle);
char *strtok(char *s, const char *delim);
char *strsep(char **sp, const char *delim);
char *utoa(unsigned int value, char *buf, int radix);
char *itoa(int value, char *buf, int radix);
int atoi(const char *str);
int memcmp(const void *s1, const void *s2, int n);
void *memchr(const void *s, unsigned char c, int n);
void *memset(void *s, unsigned char c, int n);
void *memset_word(void *s, uint16_t c, int n);
void *memset_dword(void *s, uint32_t c, int n);
void *memcpy(void *dest, const void *src, int n);
void *memmove(void *dest, const void *src, int n);
int ctz(unsigned int x);

#endif /* ASM */

#endif /* _STRING_H */
