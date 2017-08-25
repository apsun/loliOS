#include "mp1.h"
#include "mp1-taux.h"
#include "mp1-vga.h"
#include <types.h>
#include <sys.h>
#include <io.h>
#include <assert.h>
#include <rand.h>

#define SCREEN_POS(x) ((x) >> 16)
#define MISSILE_CHAR '*'
#define ENEMY_CHAR 'e'
#define EXPLOSION_CHAR '@'
#define FPS 32

static int32_t fired = 0;
static int32_t score = 0;
static int32_t bases_left = 3;
static int32_t crosshairs_x = 40;
static int32_t crosshairs_y = 12;

static void
draw_starting_screen(void)
{
    int32_t line = 5;
    draw_centered_string(line++, "            MISSILE COMMAND | TAUX EDITION            ");
    draw_centered_string(line++, "          Mark Murphy, 2007 | Andrew Sun, 2017        ");
    draw_centered_string(line++, "                                                      ");
    draw_centered_string(line++, "                        Commands:                     ");
    draw_centered_string(line++, "                  a ................. fire missile    ");
    draw_centered_string(line++, " up,down,left,right ................. move crosshairs ");
    draw_centered_string(line++, "              start ................. exit the game   ");
    draw_centered_string(line++, "                                                      ");
    draw_centered_string(line++, "                                                      ");
    draw_centered_string(line++, " Protect your bases by destroying the enemy missiles  ");
    draw_centered_string(line++, " (e's) with your missiles. You get 1 point for each   ");
    draw_centered_string(line++, " enemy missile you destroy. The game ends when your   ");
    draw_centered_string(line++, " bases are all dead or you hit the START button.      ");
    draw_centered_string(line++, "                                                      ");
    draw_centered_string(line++, "           Press the START button to continue.        ");
}

static void
draw_ending_screen(void)
{
    int32_t line = SCREEN_HEIGHT / 2 - 1;
    draw_centered_string(line++, "+--------------------------------+");
    draw_centered_string(line++, "| Game over. Press START to exit |");
    draw_centered_string(line++, "+--------------------------------+");
}

static int32_t
abs(int32_t x)
{
    return x < 0 ? -x : x;
}

static int32_t
sqrt(int32_t x)
{
    /* TODO */
    return 0;
}

static int32_t
clamp(int32_t x, int32_t min, int32_t max)
{
    return (x < min) ? min : (x > max) ? max : x;
}

static void
spawn_missile(
    int src_sx, int src_sy,
    int dest_sx, int dest_sy,
    char c, int vel)
{
#if 0
    struct missile m[1];
    int vx, vy, mag;

    m->x = (sx<<16) | 0x8000; 
    m->y = (sy<<16) | 0x8000;

    m->dest_x = dx;
    m->dest_y = dy;

    vx = (dx - sx);
    vy = (dy - sy);

    mag = sqrt((vx*vx + vy*vy)<<16);
    m->vx = ((vx<<16)*vel)/mag;
    m->vy = ((vy<<16)*vel)/mag;

    m->c = c;
    m->exploded = 0;

    ioctl(rtc_fd, RTC_ADDMISSILE, (unsigned long)m);
#endif
}

static void
handle_input(uint8_t buttons)
{
    int32_t dx = 0;
    int32_t dy = 0;

    if (buttons & TB_UP) dy--;
    if (buttons & TB_DOWN) dy++;
    if (buttons & TB_LEFT) dx--;
    if (buttons & TB_RIGHT) dx++;

    /* Update crosshair position */
    if (dx != 0 || dy != 0) {
        crosshairs_x = clamp(crosshairs_x + dx, 0, SCREEN_WIDTH - 1);
        crosshairs_y = clamp(crosshairs_y + dy, 0, SCREEN_HEIGHT - 1);

        /* Update crosshair position via ioctl */
        uint32_t d = 0;
        d |= (dx & 0xffff) << 0;
        d |= (dy & 0xffff) << 16;
        if (mp1_ioctl(IOCTL_MOVEXHAIRS, d) < 0) {
            assert(0);
        }
    }

    /* Fire ze missiles! */
    if (buttons & TB_A) {
        spawn_missile(79, 24, crosshairs_x, crosshairs_y, '*', 200);
        fired++;
        fired++;
    }
}

static void
draw_status_bar(void)
{
    int32_t accuracy = fired ? (100 * score) / fired : 0;
    char buf[80];
    snprintf(buf, sizeof(buf),
        "[score %d] [fired %d] [accuracy %d%%]   ",
        score, fired, accuracy);
    draw_string(0, 0, buf);
}

static void
spawn_enemies(void)
{
    /* TODO */
}

static int32_t
base_explode(int32_t sx, int32_t sy)
{
    int32_t bases_killed = 0;
    if (sy >= SCREEN_HEIGHT - 2) {
        if (abs(sx - 20) <= 3 && base_alive[0]) {
            base_alive[0] = 0;
            bases_killed++;
        }

        if (abs(sx - 40) <= 3 && base_alive[1]) {
            base_alive[1] = 0;
            bases_killed++;
        }

        if (abs(sx - 60) <= 3 && base_alive[2]) {
            base_alive[2] = 0;
            bases_killed++;
        }
    }
    return bases_killed;
}

__attribute__((cdecl)) int32_t
missile_explode(struct missile *m)
{
    int32_t exploded = 0;

    /* Start explosion timer */
    if (m->exploded == 0) {
        m->exploded = 50;
    }

    /* Base collision detection */
    if (m->c == ENEMY_CHAR) {
        exploded += base_explode(SCREEN_POS(m->x), SCREEN_POS(m->y));
    }

    /* Enemy collision detection */
    missile_t *curr;
    for (curr = mp1_missile_list; curr != NULL; curr = curr->next) {
        if (curr == m) {
            continue;
        }

        int32_t dsx = SCREEN_POS(m->x) - SCREEN_POS(curr->x);
        int32_t dsy = SCREEN_POS(m->y) - SCREEN_POS(curr->y);
        if (abs(dsx) <= 2 && abs(dsy) <= 1 && curr->exploded == 0 &&
            curr->c == ENEMY_CHAR && m->c == MISSILE_CHAR) {

            mp1_score++;
            exploded++;
            curr->exploded = 50;
        }
    }

    return exploded;
}

__attribute__((cdecl)) void
mp1_notify_user(void)
{
    uint32_t status;
    if (mp1_ioctl(IOCTL_GETSTATUS, (uint32_t)&status) < 0) {
        assert(0);
    }

    score = status & 0xffff;
    bases_left =
        ((status >> 16) & 1) + 
        ((status >> 17) & 1) +
        ((status >> 18) & 1);
}

int32_t
main(void)
{
    /* Initialization */
    int32_t taux_fd = open("taux");
    int32_t rtc_fd = open("rtc");
    int32_t rtc_freq = 1024;
    write(rtc_fd, &rtc_freq, sizeof(rtc_freq));
    srand(time());
    vga_init();
    clear_screen();

    /* Wait for user to begin */
    draw_starting_screen();
    while (!(taux_get_input(taux_fd) & TB_START));

    /* Here we go! */
    clear_screen();
    if (mp1_ioctl(IOCTL_STARTGAME, 0) < 0) {
        assert(0);
    }

    /* Main game loop */
    int32_t ticks = 0;
    uint8_t buttons;
    while (((buttons = taux_get_input(taux_fd)) & TB_START) == 0) {
        /* Delay to target frame rate */
        int32_t garbage;
        read(rtc_fd, &garbage, sizeof(garbage));

        handle_input(buttons);
        draw_status_bar();
        spawn_enemies();
        if (ticks++ % (rtc_freq / FPS) == 0) {
            mp1_rtc_tasklet(0);
        }
    }

    /* Cleanup time! */
    if (mp1_ioctl(IOCTL_ENDGAME, 0) < 0) {
        assert(0);
    }

    /* Wait for user to exit */
    draw_ending_screen();
    while (!(taux_get_input(taux_fd) & TB_START));

    /* Finalization */
    clear_screen();
    close(rtc_fd);
    close(taux_fd);
    return 0;
}
