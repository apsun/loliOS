#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

void
test_strlen(void)
{
    assert(strlen("") == 0);
    assert(strlen("a") == 1);
    assert(strlen("foo") == 3);
}

void
test_strcmp(void)
{
    assert(strcmp("a", "a") == 0);
    assert(strcmp("a", "b") < 0);
    assert(strcmp("", "") == 0);
    assert(strcmp("", "a") < 0);
    assert(strcmp("a", "") > 0);
}

void
test_strncmp(void)
{
    assert(strncmp("a", "a", 1) == 0);
    assert(strncmp("a", "a", 2) == 0);
    assert(strncmp("a", "a", 3) == 0);
    assert(strncmp("a", "ab", 1) == 0);
    assert(strncmp("a", "ab", 2) != 0);
}

void
test_strcpy(void)
{
    char buf[64];
    strcpy(buf, "Hello world!");
    assert(strcmp(buf, "Hello world!") == 0);
    assert(buf[strlen(buf)] == '\0');
}

void
test_strncpy(void)
{
    char buf[5];
    strncpy(buf, "Hello world!", sizeof(buf));
    assert(strncmp(buf, "Hello", 5) == 0);
}

void
test_strscpy(void)
{
    char buf[16];
    assert(strscpy(buf, "Hello world!", sizeof(buf)) == strlen("Hello world!"));
    assert(strcmp(buf, "Hello world!") == 0);
    assert(strscpy(buf, "AAAAAAAAAAAAAAAAAAAAAAAA", 5) < 0);
    assert(strcmp(buf, "AAAA") == 0);
}

void
test_strcat(void)
{
    char buf[8] = {0};
    assert(strcat(buf, "foo") == buf);
    assert(strcat(buf, "bar") == buf);
    assert(strcmp(buf, "foobar") == 0);
}

void
test_strncat(void)
{
    char buf[8] = {0};
    assert(strncat(buf, "foo", 3) == buf);
    assert(strncat(buf, "bar", 3) == buf);
    assert(strcmp(buf, "foobar") == 0);
    assert(strncat(buf, "long", 3) == buf);
    assert(strncmp(buf, "foobarlo", sizeof(buf)) == 0);
}

void
test_strrev(void)
{
    char buf[] = "Hello world!";
    assert(strrev(buf) == buf);
    assert(strcmp(buf, "!dlrow olleH") == 0);
}

void
test_strchr(void)
{
    char buf[] = "nyaa";
    assert(strchr(buf, 'c') == NULL);
    assert(strchr(buf, 'n') == &buf[0]);
    assert(strchr(buf, 'a') == &buf[2]);
}

void
test_strrchr(void)
{
    char buf[] = "nyaa";
    assert(strrchr(buf, 'c') == NULL);
    assert(strrchr(buf, 'n') == &buf[0]);
    assert(strrchr(buf, 'a') == &buf[3]);
}

void
test_strstr(void)
{
    char buf[] = "cyka blyat";
    assert(strstr(buf, "blyat") == &buf[5]);
    assert(strstr(buf, "z") == NULL);
}

void
test_utoa(void)
{
    char buf[64];
    assert(utoa(42, buf, 10) == buf);
    assert(strcmp(buf, "42") == 0);
    assert(utoa(0xff, buf, 16) == buf);
    assert(strcmp(buf, "ff") == 0);
}

void
test_itoa(void)
{
    char buf[64];
    assert(itoa(42, buf, 10) == buf);
    assert(strcmp(buf, "42") == 0);
    assert(itoa(-42, buf, 10) == buf);
    assert(strcmp(buf, "-42") == 0);
    assert(itoa(-0xff, buf, 16) == buf);
    assert(strcmp(buf, "-ff") == 0);
    assert(itoa(-2147483647 - 1, buf, 10) == buf);
    assert(strcmp(buf, "-2147483648") == 0);
}

void
test_memcmp(void)
{
    char buf[] = "i can haz buffer";
    assert(memcmp(buf, "i can haz buffer", strlen(buf)) == 0);
    assert(memcmp("a", "b", 1) != 0);
    assert(memcmp("aa", "ab", 1) == 0);
}

void
test_memset(void)
{
    unsigned char buf[16];
    memset(buf, 0xaa, sizeof(buf));
    assert(buf[0] == 0xaa);
    assert(buf[15] == 0xaa);
    memset(buf, 0xbb, 1);
    assert(buf[0] == 0xbb);
    assert(buf[1] == 0xaa);
}

void
test_memcpy(void)
{
    unsigned char buf[16];
    memcpy(buf, "i like pie", 6);
    assert(memcmp(buf, "i like", 6) == 0);
}

void
test_memmove(void)
{
    unsigned char buf[4] = {1, 2, 3, 4};
    memmove(&buf[0], &buf[1], 2);
    assert(buf[0] == 2);
    assert(buf[1] == 3);
    assert(buf[2] == 3);
    assert(buf[3] == 4);
    memmove(&buf[2], &buf[0], 2);
    assert(buf[0] == 2);
    assert(buf[1] == 3);
    assert(buf[2] == 2);
    assert(buf[3] == 3);
}

void
test_longjmp_helper(jmp_buf *envp)
{
    longjmp(*envp, 42);
    assert(false);
}

void
test_longjmp(void)
{
    jmp_buf env;
    int ret;
    if ((ret = setjmp(env)) == 0) {
        test_longjmp_helper(&env);
        assert(false);
    } else {
        assert(ret == 42);
    }
}

void
test_snprintf(void)
{
    char buf[8];
    assert(snprintf(buf, sizeof(buf), "%s!", "Hello") == strlen("Hello!"));
    assert(strcmp(buf, "Hello!") == 0);
    assert(snprintf(buf, sizeof(buf), "%s %s", "LONG", "STRING") < 0);
    assert(strcmp(buf, "LONG ST") == 0);
    assert(snprintf(buf, 1, "wat") < 0);
    assert(strcmp(buf, "") == 0);
    assert(snprintf(buf, sizeof(buf), "%d", -10) == 3);
    assert(strcmp(buf, "-10") == 0);
    assert(snprintf(buf, sizeof(buf), "%3d", -1000) == 5);
    assert(strcmp(buf, "-1000") == 0);
    assert(snprintf(buf, sizeof(buf), "%3d", 10000) == 5);
    assert(strcmp(buf, "10000") == 0);
    assert(snprintf(buf, sizeof(buf), "%-5x", 0xabc) == 5);
    assert(strcmp(buf, "abc  ") == 0);
    assert(snprintf(buf, sizeof(buf), "%-5X", 0xabc) == 5);
    assert(strcmp(buf, "ABC  ") == 0);
    assert(snprintf(buf, sizeof(buf), "% d", 10) == 3);
    assert(strcmp(buf, " 10") == 0);
    assert(snprintf(buf, sizeof(buf), "%+d", 10) == 3);
    assert(strcmp(buf, "+10") == 0);
    assert(snprintf(buf, sizeof(buf), "%-5d", -10) == 5);
    assert(strcmp(buf, "-10  ") == 0);
    assert(snprintf(buf, sizeof(buf), "%05d", -10) == 5);
    assert(strcmp(buf, "-0010") == 0);
    assert(snprintf(buf, sizeof(buf), "%5d", -10) == 5);
    assert(strcmp(buf, "  -10") == 0);
    assert(snprintf(buf, sizeof(buf), "%025d", 10) < 0);
    assert(strcmp(buf, "0000000") == 0);
    assert(snprintf(buf, sizeof(buf), "%5s", "hi") == 5);
    assert(strcmp(buf, "   hi") == 0);
    assert(snprintf(buf, sizeof(buf), "%-5s", "hi") == 5);
    assert(strcmp(buf, "hi   ") == 0);
    assert(snprintf(buf, sizeof(buf), "") == 0);
    assert(strcmp(buf, "") == 0);
}

void
test_varargs(char dummy, ...)
{
    va_list args;
    va_start(args, dummy);
    assert(va_arg(args, int) == 1);
    va_list args2;
    va_copy(args2, args);
    va_end(args);
    assert(va_arg(args2, int) == 2);
    assert(va_arg(args2, int) == 3);
    va_end(args2);
}

void
test_atexit(void)
{
    puts("All tests passed!");
}

int
main(void)
{
    test_strlen();
    test_strcmp();
    test_strncmp();
    test_strcpy();
    test_strncpy();
    test_strscpy();
    test_strcat();
    test_strncat();
    test_strrev();
    test_strchr();
    test_strrchr();
    test_strstr();
    test_utoa();
    test_itoa();
    test_memcmp();
    test_memset();
    test_memcpy();
    test_memmove();
    test_snprintf();
    test_longjmp();
    test_varargs('c', 1, 2, 3);
    atexit(test_atexit);
    return 0;
}
