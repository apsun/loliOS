#ifndef _MP1_H
#define _MP1_H

#define ASM_VISIBLE __attribute__((cdecl))

enum {
    IOCTL_ADD,
    IOCTL_REMOVE,
    IOCTL_FIND,
    IOCTL_SYNC,
};

typedef struct blink {
    unsigned short location;
    char on_char;
    char off_char;
    unsigned short on_length;
    unsigned short off_length;
    unsigned short countdown;
    unsigned short status;
    struct blink *next;
} blink_t;

ASM_VISIBLE void mp1_rtc_tasklet(int garbage);
ASM_VISIBLE int mp1_ioctl(int arg, int cmd);

#endif /* _MP1_H */
