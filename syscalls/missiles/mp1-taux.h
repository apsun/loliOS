#ifndef _MP1_TAUX_H
#define _MP1_TAUX_H

#include <stdint.h>

#define TUX_SET_LED 0x10
#define TUX_BUTTONS 0x12
#define TUX_INIT    0x13

enum {
    TB_START  = 0x01,
    TB_A      = 0x02,
    TB_B      = 0x04,
    TB_C      = 0x08,
    TB_UP     = 0x10,
    TB_DOWN   = 0x20,
    TB_LEFT   = 0x40,
    TB_RIGHT  = 0x80,
    TB_ALL    = 0xff,
};

void taux_display_time(int32_t taux_fd, int32_t num_seconds);
void taux_display_coords(int32_t taux_fd, int32_t x, int32_t y);
void taux_display_num(int32_t taux_fd, int32_t score);
uint8_t taux_get_input(int32_t taux_fd);

#endif /* _MP1_TAUX_H */
