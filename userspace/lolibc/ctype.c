#include <ctype.h>
#include <stdbool.h>

bool
islower(char c)
{
    return c >= 'a' && c <= 'z';
}

bool
isupper(char c)
{
    return c >= 'A' && c <= 'Z';
}

bool
isalpha(char c)
{
    return islower(c) || isupper(c);
}

bool
isdigit(char c)
{
    return c >= '0' && c <= '9';
}

bool
isalnum(char c)
{
    return isalpha(c) || isdigit(c);
}

bool
iscntrl(char c)
{
    return (c >= 0 && c <= 31) || c == 127;
}

bool
isblank(char c)
{
    return c == ' ' || c == '\t';
}

bool
isspace(char c)
{
    return isblank(c) || (c >= 10 && c <= 13);
}

bool
isprint(char c)
{
    return !iscntrl(c);
}

bool
isgraph(char c)
{
    return isprint(c) && c != ' ';
}

bool
ispunct(char c)
{
    return isgraph(c) && !isalnum(c);
}

bool
isxdigit(char c)
{
    return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

char
tolower(char c)
{
    if (isupper(c)) {
        return c - 'A' + 'a';
    } else {
        return c;
    }
}

char
toupper(char c)
{
    if (islower(c)) {
        return c - 'a' + 'A';
    } else {
        return c;
    }
}
