#include "mp1.h"
#include "mp1-math.h"
#include "mp1-taux.h"
#include "mp1-vga.h"
#include <assert.h>
#include <attrib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <syscall.h>

#define MISSILE_CHAR '*'
#define ENEMY_CHAR 'e'
#define EXPLOSION_CHAR '@'
#define TICKS_PER_SEC 32

static int fired = 0;
static int score = 0;
static int bases_left = 3;
static int crosshairs_x = 40;
static int crosshairs_y = 12;
static enum {
    TDM_SCORE,
    TDM_FIRED,
    TDM_XHAIR,
    TDM_TIME,
    TDM_MAX,
} taux_display_mode = TDM_SCORE;

static void
draw_starting_screen(void)
{
    int line = 5;
    draw_centered_string(line++, "            MISSILE COMMAND | TAUX EDITION             ");
    draw_centered_string(line++, "          Mark Murphy, 2007 | Andrew Sun, 2017         ");
    draw_centered_string(line++, "                                                       ");
    draw_centered_string(line++, "                        Commands:                      ");
    draw_centered_string(line++, "                  a ................. fire missile     ");
    draw_centered_string(line++, "                  c ................. toggle taux LEDs ");
    draw_centered_string(line++, " up,down,left,right ................. move crosshairs  ");
    draw_centered_string(line++, "              start ................. exit the game    ");
    draw_centered_string(line++, "                                                       ");
    draw_centered_string(line++, "                                                       ");
    draw_centered_string(line++, " Protect your bases by destroying the enemy missiles   ");
    draw_centered_string(line++, " (e's) with your missiles. You get 1 point for each    ");
    draw_centered_string(line++, " enemy missile you destroy. The game ends when your    ");
    draw_centered_string(line++, " bases are all dead or you hit the START button.       ");
    draw_centered_string(line++, "                                                       ");
    draw_centered_string(line++, "           Press the START button to continue.         ");
}

static void
draw_ending_screen(void)
{
    int line = SCREEN_HEIGHT / 2 - 1;
    draw_centered_string(line++, "+--------------------------------+");
    draw_centered_string(line++, "| Game over. Press START to exit |");
    draw_centered_string(line++, "+--------------------------------+");
}

static void
spawn_missile(
    int src_sx, int src_sy,
    int dest_sx, int dest_sy,
    char c, int vel)
{
    missile_t m;

    /* Starting position */
    m.x = (src_sx << 16) | 0x8000;
    m.y = (src_sy << 16) | 0x8000;

    /* Target position */
    m.dest_x = dest_sx;
    m.dest_y = dest_sy;

    /* Velocity */
    int vx = dest_sx - src_sx;
    int vy = dest_sy - src_sy;
    int mag = sqrt((vx * vx + vy * vy) << 16);
    m.vx = mag != 0 ? (vx << 16) * vel / mag : 0;
    m.vy = mag != 0 ? (vy << 16) * vel / mag : 0;

    /* Other parameters */
    m.c = c;
    m.exploded = 0;

    /* Add it to the missile list */
    mp1_ioctl((intptr_t)&m, IOCTL_ADDMISSILE);
}

static void
handle_taux_input(int buttons)
{
    int dx = 0;
    int dy = 0;

    if (buttons & TB_UP) dy--;
    if (buttons & TB_DOWN) dy++;
    if (buttons & TB_LEFT) dx--;
    if (buttons & TB_RIGHT) dx++;

    /* Update crosshair position */
    if (dx != 0 || dy != 0) {
        crosshairs_x = clamp(crosshairs_x + dx, 0, SCREEN_WIDTH - 1);
        crosshairs_y = clamp(crosshairs_y + dy, 0, SCREEN_HEIGHT - 1);

        /* Update crosshair position via ioctl */
        int d = 0;
        d |= (dx & 0xffff) << 0;
        d |= (dy & 0xffff) << 16;
        if (mp1_ioctl(d, IOCTL_MOVEXHAIRS) < 0) {
            assert(0);
        }
    }

    if (buttons & TB_A) {
        spawn_missile(79, 24, crosshairs_x, crosshairs_y, '*', 200);
        fired++;
    }

    if (buttons & TB_C) {
        taux_display_mode = (taux_display_mode + 1) % TDM_MAX;
    }
}

static void
draw_status_bar(void)
{
    int accuracy = (fired > 0) ? (100 * score) / fired : 0;
    char buf[80];
    snprintf(buf, sizeof(buf), "[score %3d] [fired %3d] [accuracy %3d%%]   ",
        score, fired, accuracy);
    draw_string(0, 0, buf);
}

static void
spawn_enemies(int ticks)
{
    static int total_enemies = 0;
    static int last_enemy_tick = -1;
    static int avg_enemy_delay = 4 * TICKS_PER_SEC;
    static int next_enemy_delay = 4 * TICKS_PER_SEC;

    /* Initialize on first call */
    if (last_enemy_tick < 0) {
        last_enemy_tick = ticks;
    }

    /* Time for another enemy? */
    if (ticks - last_enemy_tick >= next_enemy_delay) {
        /* Spawn enemy missile */
        int src_sx = rand() % SCREEN_WIDTH;
        int dest_sx = 20 * (rand() % 3 + 1);
        int vel = rand() % 5 + 10;
        spawn_missile(src_sx, 0, dest_sx, SCREEN_HEIGHT - 1, 'e', vel);
        total_enemies++;

        /* Update timing for the next missile */
        if (total_enemies % 10 == 0 && avg_enemy_delay > 2 * TICKS_PER_SEC / 10) {
            avg_enemy_delay -= TICKS_PER_SEC / 10;
        }
        last_enemy_tick = ticks;
        next_enemy_delay = avg_enemy_delay + (rand() % TICKS_PER_SEC) - TICKS_PER_SEC / 2;
    }
}

static int
base_explode(int sx, int sy)
{
    int bases_killed = 0;
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

static int
enemy_explode(int sx, int sy)
{
    int exploded = 0;
    missile_t *e;
    for (e = mp1_missile_list; e != NULL; e = e->next) {
        if (e->c != ENEMY_CHAR || e->exploded != 0) {
            continue;
        }

        int dsx = sx - (e->x >> 16);
        int dsy = sy - (e->y >> 16);
        if (abs(dsx) <= 2 && abs(dsy) <= 1) {
            mp1_score++;
            exploded++;
            e->exploded = 50;
        }
    }
    return exploded;
}

static void
update_taux_leds(int taux_fd, int ticks)
{
    switch (taux_display_mode) {
    case TDM_SCORE:
        taux_display_num(taux_fd, mp1_score);
        break;
    case TDM_FIRED:
        taux_display_num(taux_fd, fired);
        break;
    case TDM_XHAIR:
        taux_display_coords(taux_fd, crosshairs_x, crosshairs_y);
        break;
    case TDM_TIME:
        taux_display_time(taux_fd, ticks / TICKS_PER_SEC);
        break;
    default:
        break;
    }
}

__cdecl int
missile_explode(struct missile *m)
{
    int exploded = 0;

    /* Start explosion timer */
    if (m->exploded == 0) {
        m->exploded = 50;
    }

    /* Base collision detection */
    if (m->c == ENEMY_CHAR) {
        exploded += base_explode(m->x >> 16, m->y >> 16);
    }

    /* Enemy collision detection */
    if (m->c == MISSILE_CHAR) {
        exploded += enemy_explode(m->x >> 16, m->y >> 16);
    }

    return exploded;
}

__cdecl void
mp1_notify_user(void)
{
    int status;
    if (mp1_ioctl((intptr_t)&status, IOCTL_GETSTATUS) < 0) {
        assert(0);
    }

    score = status & 0xffff;
    bases_left =
        ((status >> 16) & 1) +
        ((status >> 17) & 1) +
        ((status >> 18) & 1);
}

int
main(void)
{
    int taux_fd = create("taux", OPEN_RDWR);
    int rtc_fd = create("rtc", OPEN_RDWR);
    int rtc_freq = TICKS_PER_SEC;
    write(rtc_fd, &rtc_freq, sizeof(rtc_freq));

    /* Initialization */
    srand((unsigned int)realtime());
    vga_init();

    /* Wait for user to begin */
    clear_screen();
    draw_starting_screen();
    taux_display_str(taux_fd, "strt");
    while (!(taux_get_input(taux_fd) & TB_START));

    /* Here we go! */
    clear_screen();
    if (mp1_ioctl(0, IOCTL_STARTGAME) < 0) {
        assert(0);
    }

    /* Main game loop */
    int ticks = 0;
    unsigned char buttons;
    while (1) {
        /* Check if all bases are dead */
        if (bases_left == 0) {
            taux_display_str(taux_fd, "dead");
            break;
        }

        /* Update taux input, exit if start is pressed */
        buttons = taux_get_input(taux_fd);
        if (buttons & TB_START) {
            taux_display_str(taux_fd, "bye ");
            break;
        }

        /* Delay to target tick rate */
        int garbage;
        read(rtc_fd, &garbage, sizeof(garbage));
        ticks++;

        /* Process button input */
        handle_taux_input(buttons);

        /* Spawn more enemies if necessary */
        spawn_enemies(ticks);

        /* Update display on the taux controller */
        update_taux_leds(taux_fd, ticks);

        /* Perform in-game updates */
        mp1_rtc_tasklet(0);

        /* Draw the status bar (after the tasklet so it's on top) */
        draw_status_bar();
    }

    /* Cleanup time! */
    if (mp1_ioctl(0, IOCTL_ENDGAME) < 0) {
        assert(0);
    }

    /* Wait for user to exit */
    draw_ending_screen();
    while (!(taux_get_input(taux_fd) & TB_START));
    taux_display_str(taux_fd, "    ");

    /* Bye! */
    clear_screen();
    close(rtc_fd);
    close(taux_fd);
    return 0;
}
