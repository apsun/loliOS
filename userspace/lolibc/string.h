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
bool islower(char c);
bool isupper(char c);
bool isalpha(char c);
bool isdigit(char c);
bool isalnum(char c);
bool iscntrl(char c);
bool isblank(char c);
bool isspace(char c);
bool isprint(char c);
bool isgraph(char c);
bool ispunct(char c);
bool isxdigit(char c);
char tolower(char c);
char toupper(char c);

#endif /* _LOLIBC_STRING_H */
