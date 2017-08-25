#include <types.h>
#include <sys.h>
#include <io.h>

#define BUFSIZE 1024

int32_t
main(void)
{
    printf("Hi, what's your name? ");

    char buf[1024];
    if (gets(buf, sizeof(buf)) == NULL) {
        puts("Can't read name from keyboard.");
        return 3;
    }

    printf("Hello, %s\n", buf);
    return 0;
}
