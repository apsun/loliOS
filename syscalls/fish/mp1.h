#ifndef _MP1_H
#define _MP1_H

#include <stdint.h>

#define ASM_VISIBLE __attribute__((cdecl))

enum {
    IOCTL_ADD,
    IOCTL_REMOVE,
    IOCTL_FIND,
    IOCTL_SYNC,
};

typedef struct blink {
    uint16_t location;
    char on_char;
    char off_char;
    uint16_t on_length;
    uint16_t off_length;
    uint16_t countdown;
    uint16_t status;
    struct blink *next;
} __attribute__((packed)) blink_t;

ASM_VISIBLE void mp1_rtc_tasklet(uint32_t garbage);
ASM_VISIBLE int32_t mp1_ioctl(uint32_t arg, uint32_t cmd);

#endif /* _MP1_H */
