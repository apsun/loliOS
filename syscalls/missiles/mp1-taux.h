#ifndef _MP1_TAUX_H
#define _MP1_TAUX_H

#define TUX_SET_LED     0x10
#define TUX_BUTTONS     0x12
#define TUX_INIT        0x13
#define TUX_SET_LED_STR 0x16

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

void taux_display_str(int taux_fd, const char *str);
void taux_display_time(int taux_fd, int num_seconds);
void taux_display_coords(int taux_fd, int x, int y);
void taux_display_num(int taux_fd, int score);
int taux_get_input(int taux_fd);

#endif /* _MP1_TAUX_H */
