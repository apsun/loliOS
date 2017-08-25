#ifndef _LOLIBC_CTYPE_H
#define _LOLIBC_CTYPE_H

#include <stdbool.h>

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

#endif /* _LOLIBC_CTYPE_H */
