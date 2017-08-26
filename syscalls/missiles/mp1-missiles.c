#include "mp1.h"
#include "mp1-math.h"
#include "mp1-taux.h"
#include "mp1-vga.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

#define MISSILE_CHAR '*'
#define ENEMY_CHAR 'e'
#define EXPLOSION_CHAR '@'
#define TICKS_PER_SEC 32

static int32_t fired = 0;
static int32_t score = 0;
static int32_t bases_left = 3;
static int32_t crosshairs_x = 40;
static int32_t crosshairs_y = 12;
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
    int32_t line = 5;
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
    int32_t line = SCREEN_HEIGHT / 2 - 1;
    draw_centered_string(line++, "+--------------------------------+");
    draw_centered_string(line++, "| Game over. Press START to exit |");
    draw_centered_string(line++, "+--------------------------------+");
}

static void
spawn_missile(
    int32_t src_sx, int32_t src_sy,
    int32_t dest_sx, int32_t dest_sy,
    char c, int32_t vel)
{
    missile_t m;

    /* Starting position */
    m.x = (src_sx << 16) | 0x8000;
    m.y = (src_sy << 16) | 0x8000;

    /* Target position */
    m.dest_x = dest_sx;
    m.dest_y = dest_sy;

    /* Velocity */
    int32_t vx = dest_sx - src_sx;
    int32_t vy = dest_sy - src_sy;
    int32_t mag = sqrt((vx * vx + vy * vy) << 16);
    m.vx = mag != 0 ? (vx << 16) * vel / mag : 0;
    m.vy = mag != 0 ? (vy << 16) * vel / mag : 0;

    /* Other parameters */
    m.c = c;
    m.exploded = 0;

    /* Add it to the missile list */
    mp1_ioctl((uint32_t)&m, IOCTL_ADDMISSILE);
}

static void
handle_taux_input(uint8_t buttons)
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
    int32_t accuracy = (fired > 0) ? (100 * score) / fired : 0;
    char buf[80];
    snprintf(buf, sizeof(buf), "[score %3d] [fired %3d] [accuracy %3d%%]   ",
        score, fired, accuracy);
    draw_string(0, 0, buf);
}

static void
spawn_enemies(int32_t ticks)
{
    static int32_t total_enemies = 0;
    static int32_t last_enemy_tick = -1;
    static int32_t avg_enemy_delay = 4 * TICKS_PER_SEC;
    static int32_t next_enemy_delay = 4 * TICKS_PER_SEC;

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

static int32_t
enemy_explode(int32_t sx, int32_t sy)
{
    int32_t exploded = 0;
    missile_t *e;
    for (e = mp1_missile_list; e != NULL; e = e->next) {
        if (e->c != ENEMY_CHAR || e->exploded != 0) {
            continue;
        }

        int32_t dsx = sx - (e->x >> 16);
        int32_t dsy = sy - (e->y >> 16);
        if (abs(dsx) <= 2 && abs(dsy) <= 1) {
            mp1_score++;
            exploded++;
            e->exploded = 50;
        }
    }
    return exploded;
}

static void
update_taux_leds(int32_t taux_fd, int32_t ticks)
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

ASM_VISIBLE int32_t
missile_explode(struct missile *m)
{
    int32_t exploded = 0;

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

ASM_VISIBLE void
mp1_notify_user(void)
{
    uint32_t status;
    if (mp1_ioctl((uint32_t)&status, IOCTL_GETSTATUS) < 0) {
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
    int32_t taux_fd = open("taux");
    int32_t rtc_fd = open("rtc");
    int32_t rtc_freq = TICKS_PER_SEC;
    write(rtc_fd, &rtc_freq, sizeof(rtc_freq));

    /* Initialization */
    srand(time());
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
    int32_t ticks = 0;
    uint8_t buttons;
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
        int32_t garbage;
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
