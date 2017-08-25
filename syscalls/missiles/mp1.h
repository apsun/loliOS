#ifndef _MP1_H
#define _MP1_H

#include <stdint.h>

#define ASM_VISIBLE __attribute__((cdecl))

enum {
    IOCTL_STARTGAME,
    IOCTL_ADDMISSILE,
    IOCTL_MOVEXHAIRS,
    IOCTL_GETSTATUS,
    IOCTL_ENDGAME,
};

typedef struct missile {
    struct missile *next;   /* pointer to next missile in linked list */
    int32_t x, y;           /* x,y position on screen                 */
    int32_t vx, vy;         /* x,y velocity vector                    */
    int32_t dest_x, dest_y; /* location at which the missile explodes */
    int32_t exploded;       /* explosion duration counter             */
    char c;                 /* character to draw for this missile     */
} missile_t;

extern missile_t *mp1_missile_list;
extern char base_alive[3];
extern int32_t mp1_score;

ASM_VISIBLE void mp1_rtc_tasklet(uint32_t garbage);
ASM_VISIBLE int32_t mp1_ioctl(uint32_t arg, uint32_t cmd);

#endif /* _MP1_H */
