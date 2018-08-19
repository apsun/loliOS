#include <ctype.h>
#include <stdbool.h>

/*
 * Checks whether the input is a lowercase
 * alphabetical character.
 */
bool
islower(char c)
{
    return c >= 'a' && c <= 'z';
}

/*
 * Checks whether the input is an uppercase
 * alphabetical character.
 */
bool
isupper(char c)
{
    return c >= 'A' && c <= 'Z';
}

/*
 * Checks whether the input is an alphabetical
 * character.
 */
bool
isalpha(char c)
{
    return islower(c) || isupper(c);
}

/*
 * Checks whether the input is a numerical
 * character.
 */
bool
isdigit(char c)
{
    return c >= '0' && c <= '9';
}

/*
 * Checks whether the input is either an alphabetical
 * or a numerical character.
 */
bool
isalnum(char c)
{
    return isalpha(c) || isdigit(c);
}

/*
 * Checks whether the input is a control character.
 */
bool
iscntrl(char c)
{
    return (c >= 0 && c <= 31) || c == 127;
}

/*
 * Checks whether the input is space or tab.
 */
bool
isblank(char c)
{
    return c == ' ' || c == '\t';
}

/*
 * Checks whether the input is whitespace.
 */
bool
isspace(char c)
{
    return isblank(c) || (c >= 10 && c <= 13);
}

/*
 * Checks whether the input is a printable (non-control)
 * character.
 */
bool
isprint(char c)
{
    return !iscntrl(c);
}

/*
 * Checks whether the input is a printable non-space
 * character.
 */
bool
isgraph(char c)
{
    return isprint(c) && c != ' ';
}

/*
 * Checks whether the input is a punctuation character.
 */
bool
ispunct(char c)
{
    return isgraph(c) && !isalnum(c);
}

/*
 * Checks whether the input is a hexadecimal character.
 */
bool
isxdigit(char c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/*
 * Converts a character to lowercase.
 */
char
tolower(char c)
{
    if (isupper(c)) {
        return c - 'A' + 'a';
    } else {
        return c;
    }
}

/*
 * Converts a character to uppercase.
 */
char
toupper(char c)
{
    if (islower(c)) {
        return c - 'a' + 'A';
    } else {
        return c;
    }
}
