#include <attrib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <syscall.h>

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

/* Used to reset color upon exit */
#define TERM_ATTRIB 0x7

/* Number of inputs to read at once */
#define MOUSE_BUF_SIZE 64

/* Raw mouse input, straight from the kernel */
typedef struct {
    uint8_t flags;
    uint8_t dx;
    uint8_t dy;
} raw_mouse_input_t;

/* Pre-processed mouse input */
typedef struct {
    int dx;
    int dy;
    bool left : 1;
    bool right : 1;
    bool middle : 1;
} mouse_input_t;

static bool haz_interrupt = false;

static bool
parse_mouse_input(raw_mouse_input_t in, mouse_input_t *out)
{
    uint8_t flags = in.flags;

    /* Discard packet if overflow */
    if ((flags & (MOUSE_X_OVERFLOW | MOUSE_Y_OVERFLOW)) != 0) {
        return false;
    }

    /* Delta x movement */
    int dx = in.dx;
    if ((flags & MOUSE_X_SIGN) != 0) {
        dx |= 0xffffff00;
    }

    /* Delta y movement */
    int dy = in.dy;
    if ((flags & MOUSE_Y_SIGN) != 0) {
        dy |= 0xffffff00;
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

static int
parse_mouse_inputs(
    int num_inputs,
    raw_mouse_input_t in[MOUSE_BUF_SIZE],
    mouse_input_t out[MOUSE_BUF_SIZE])
{
    int num = 0;
    int i;
    for (i = 0; i < num_inputs; ++i) {
        if (parse_mouse_input(in[i], &out[num])) {
            num++;
        }
    }
    return num;
}

static int
read_mouse_inputs(int fd, mouse_input_t out[MOUSE_BUF_SIZE])
{
    raw_mouse_input_t raw[MOUSE_BUF_SIZE];
    int ret = read(fd, raw, sizeof(raw));
    if (ret < 0) {
        return ret;
    }

    return parse_mouse_inputs(ret / sizeof(raw_mouse_input_t), raw, out);
}

static void
draw_char(uint8_t *video_mem, int x, int y, char c)
{
    uint8_t *addr = &video_mem[(SCREEN_WIDTH * y + x) * 2];
    *addr = (uint8_t)c;
}

static void
draw_pixel(uint8_t *video_mem, int x, int y, uint8_t color)
{
    uint8_t *addr = &video_mem[(SCREEN_WIDTH * y + x) * 2 + 1];
    *addr &= 0x88;
    *addr |= (color << 4);
    *addr |= (color);
}

static void
set_highlight(uint8_t *video_mem, int x, int y, bool highlight)
{
    uint8_t *addr = &video_mem[(SCREEN_WIDTH * y + x) * 2 + 1];
    if (highlight) {
        *addr |= 0x88;
    } else {
        *addr &= 0x77;
    }
}

static void
draw_palette(uint8_t *video_mem)
{
    int i, j, k;
    for (i = 0; i < NUM_COLORS; ++i) {
        for (j = 0; j < PALETTE_WIDTH; ++j) {
            for (k = 0; k < PALETTE_HEIGHT; ++k) {
                int x = PALETTE_WIDTH * i + j;
                int y = SCREEN_HEIGHT - PALETTE_HEIGHT + k;
                draw_pixel(video_mem, x, y, i);
                set_highlight(video_mem, x, y, HIGHLIGHT_BG);
            }
        }
    }
}

static bool
update_palette(int sx, int sy, uint8_t *selected_color)
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

static void
clear_screen(uint8_t *video_mem, uint8_t color)
{
    int i, j;
    for (i = 0; i < SCREEN_HEIGHT; ++i) {
        for (j = 0; j < SCREEN_WIDTH; ++j) {
            draw_char(video_mem, j, i, ' ');
            draw_pixel(video_mem, j, i, color);
            set_highlight(video_mem, j, i, HIGHLIGHT_BG);
        }
    }
}

static void
reset_screen(uint8_t *video_mem)
{
    int i, j;
    for (i = 0; i < SCREEN_HEIGHT; ++i) {
        for (j = 0; j < SCREEN_WIDTH; ++j) {
            video_mem[(SCREEN_WIDTH * i + j) * 2 + 1] = TERM_ATTRIB;
        }
    }
}

static void
clamp_coords(int *x, int *y)
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

static void
canvas_to_screen(int cx, int cy, int *sx, int *sy)
{
    *sx = cx / SCALE_FACTOR_X;
    *sy = SCREEN_HEIGHT - 1 - cy / SCALE_FACTOR_Y;
}

__cdecl static void
sigint_handler(int signum)
{
    haz_interrupt = true;
}

int
main(void)
{
    int ret = 1;
    int mouse_fd = -1;

    /* Set signal handler */
    if (sigaction(SIGINT, sigint_handler) < 0) {
        fprintf(stderr, "Could not set interrupt handler\n");
        goto exit;
    }

    /* Open mouse file */
    if ((mouse_fd = create("mouse", OPEN_READ)) < 0) {
        fprintf(stderr, "Could not open mouse file\n");
        goto exit;
    }

    /* Create vidmap page */
    uint8_t *video_mem;
    if (vidmap(&video_mem) < 0) {
        fprintf(stderr, "Could not create vidmap page\n");
        goto exit;
    }

    /* Clear the screen and draw the palette */
    clear_screen(video_mem, COLOR_BG);
    draw_palette(video_mem);

    /* Some state variables... */
    int prev_cx = CANVAS_WIDTH / 2;
    int prev_cy = CANVAS_HEIGHT / 2;
    uint8_t selected_color = COLOR_RED;
    mouse_input_t inputs[MOUSE_BUF_SIZE];

    while (1) {
        /* If user pressed CTRL-C, reset the screen and exit */
        if (haz_interrupt) {
            reset_screen(video_mem);
            break;
        }

        /* Read some more mouse inputs */
        int num_inputs = read_mouse_inputs(mouse_fd, inputs);
        if (num_inputs == -EAGAIN || num_inputs == 0) {
            continue;
        }

        int i;
        for (i = 0; i < num_inputs; ++i) {
            mouse_input_t input = inputs[i];

            /* Undraw cursor at old location */
            int prev_sx, prev_sy;
            canvas_to_screen(prev_cx, prev_cy, &prev_sx, &prev_sy);
            set_highlight(video_mem, prev_sx, prev_sy, HIGHLIGHT_BG);

            /* Compute new canvas location */
            int new_cx = prev_cx + input.dx * MOUSE_SPEED;
            int new_cy = prev_cy + input.dy * MOUSE_SPEED;
            clamp_coords(&new_cx, &new_cy);

            /* Draw cursor at new location */
            int new_sx, new_sy;
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
    }

    ret = 0;

exit:
    if (mouse_fd >= 0) close(mouse_fd);
    return ret;
}
