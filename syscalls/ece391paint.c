#include <stdint.h>
#include "ece391support.h"
#include "ece391syscall.h"

/* Mouse bits */
#define MOUSE_LEFT (1 << 0)
#define MOUSE_RIGHT (1 << 1)
#define MOUSE_MIDDLE (1 << 2)
#define MOUSE_X_SIGN (1 << 4)
#define MOUSE_Y_SIGN (1 << 5)
#define MOUSE_X_OVERFLOW (1 << 6)
#define MOUSE_Y_OVERFLOW (1 << 7)

/* VGA colors */
#define COLOR_BLACK 0
#define COLOR_BLUE 1
#define COLOR_GREEN 2
#define COLOR_CYAN 3
#define COLOR_RED 4
#define COLOR_PURPLE 5
#define COLOR_ORANGE 6
#define COLOR_GRAY 7
#define NUM_COLORS 8

/* Screen dimensions */
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

/* Palette dimensions */
#define PALETTE_WIDTH 4
#define PALETTE_HEIGHT 2

/* Virtual canvas dimensions */
#define SCALE_FACTOR_BASE 72
#define SCALE_FACTOR_X (SCALE_FACTOR_BASE * 1)
#define SCALE_FACTOR_Y (SCALE_FACTOR_BASE * 1)
#define CANVAS_WIDTH (SCREEN_WIDTH * SCALE_FACTOR_X)
#define CANVAS_HEIGHT (SCREEN_HEIGHT * SCALE_FACTOR_Y)
#define MOUSE_SPEED 1

/* Configurable color stuff */
#define USE_DARK_BG 0
#if USE_DARK_BG
    #define HIGHLIGHT_FG true
    #define HIGHLIGHT_BG false
    #define COLOR_BG COLOR_BLACK
#else
    #define HIGHLIGHT_FG false
    #define HIGHLIGHT_BG true
    #define COLOR_BG COLOR_GRAY
#endif

/* Boolean type */
typedef uint8_t bool;
#define true 1
#define false 0

/* Null definition */
#define NULL 0

/* Raw mouse input, straight from the kernel */
typedef struct {
    uint8_t flags;
    uint8_t dx;
    uint8_t dy;
} raw_mouse_input_t;

/* Pre-processed mouse input */
typedef struct {
    int16_t dx;
    int16_t dy;
    bool left : 1;
    bool right : 1;
    bool middle : 1;
} mouse_input_t;

static bool haz_interrupt = false;

void
puts(const char *s)
{
    ece391_fdputs(1, (uint8_t *)s);
}

bool
parse_mouse_input(raw_mouse_input_t in, mouse_input_t *out)
{
    uint8_t flags = in.flags;

    /* Discard packet if overflow */
    if ((flags & (MOUSE_X_OVERFLOW | MOUSE_Y_OVERFLOW)) != 0) {
        return false;
    }

    /* Delta x movement */
    int16_t dx = in.dx;
    if ((flags & MOUSE_X_SIGN) != 0) {
        dx |= 0xff00;
    }

    /* Delta y movement */
    int16_t dy = in.dy;
    if ((flags & MOUSE_Y_SIGN) != 0) {
        dy |= 0xff00;
    }

    /* Buttons */
    bool left = !!(flags & MOUSE_LEFT);
    bool right = !!(flags & MOUSE_RIGHT);
    bool middle = !!(flags & MOUSE_MIDDLE);

    /* Copy to output */
    out->dx = dx;
    out->dy = dy;
    out->left = left;
    out->right = right;
    out->middle = middle;
    return true;
}

bool
read_mouse_input(int fd, mouse_input_t *out)
{
    raw_mouse_input_t raw;
    int32_t ret = ece391_read(fd, &raw, sizeof(raw));
    if (ret <= 0) {
        return false;
    }

    if (!parse_mouse_input(raw, out)) {
        return false;
    }

    return true;
}

void
draw_char(uint8_t *video_mem, int32_t x, int32_t y, char c)
{
    uint8_t *addr = &video_mem[(SCREEN_WIDTH * y + x) * 2];
    *addr = (uint8_t)c;
}

void
draw_pixel(uint8_t *video_mem, int32_t x, int32_t y, uint8_t color)
{
    uint8_t *addr = &video_mem[(SCREEN_WIDTH * y + x) * 2 + 1];
    *addr &= 0x88;
    *addr |= (color << 4);
    *addr |= (color);
}

void
set_highlight(uint8_t *video_mem, int32_t x, int32_t y, bool highlight)
{
    uint8_t *addr = &video_mem[(SCREEN_WIDTH * y + x) * 2 + 1];
    if (highlight) {
        *addr |= 0x88;
    } else {
        *addr &= 0x77;
    }
}

void
draw_palette(uint8_t *video_mem)
{
    /* 3-bit color, yay! */
    int32_t i, j, k;
    for (i = 0; i < NUM_COLORS; ++i) {
        for (j = 0; j < PALETTE_WIDTH; ++j) {
            for (k = 0; k < PALETTE_HEIGHT; ++k) {
                int32_t x = PALETTE_WIDTH * i + j;
                int32_t y = SCREEN_HEIGHT - PALETTE_HEIGHT + k;
                draw_pixel(video_mem, x, y, i);
                set_highlight(video_mem, x, y, HIGHLIGHT_BG);
            }
        }
    }
}

bool
update_palette(int32_t sx, int32_t sy, uint8_t *selected_color)
{
    if (sx < 0 || sx >= PALETTE_WIDTH * NUM_COLORS) {
        return false;
    }

    if (sy < SCREEN_HEIGHT - PALETTE_HEIGHT || sy >= SCREEN_HEIGHT) {
        return false;
    }

    if (selected_color != NULL) {
        *selected_color = sx / PALETTE_WIDTH;
    }
    return true;
}

void
clear_screen(uint8_t *video_mem, uint8_t color)
{
    int32_t i, j;
    for (i = 0; i < SCREEN_WIDTH; ++i) {
        for (j = 0; j < SCREEN_HEIGHT; ++j) {
            draw_char(video_mem, i, j, ' ');
            draw_pixel(video_mem, i, j, color);
            set_highlight(video_mem, i, j, HIGHLIGHT_BG);
        }
    }
}

void
clamp_coords(int32_t *x, int32_t *y)
{
    if (*x < 0) {
        *x = 0;
    } else if (*x >= CANVAS_WIDTH) {
        *x = CANVAS_WIDTH - 1;
    }

    if (*y < 0) {
        *y = 0;
    } else if (*y >= CANVAS_HEIGHT) {
        *y = CANVAS_HEIGHT - 1;
    }
}

void
canvas_to_screen(int32_t cx, int32_t cy, int32_t *sx, int32_t *sy)
{
    *sx = cx / SCALE_FACTOR_X;
    *sy = SCREEN_HEIGHT - 1 - cy / SCALE_FACTOR_Y;
}

void
sig_interrupt_handler(void)
{
    haz_interrupt = true;
}

int
main(void)
{
    /* Set signal handler */
    if (ece391_set_handler(INTERRUPT, (void *)sig_interrupt_handler) < 0) {
        puts("Could not set interrupt handler\n");
        return 1;
    }

    /* Open mouse file */
    int mouse_fd = ece391_open((uint8_t *)"mouse");
    if (mouse_fd < 0) {
        puts("Could not open mouse file\n");
        return 1;
    }

    /* Create vidmap page */
    uint8_t *video_mem;
    if (ece391_vidmap(&video_mem) < 0) {
        puts("Could not create vidmap page\n");
        return 1;
    }

    /* Clear the screen and draw the palette */
    clear_screen(video_mem, COLOR_BG);
    draw_palette(video_mem);

    /* Some state variables... */
    int32_t prev_cx = CANVAS_WIDTH / 2;
    int32_t prev_cy = CANVAS_HEIGHT / 2;
    uint8_t selected_color = COLOR_RED;
    mouse_input_t input;

    while (1) {
        /* If user pressed CTRL-C, clear the screen */
        if (haz_interrupt) {
            clear_screen(video_mem, COLOR_BG);
            draw_palette(video_mem);
            haz_interrupt = false;
        }

        /* Read one mouse input packet */
        if (!read_mouse_input(mouse_fd, &input)) {
            continue;
        }

        /* Undraw cursor at old location */
        int32_t prev_sx, prev_sy;
        canvas_to_screen(prev_cx, prev_cy, &prev_sx, &prev_sy);
        set_highlight(video_mem, prev_sx, prev_sy, HIGHLIGHT_BG);

        /* Compute new canvas location */
        int32_t new_cx = prev_cx + input.dx * MOUSE_SPEED;
        int32_t new_cy = prev_cy + input.dy * MOUSE_SPEED;
        clamp_coords(&new_cx, &new_cy);

        /* Draw cursor at new location */
        int32_t new_sx, new_sy;
        canvas_to_screen(new_cx, new_cy, &new_sx, &new_sy);
        set_highlight(video_mem, new_sx, new_sy, HIGHLIGHT_FG);

        /* Draw/erase pixel under cursor */
        if (input.left) {
            if (!update_palette(new_sx, new_sy, &selected_color)) {
                draw_pixel(video_mem, new_sx, new_sy, selected_color);
            }
        } else if (input.right) {
            if (!update_palette(new_sx, new_sy, NULL)) {
                draw_pixel(video_mem, new_sx, new_sy, COLOR_BG);
            }
        }

        prev_cx = new_cx;
        prev_cy = new_cy;
    }

    return 0;
}
