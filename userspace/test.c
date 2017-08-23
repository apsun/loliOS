#include "lolibc/types.h"
#include "lolibc/sys.h"
#include "lolibc/io.h"
#include "lolibc/longjmp.h"
#include "lolibc/string.h"
#include "lolibc/assert.h"

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
    buf[5] = '\0';
    strncpy(buf, "Hello world!", 5);
    assert(strcmp(buf, "Hello") == 0);
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
    assert(strcmp(buf, "FF") == 0);
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
    assert(strcmp(buf, "-FF") == 0);
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
    uint8_t buf[16];
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
    uint8_t buf[16];
    memcpy(buf, "i like pie", 6);
    assert(memcmp(buf, "i like", 6) == 0);
}

void
test_memmove(void)
{
    uint8_t buf[4] = {1, 2, 3, 4};
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

jmp_buf env;

void
test_longjmp(void)
{
    longjmp(env, 42);
    assert(false);
}

int32_t
main(void)
{
    test_strlen();
    test_strcmp();
    test_strncmp();
    test_strcpy();
    test_strncpy();
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
    
    int32_t ret;
    if ((ret = setjmp(env)) == 0) {
        test_longjmp();
        assert(false);
    } else {
        assert(ret == 42);
    }

    return 0;
}
