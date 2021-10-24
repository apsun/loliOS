#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

static void
test_strlen(void)
{
    assert(strlen("") == 0);
    assert(strlen("a") == 1);
    assert(strlen("foo") == 3);
}

static void
test_strcmp(void)
{
    assert(strcmp("a", "a") == 0);
    assert(strcmp("a", "b") < 0);
    assert(strcmp("", "") == 0);
    assert(strcmp("", "a") < 0);
    assert(strcmp("a", "") > 0);
}

static void
test_strncmp(void)
{
    assert(strncmp("a", "a", 1) == 0);
    assert(strncmp("a", "a", 2) == 0);
    assert(strncmp("a", "a", 3) == 0);
    assert(strncmp("a", "ab", 1) == 0);
    assert(strncmp("a", "ab", 2) != 0);
}

static void
test_strcpy(void)
{
    char buf[64];
    strcpy(buf, "Hello world!");
    assert(strcmp(buf, "Hello world!") == 0);
    assert(buf[strlen(buf)] == '\0');
}

static void
test_stpcpy(void)
{
    char buf[64];
    assert(stpcpy(buf, "Hello world!") == &buf[strlen("Hello world!")]);
    assert(strcmp(buf, "Hello world!") == 0);
    assert(buf[strlen(buf)] == '\0');
}

static void
test_strncpy(void)
{
    char buf[5];
    strncpy(buf, "Hello world!", sizeof(buf));
    assert(strncmp(buf, "Hello", 5) == 0);
}

static void
test_strscpy(void)
{
    char buf[16];
    assert(strscpy(buf, "Hello world!", sizeof(buf)) == strlen("Hello world!"));
    assert(strcmp(buf, "Hello world!") == 0);
    assert(strscpy(buf, "AAAAAAAAAAAAAAAAAAAAAAAA", 5) < 0);
    assert(strcmp(buf, "AAAA") == 0);
    assert(strscpy(buf, "foo", 0) < 0);
    assert(strcmp(buf, "AAAA") == 0);
}

static void
test_strcat(void)
{
    char buf[8] = {0};
    assert(strcat(buf, "foo") == buf);
    assert(strcat(buf, "bar") == buf);
    assert(strcmp(buf, "foobar") == 0);
}

static void
test_strncat(void)
{
    char buf[11] = {0};
    buf[10] = '\xff';
    assert(strncat(buf, "foo", 3) == buf);
    assert(strncat(buf, "bar", 3) == buf);
    assert(strcmp(buf, "foobar") == 0);
    assert(strncat(buf, "long", 3) == buf);
    assert(memcmp(buf, "foobarlon\0\xff", sizeof(buf)) == 0);
    assert(strncat(buf, "a", 3) == buf);
    assert(memcmp(buf, "foobarlona\0", sizeof(buf)) == 0);

    char buf2[1] = {'A'};
    assert(strncat(buf, "foo", 0) == buf);
    assert(buf2[0] == 'A');
}

static void
test_strrev(void)
{
    char buf[] = "Hello world!";
    assert(strrev(buf) == buf);
    assert(strcmp(buf, "!dlrow olleH") == 0);
}

static void
test_strchr(void)
{
    char buf[] = "nyaa";
    assert(strchr(buf, 'c') == NULL);
    assert(strchr(buf, 'n') == &buf[0]);
    assert(strchr(buf, 'a') == &buf[2]);
}

static void
test_strrchr(void)
{
    char buf[] = "nyaa";
    assert(strrchr(buf, 'c') == NULL);
    assert(strrchr(buf, 'n') == &buf[0]);
    assert(strrchr(buf, 'a') == &buf[3]);
}

static void
test_strstr(void)
{
    char buf[] = "cyka blyat";
    assert(strstr(buf, "blyat") == &buf[5]);
    assert(strstr(buf, "z") == NULL);
}

static void
test_strspn(void)
{
    char buf[] = "abcdefg1234567";
    assert(strspn(buf, "gfedcba") == 7);
    assert(strspn(buf, buf) == strlen(buf));
    assert(strspn(buf, "") == 0);
    assert(strspn(buf, "1234567") == 0);
}

static void
test_strcspn(void)
{
    char buf[] = "foo:bar;baz";
    assert(strcspn(buf, "@") == strlen(buf));
    assert(strcspn(buf, "") == strlen(buf));
    assert(strcspn(buf, ":") == 3);
    assert(strcspn(buf, ";-@") == 7);
    assert(strcspn(buf, ";:") == 3);
}

static void
test_strpbrk(void)
{
    char buf[] = "foo:bar;baz";
    assert(strpbrk(buf, "@") == NULL);
    assert(strpbrk(buf, "") == NULL);
    assert(strpbrk(buf, ":") == &buf[3]);
    assert(strpbrk(buf, ";-@") == &buf[7]);
    assert(strpbrk(buf, ";:") == &buf[3]);
}

static void
test_strtok(void)
{
    char buf[] = "foo:bar;baz@@blah-";
    assert(strcmp(strtok(buf, ""), "foo:bar;baz@@blah-") == 0);
    assert(strtok(NULL, "") == NULL);
    assert(strcmp(strtok(buf, "#"), "foo:bar;baz@@blah-") == 0);
    assert(strtok(NULL, "#") == NULL);
    assert(strcmp(strtok(buf, ":;@-"), "foo") == 0);
    assert(strcmp(strtok(NULL, ":;@-"), "bar") == 0);
    assert(strcmp(strtok(NULL, ":;@-"), "baz") == 0);
    assert(strcmp(strtok(NULL, ":;@-"), "blah") == 0);
    assert(strtok(NULL, ":;@-") == NULL);

    char buf2[] = "";
    assert(strtok(buf2, "") == NULL);
    assert(strtok(buf2, "abc") == NULL);
}

static void
test_strsep(void)
{
    char buf[] = "foo:bar;baz@@blah-";
    char *p = buf;
    assert(strcmp(strsep(&p, ""), "foo:bar;baz@@blah-") == 0);
    assert(strsep(&p, "") == NULL);
    p = buf;
    assert(strcmp(strsep(&p, "#"), "foo:bar;baz@@blah-") == 0);
    assert(strsep(&p, "#") == NULL);
    p = buf;
    assert(strcmp(strsep(&p, ":;@-"), "foo") == 0);
    assert(strcmp(strsep(&p, ":;@-"), "bar") == 0);
    assert(strcmp(strsep(&p, ":;@-"), "baz") == 0);
    assert(strcmp(strsep(&p, ":;@-"), "") == 0);
    assert(strcmp(strsep(&p, ":;@-"), "blah") == 0);
    assert(strcmp(strsep(&p, ":;@-"), "") == 0);
    assert(strsep(&p, ":;@-") == NULL);

    char buf2[] = "";
    char *p2 = buf2;
    assert(strsep(&p2, "") == buf2);
    assert(strsep(&p2, "") == NULL);
    p2 = buf2;
    assert(strsep(&p2, "abc") == buf2);
    assert(strsep(&p2, "abc") == NULL);
}

static void
test_utoa(void)
{
    char buf[64];
    assert(utoa(42, buf, 10) == buf);
    assert(strcmp(buf, "42") == 0);
    assert(utoa(0xff, buf, 16) == buf);
    assert(strcmp(buf, "ff") == 0);
}

static void
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

static void
test_memcmp(void)
{
    char buf[] = "i can haz buffer";
    assert(memcmp(buf, "i can haz buffer", strlen(buf)) == 0);
    assert(memcmp("a", "b", 1) != 0);
    assert(memcmp("aa", "ab", 1) == 0);
}

static void
test_memset(void)
{
    unsigned char buf[123];
    memset(buf, 0xaa, sizeof(buf));
    assert(buf[0] == 0xaa);
    assert(buf[122] == 0xaa);
    memset(buf, 0xbb, 1);
    assert(buf[0] == 0xbb);
    assert(buf[1] == 0xaa);
    memset(&buf[1], 0, sizeof(buf) - 1);
    assert(buf[0] == 0xbb);
    assert(buf[1] == 0);
    assert(buf[122] == 0);
}

static void
test_memcpy(void)
{
    unsigned char buf[16];
    memcpy(buf, "i like pie", 6);
    assert(memcmp(buf, "i like", 6) == 0);

    unsigned char buf2[50];
    memset(&buf2[0], 0x42, 25);
    memset(&buf2[25], 0x69, 25);
    memcpy(&buf2[3], &buf2[24], 3);
    assert(buf2[0] == 0x42);
    assert(buf2[3] == 0x42);
    assert(buf2[4] == 0x69);
    assert(buf2[5] == 0x69);
    assert(buf2[6] == 0x42);
}

static void
test_memmove(void)
{
    unsigned char buf[4] = {1, 2, 3, 4};
    memmove(&buf[0], &buf[1], 2);
    assert(buf[0] == 2);
    assert(buf[1] == 3);
    assert(buf[2] == 3);
    assert(buf[3] == 4);
    memmove(&buf[1], &buf[0], 3);
    assert(buf[0] == 2);
    assert(buf[1] == 2);
    assert(buf[2] == 3);
    assert(buf[3] == 3);
    memmove(&buf[2], &buf[0], 2);
    assert(buf[0] == 2);
    assert(buf[1] == 2);
    assert(buf[2] == 2);
    assert(buf[3] == 2);
}

static void
test_longjmp_helper(jmp_buf *envp)
{
    longjmp(*envp, 42);
    assert(false);
}

static void
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

static void
test_snprintf(void)
{
    char buf[16];
    assert(snprintf(buf, sizeof(buf), "%s!", "Hello") == strlen("Hello!"));
    assert(strcmp(buf, "Hello!") == 0);
    assert(snprintf(buf, sizeof(buf), "%s %s", "SUPER LONG", "STRING") == 17);
    assert(strcmp(buf, "SUPER LONG STRI") == 0);
    assert(snprintf(buf, 1, "wat") == 3);
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
    assert(snprintf(buf, sizeof(buf), "%025d", 10) == 25);
    assert(strcmp(buf, "000000000000000") == 0);
    assert(snprintf(buf, sizeof(buf), "%5s", "hi") == 5);
    assert(strcmp(buf, "   hi") == 0);
    assert(snprintf(buf, sizeof(buf), "%-5s", "hi") == 5);
    assert(strcmp(buf, "hi   ") == 0);
    assert(snprintf(buf, sizeof(buf), "%5p", 0x1234abcd) == 10);
    assert(strcmp(buf, "0x1234abcd") == 0);
    assert(snprintf(buf, sizeof(buf), "%12p", 0x1234abcd) == 12);
    assert(strcmp(buf, "  0x1234abcd") == 0);
    assert(snprintf(buf, sizeof(buf), "") == 0);
    assert(strcmp(buf, "") == 0);

    char buf2[1];
    assert(snprintf(buf2, sizeof(buf2), "Hello!") == strlen("Hello!"));
    assert(buf2[0] == '\0');
}

static void
test_atexit(void)
{
    puts("All tests passed!");
    halt(0);
}

static void
test_atexit_2(void)
{
    assert(false);
}

int
main(void)
{
    test_strlen();
    test_strcmp();
    test_strncmp();
    test_strcpy();
    test_stpcpy();
    test_strncpy();
    test_strscpy();
    test_strcat();
    test_strncat();
    test_strrev();
    test_strchr();
    test_strrchr();
    test_strstr();
    test_strspn();
    test_strcspn();
    test_strpbrk();
    test_strtok();
    test_strsep();
    test_utoa();
    test_itoa();
    test_memcmp();
    test_memset();
    test_memcpy();
    test_memmove();
    test_snprintf();
    test_longjmp();
    atexit(test_atexit_2);
    atexit(test_atexit);
    return 1;
}
