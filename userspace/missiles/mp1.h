#ifndef _MP1_H
#define _MP1_H

#include <attrib.h>

enum {
    IOCTL_STARTGAME,
    IOCTL_ADDMISSILE,
    IOCTL_MOVEXHAIRS,
    IOCTL_GETSTATUS,
    IOCTL_ENDGAME,
};

typedef struct missile {
    struct missile *next;   /* pointer to next missile in linked list */
    int x, y;               /* x,y position on screen                 */
    int vx, vy;             /* x,y velocity vector                    */
    int dest_x, dest_y;     /* location at which the missile explodes */
    int exploded;           /* explosion duration counter             */
    char c;                 /* character to draw for this missile     */
} missile_t;

extern missile_t *mp1_missile_list;
extern char base_alive[3];
extern int mp1_score;

__cdecl void mp1_rtc_tasklet(int garbage);
__cdecl int mp1_ioctl(int arg, int cmd);

#endif /* _MP1_H */
