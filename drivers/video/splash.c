/*
 * NEXUS OS - Graphical Boot Splash Screen
 * drivers/video/splash.c
 *
 * Renders the NEXUS boot screen directly into the Multiboot2 framebuffer:
 * a vertical gradient backdrop, the NEXUS hex logo, an animated progress
 * bar with stage markers, and live status text.
 *
 * Everything here draws pixels. The previous version filled the screen and
 * then tried to report progress through console_print(), which goes to the
 * VGA text buffer and the serial port - neither of which is visible once
 * GRUB has put the machine into a linear framebuffer graphics mode. That is
 * why the graphical boot looked like an empty coloured rectangle.
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "font8x16.h"
#include "console.h"

/*===========================================================================*/
/*                         SPLASH CONFIGURATION                              */
/*===========================================================================*/

/* Backdrop gradient (top -> bottom) */
#define SPLASH_BG_TOP       0x12121F   /* near-black navy  */
#define SPLASH_BG_BOTTOM    0x1E2A4A   /* lighter navy     */

/* Accent palette - matches the login screen / desktop theme */
#define SPLASH_ACCENT       0x16C79A   /* NEXUS teal       */
#define SPLASH_ACCENT_HI    0x3BF7CE   /* teal highlight   */
#define SPLASH_TEXT         0xE8F0F2   /* near-white       */
#define SPLASH_TEXT_DIM     0x7A8CA6   /* muted blue-grey  */
#define SPLASH_TRACK        0x24304A   /* progress track   */
#define SPLASH_GLOW         0x0F3460

#define PROGRESS_BAR_WIDTH  420
#define PROGRESS_BAR_HEIGHT 6

/* Boot stages shown under the bar as tick marks */
#define SPLASH_STAGE_COUNT  6

static const char *splash_stage_names[SPLASH_STAGE_COUNT] = {
    "Kernel",
    "Memory",
    "Devices",
    "Services",
    "Desktop",
    "Ready"
};

/*===========================================================================*/
/*                         SPLASH STATE                                      */
/*===========================================================================*/

typedef struct {
    bool initialized;
    u32 width;
    u32 height;
    u32 *framebuffer;
    u32 pitch;          /* bytes per scanline */
    u32 bpp;

    u32 progress;       /* 0..100 as displayed             */
    u32 progress_target;/* where the bar is animating to    */
    u32 stage;
    u32 frame;          /* animation tick counter          */
    u32 spinner;        /* rotating dot index              */
} splash_state_t;

static splash_state_t g_splash;

/*===========================================================================*/
/*                         PIXEL PRIMITIVES                                  */
/*===========================================================================*/

static inline u32 splash_stride_px(void)
{
    return g_splash.pitch / 4;
}

static inline void splash_put_pixel(s32 x, s32 y, u32 color)
{
    if (!g_splash.initialized || x < 0 || y < 0)
        return;
    if ((u32)x >= g_splash.width || (u32)y >= g_splash.height)
        return;

    g_splash.framebuffer[(u32)y * splash_stride_px() + (u32)x] = color;
}

/* Blend `color` over the existing pixel with the given alpha (0..255) */
static inline void splash_blend_pixel(s32 x, s32 y, u32 color, u8 alpha)
{
    if (!g_splash.initialized || x < 0 || y < 0)
        return;
    if ((u32)x >= g_splash.width || (u32)y >= g_splash.height)
        return;
    if (alpha == 0)
        return;

    u32 *dst = &g_splash.framebuffer[(u32)y * splash_stride_px() + (u32)x];
    u32 old = *dst;

    u32 orr = (old >> 16) & 0xFF, og = (old >> 8) & 0xFF, ob = old & 0xFF;
    u32 nr = (color >> 16) & 0xFF, ng = (color >> 8) & 0xFF, nb = color & 0xFF;

    u32 r = (orr * (255 - alpha) + nr * alpha) / 255;
    u32 g = (og  * (255 - alpha) + ng * alpha) / 255;
    u32 b = (ob  * (255 - alpha) + nb * alpha) / 255;

    *dst = (r << 16) | (g << 8) | b;
}

static void splash_draw_rect(s32 x, s32 y, u32 w, u32 h, u32 color)
{
    for (u32 py = 0; py < h; py++)
        for (u32 px = 0; px < w; px++)
            splash_put_pixel(x + (s32)px, y + (s32)py, color);
}

/* Filled rectangle with rounded corners, radius in pixels */
static void splash_draw_rounded_rect(s32 x, s32 y, u32 w, u32 h,
                                     u32 radius, u32 color)
{
    if (radius == 0) {
        splash_draw_rect(x, y, w, h, color);
        return;
    }

    /* Middle band spans the full width */
    splash_draw_rect(x, y + (s32)radius, w, h - 2 * radius, color);

    /* Top and bottom bands are inset by the corner radius */
    splash_draw_rect(x + (s32)radius, y, w - 2 * radius, radius, color);
    splash_draw_rect(x + (s32)radius, y + (s32)(h - radius),
                     w - 2 * radius, radius, color);

    /* Corner quarter-discs */
    s32 r = (s32)radius;
    for (s32 dy = 0; dy < r; dy++) {
        for (s32 dx = 0; dx < r; dx++) {
            if ((dx * dx + dy * dy) <= r * r) {
                /* top-left */
                splash_put_pixel(x + dx, y + dy, color);
                /* top-right */
                splash_put_pixel(x + (s32)(w - 1) - dx, y + dy, color);
                /* bottom-left */
                splash_put_pixel(x + dx, y + (s32)(h - 1) - dy, color);
                /* bottom-right */
                splash_put_pixel(x + (s32)(w - 1) - dx,
                                 y + (s32)(h - 1) - dy, color);
            }
        }
    }
}

/* Vertical gradient background, drawn once */
static void splash_draw_background(void)
{
    u32 h = g_splash.height;

    u32 tr = (SPLASH_BG_TOP >> 16) & 0xFF;
    u32 tg = (SPLASH_BG_TOP >> 8) & 0xFF;
    u32 tb = SPLASH_BG_TOP & 0xFF;

    u32 br = (SPLASH_BG_BOTTOM >> 16) & 0xFF;
    u32 bg = (SPLASH_BG_BOTTOM >> 8) & 0xFF;
    u32 bb = SPLASH_BG_BOTTOM & 0xFF;

    for (u32 y = 0; y < h; y++) {
        u32 t = h ? (y * 255 / h) : 0;

        u32 r = (tr * (255 - t) + br * t) / 255;
        u32 g = (tg * (255 - t) + bg * t) / 255;
        u32 b = (tb * (255 - t) + bb * t) / 255;

        u32 color = (r << 16) | (g << 8) | b;
        for (u32 x = 0; x < g_splash.width; x++)
            splash_put_pixel((s32)x, (s32)y, color);
    }
}

/*
 * splash_draw_radial_glow - soft circular glow behind the logo.
 *
 * Cheap approximation: concentric rings with decreasing alpha, which is
 * plenty for a boot screen and costs no floating point.
 */
static void splash_draw_radial_glow(s32 cx, s32 cy, u32 radius, u32 color)
{
    for (u32 r = radius; r > 0; r--) {
        u8 alpha = (u8)(36 * r / radius);

        s32 x = (s32)r;
        s32 y = 0;
        s32 err = 0;

        while (x >= y) {
            splash_blend_pixel(cx + y, cy + x, color, alpha);
            splash_blend_pixel(cx - y, cy + x, color, alpha);
            splash_blend_pixel(cx + y, cy - x, color, alpha);
            splash_blend_pixel(cx - y, cy - x, color, alpha);
            splash_blend_pixel(cx + x, cy + y, color, alpha);
            splash_blend_pixel(cx - x, cy + y, color, alpha);
            splash_blend_pixel(cx + x, cy - y, color, alpha);
            splash_blend_pixel(cx - x, cy - y, color, alpha);

            y++;
            err += 1 + 2 * y;
            if (2 * (err - x) + 1 > 0) {
                x--;
                err += 1 - 2 * x;
            }
        }
    }
}

/* Filled circle */
static void splash_draw_circle(s32 cx, s32 cy, u32 radius, u32 color)
{
    s32 x = (s32)radius;
    s32 y = 0;
    s32 err = 0;

    while (x >= y) {
        splash_draw_rect(cx - y, cy - x, (u32)(y * 2 + 1), (u32)(x * 2 + 1), color);
        splash_draw_rect(cx - x, cy - y, (u32)(x * 2 + 1), (u32)(y * 2 + 1), color);
        splash_draw_rect(cx + x - y + 1, cy - y, (u32)(y * 2 + 1), (u32)(y * 2 + 1), color);
        splash_draw_rect(cx - x, cy + x - y + 1, (u32)(y * 2 + 1), (u32)(y * 2 + 1), color);

        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

/* Line, Bresenham */
static void splash_draw_line(s32 x0, s32 y0, s32 x1, s32 y1,
                             u32 color, u32 thickness)
{
    s32 dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    s32 dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    s32 sx = (x0 < x1) ? 1 : -1;
    s32 sy = (y0 < y1) ? 1 : -1;
    s32 err = dx - dy;

    for (;;) {
        if (thickness <= 1) {
            splash_put_pixel(x0, y0, color);
        } else {
            splash_draw_rect(x0 - (s32)(thickness / 2),
                             y0 - (s32)(thickness / 2),
                             thickness, thickness, color);
        }

        if (x0 == x1 && y0 == y1)
            break;

        s32 e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/*===========================================================================*/
/*                         LOGO                                              */
/*===========================================================================*/

/*
 * splash_draw_logo - Draw the NEXUS mark.
 *
 * A hexagonal outline with an inner chevron, i.e. a geometric "N", rather
 * than the previous placeholder (a green disc with a filled rectangle in
 * it). Sized relative to the framebuffer so it scales from 640x480 up to
 * 1920x1080.
 */
static void splash_draw_logo(s32 cx, s32 cy, u32 size, u8 alpha)
{
    s32 r = (s32)(size / 2);

    /* Soft glow behind the mark */
    splash_draw_radial_glow(cx, cy, (u32)(r * 2), SPLASH_ACCENT);

    /*
     * Hexagon vertices. Using flat-top orientation reads better as a mark
     * and gives a stable silhouette at boot.
     */
    s32 hx[6], hy[6];
    for (int i = 0; i < 6; i++) {
        /* Precomputed cos/sin for 0,60,...,300 deg, scaled to r */
        static const s32 cos60[6] = { 1000, 500, -500, -1000, -500, 500 };
        static const s32 sin60[6] = { 0, 866, 866, 0, -866, -866 };
        hx[i] = cx + (r * cos60[i]) / 1000;
        hy[i] = cy + (r * sin60[i]) / 1000;
    }

    u32 edge_alpha = alpha;
    for (int i = 0; i < 6; i++) {
        int j = (i + 1) % 6;

        /*
         * Draw the edge with a manual alpha blend so the whole logo can be
         * faded during the intro animation.
         */
        if (edge_alpha >= 255) {
            splash_draw_line(hx[i], hy[i], hx[j], hy[j], SPLASH_ACCENT, 3);
        } else {
            s32 steps = 48;
            for (s32 s = 0; s <= steps; s++) {
                s32 px = hx[i] + (hx[j] - hx[i]) * s / steps;
                s32 py = hy[i] + (hy[j] - hy[i]) * s / steps;
                for (s32 t = -1; t <= 1; t++) {
                    splash_blend_pixel(px + t, py, SPLASH_ACCENT, edge_alpha);
                    splash_blend_pixel(px, py + t, SPLASH_ACCENT, edge_alpha);
                }
            }
        }
    }

    /* Inner "N" chevron: left stroke, diagonal, right stroke */
    s32 inset = r / 3;
    s32 left_x  = cx - inset;
    s32 right_x = cx + inset;
    s32 top_y   = cy - (r / 2);
    s32 bot_y   = cy + (r / 2);

    if (alpha >= 255) {
        splash_draw_line(left_x, bot_y, left_x, top_y, SPLASH_ACCENT_HI, 3);
        splash_draw_line(left_x, top_y, right_x, bot_y, SPLASH_ACCENT_HI, 3);
        splash_draw_line(right_x, bot_y, right_x, top_y, SPLASH_ACCENT_HI, 3);
    } else {
        s32 steps = 40;
        for (s32 s = 0; s <= steps; s++) {
            s32 ax = left_x + (right_x - left_x) * s / steps;
            s32 ay = top_y + (bot_y - top_y) * s / steps;
            splash_blend_pixel(ax, ay, SPLASH_ACCENT_HI, alpha);
            splash_blend_pixel(ax, ay + 1, SPLASH_ACCENT_HI, alpha);
        }
        for (s32 s = 0; s <= steps; s++) {
            s32 ay = top_y + (bot_y - top_y) * s / steps;
            splash_blend_pixel(left_x, ay, SPLASH_ACCENT_HI, alpha);
            splash_blend_pixel(left_x + 1, ay, SPLASH_ACCENT_HI, alpha);
            splash_blend_pixel(right_x, ay, SPLASH_ACCENT_HI, alpha);
            splash_blend_pixel(right_x - 1, ay, SPLASH_ACCENT_HI, alpha);
        }
    }
}

/*===========================================================================*/
/*                         TEXT                                              */
/*===========================================================================*/

static void splash_text_centered(s32 cx, s32 y, const char *s, u32 color,
                                 u32 scale, u8 alpha)
{
    font8x16_draw_string_centered(g_splash.framebuffer,
                                  g_splash.width, g_splash.height,
                                  g_splash.pitch, cx, y, s, color, scale, alpha);
}

/*===========================================================================*/
/*                         PROGRESS BAR                                      */
/*===========================================================================*/

/*
 * splash_draw_progress - Draw the progress track, fill and stage ticks.
 *
 * Rounded track, gradient-less two-tone fill, and a bright leading edge so
 * the bar reads as "moving" even on a static screenshot.
 */
static void splash_draw_progress(u32 percent)
{
    if (percent > 100)
        percent = 100;

    s32 bar_w = PROGRESS_BAR_WIDTH;
    s32 bar_h = PROGRESS_BAR_HEIGHT;
    s32 bar_x = (s32)(g_splash.width / 2) - bar_w / 2;
    s32 bar_y = (s32)(g_splash.height * 2 / 3);

    if (bar_w > (s32)g_splash.width - 40) {
        bar_w = (s32)g_splash.width - 40;
        bar_x = 20;
    }

    /* Track */
    splash_draw_rounded_rect(bar_x, bar_y, (u32)bar_w, (u32)bar_h,
                             (u32)(bar_h / 2), SPLASH_TRACK);

    /* Fill */
    u32 fill = (u32)(bar_w - 4) * percent / 100;
    if (fill > 2) {
        splash_draw_rounded_rect(bar_x + 2, bar_y + 2, fill,
                                 (u32)(bar_h - 4),
                                 (u32)((bar_h - 4) / 2), SPLASH_ACCENT);

        /* Leading-edge highlight */
        s32 edge_x = bar_x + 2 + (s32)fill - 2;
        if (edge_x > bar_x + 2) {
            splash_draw_rect(edge_x, bar_y + 2, 2, (u32)(bar_h - 4),
                             SPLASH_ACCENT_HI);
        }
    }

    /* Stage tick marks under the bar */
    s32 tick_y = bar_y + bar_h + 8;
    for (u32 i = 0; i < SPLASH_STAGE_COUNT; i++) {
        s32 tx = bar_x + (s32)((bar_w * (s32)i) / (SPLASH_STAGE_COUNT - 1));
        if (i == SPLASH_STAGE_COUNT - 1)
            tx = bar_x + bar_w - 2;

        u32 color = (i <= g_splash.stage) ? SPLASH_ACCENT : SPLASH_TRACK;
        splash_draw_rect(tx, tick_y, 2, 6, color);
    }
}

/*
 * splash_draw_spinner - Three-dot activity indicator.
 *
 * Cheap motion cue that makes the screen feel alive while a long init step
 * (heap, drivers) runs, without needing a timer interrupt.
 */
static void splash_draw_spinner(s32 cx, s32 y)
{
    for (u32 i = 0; i < 3; i++) {
        u32 alpha = (i == (g_splash.spinner % 3)) ? 255 : 70;
        s32 dx = ((s32)i - 1) * 12;

        u32 *dst = NULL;
        s32 px = cx + dx;
        if (px >= 0 && (u32)px < g_splash.width && y >= 0 &&
            (u32)y < g_splash.height) {
            dst = &g_splash.framebuffer[(u32)y * splash_stride_px() + (u32)px];
        }
        if (dst) {
            splash_draw_circle(px, y, 3, SPLASH_ACCENT);
            if (alpha < 255) {
                /* dim the inactive dots */
                splash_draw_circle(px, y, 3, SPLASH_BG_BOTTOM);
                splash_blend_pixel(px, y, SPLASH_ACCENT, (u8)alpha);
            }
        }
    }
}

/*===========================================================================*/
/*                         FULL SCREEN COMPOSITION                           */
/*===========================================================================*/

/*
 * splash_render - Repaint the whole splash.
 *
 * Called on init and whenever progress/stage changes. Full repaint is used
 * deliberately: at boot frame times do not matter, and it keeps the code
 * free of damage-tracking bugs.
 */
static void splash_render(u8 logo_alpha)
{
    if (!g_splash.initialized)
        return;

    splash_draw_background();

    s32 cx = (s32)(g_splash.width / 2);
    u32 logo_size = g_splash.height / 4;
    if (logo_size < 96)  logo_size = 96;
    if (logo_size > 220) logo_size = 220;

    s32 logo_cy = (s32)(g_splash.height / 3);

    splash_draw_logo(cx, logo_cy, logo_size, logo_alpha);

    /* Wordmark under the mark */
    u32 title_scale = (g_splash.height >= 720) ? 3 : 2;
    splash_text_centered(cx, logo_cy + (s32)(logo_size / 2) + 24,
                         "NEXUS OS", SPLASH_TEXT, title_scale, 255);

    splash_text_centered(cx, logo_cy + (s32)(logo_size / 2) + 24 +
                             (s32)(FONT8X16_HEIGHT * title_scale) + 8,
                         "Genesis Edition", SPLASH_TEXT_DIM, 1, 255);

    /* Progress bar + stage label */
    splash_draw_progress(g_splash.progress);

    s32 bar_y = (s32)(g_splash.height * 2 / 3);
    const char *stage_name = (g_splash.stage < SPLASH_STAGE_COUNT)
                                 ? splash_stage_names[g_splash.stage]
                                 : "Ready";

    char line[64];
    snprintf(line, sizeof(line), "%s  %u%%", stage_name, g_splash.progress);
    splash_text_centered(cx, bar_y + PROGRESS_BAR_HEIGHT + 26, line,
                         SPLASH_TEXT_DIM, 1, 255);

    /* Footer */
    splash_text_centered(cx, (s32)g_splash.height - 32,
                         "nexus-os  v1.0.0-alpha", SPLASH_TEXT_DIM, 1, 255);

    splash_draw_spinner(cx, (s32)g_splash.height - 64);
}

/*===========================================================================*/
/*                         PUBLIC API                                        */
/*===========================================================================*/

/**
 * splash_init - Initialize the graphical boot splash
 */
void splash_init(u32 *fb, u32 width, u32 height, u32 pitch, u32 bpp)
{
    if (!fb || width == 0 || height == 0) {
        printk("[SPLASH] No framebuffer supplied, graphical splash skipped\n");
        return;
    }

    /*
     * The renderer writes 32-bit XRGB pixels. Rendering into a 16- or 24-bpp
     * surface would produce colour garbage rather than a splash, so refuse
     * the framebuffer instead of pretending to draw on it.
     */
    if (bpp && bpp != 32) {
        printk("[SPLASH] Framebuffer is %ubpp; the splash renderer needs 32bpp "
               "- graphical splash skipped (console output continues)\n", bpp);
        return;
    }

    g_splash.framebuffer = fb;
    g_splash.width = width;
    g_splash.height = height;
    g_splash.pitch = pitch ? pitch : width * 4;
    g_splash.bpp = bpp ? bpp : 32;

    g_splash.progress = 0;
    g_splash.progress_target = 0;
    g_splash.stage = 0;
    g_splash.frame = 0;
    g_splash.spinner = 0;
    g_splash.initialized = true;

    splash_render(0);

    printk("[SPLASH] Graphical boot screen active: %ux%u @ %ubpp\n",
           width, height, g_splash.bpp);
}

/**
 * splash_update_progress - Advance the boot progress bar
 */
void splash_update_progress(u32 percent, const char *status)
{
    if (!g_splash.initialized)
        return;

    g_splash.progress_target = (percent > 100) ? 100 : percent;

    /*
     * Ease toward the target instead of jumping, but never by less than a
     * quarter of the remaining distance: boot only reports a handful of
     * milestones, and a fixed 4%/call step meant "45%" drew a 16% bar.
     */
    if (g_splash.progress < g_splash.progress_target) {
        u32 delta = g_splash.progress_target - g_splash.progress;
        u32 step = (delta * 3) / 4;      /* close most of the gap at once */
        if (step < 4) {
            step = 4;
        }
        g_splash.progress += step;
        if (g_splash.progress > g_splash.progress_target) {
            g_splash.progress = g_splash.progress_target;
        }
    } else if (g_splash.progress > g_splash.progress_target) {
        g_splash.progress = g_splash.progress_target;
    }

    /* Map percent onto a stage index for the tick marks and label */
    g_splash.stage = (g_splash.progress * SPLASH_STAGE_COUNT) / 100;
    if (g_splash.stage >= SPLASH_STAGE_COUNT) {
        g_splash.stage = SPLASH_STAGE_COUNT - 1;
    }

    g_splash.spinner++;
    splash_render(255);

    if (status) {
        printk("[SPLASH] %3u%%  %s\n", g_splash.progress, status);
    }
}

/**
 * splash_show_message - Show a transient message above the progress bar
 */
void splash_show_message(const char *message)
{
    if (!g_splash.initialized || !message)
        return;

    s32 cx = (s32)(g_splash.width / 2);
    s32 msg_y = (s32)(g_splash.height * 2 / 3) - 34;

    /* Clear the message strip first so messages do not overprint */
    splash_draw_rect(0, msg_y - 2, g_splash.width,
                     (u32)(FONT8X16_HEIGHT + 4), SPLASH_BG_BOTTOM);

    splash_text_centered(cx, msg_y, message, SPLASH_ACCENT_HI, 1, 255);
}

/**
 * splash_done - Finish the splash (leaves the final frame on screen)
 */
void splash_done(void)
{
    if (!g_splash.initialized)
        return;

    g_splash.progress = 100;
    g_splash.stage = SPLASH_STAGE_COUNT - 1;
    g_splash.progress_target = 100;
    splash_render(255);

    /*
     * Release the framebuffer. init.c calls splash_done() immediately
     * before starting the compositor and desktop; leaving initialized set
     * kept the timer tick repainting the boot screen over the desktop
     * forever.
     */
    g_splash.initialized = false;

    printk("[SPLASH] Boot splash complete, framebuffer released\n");
}

/**
 * splash_animate - Advance one animation frame.
 *
 * Safe to call from either a polling loop or the timer tick; it only
 * redraws when the spinner phase actually changes, so it stays cheap.
 */
void splash_animate(void)
{
    if (!g_splash.initialized)
        return;

    g_splash.frame++;

    /* ~ every 16 frames, step the spinner and smooth the bar */
    if ((g_splash.frame & 0xF) != 0)
        return;

    g_splash.spinner++;

    /*
     * Close the remaining gap between animation frames. A flat +1 was far
     * too slow to be seen: boot reports a milestone every few hundred ms
     * and the bar would still be catching up long after it had moved on.
     */
    if (g_splash.progress < g_splash.progress_target) {
        u32 delta = g_splash.progress_target - g_splash.progress;
        u32 step = delta / 4;
        g_splash.progress += (step < 1) ? 1 : step;
        if (g_splash.progress > g_splash.progress_target) {
            g_splash.progress = g_splash.progress_target;
        }
    }

    splash_render(255);
}

/**
 * splash_is_active - True while the splash is on screen
 */
bool splash_is_active(void)
{
    return g_splash.initialized;
}

/**
 * splash_test - Print splash diagnostic information
 */
void splash_test(void)
{
    console_print("NEXUS OS Splash Screen Test\n");
    console_print("Framebuffer: %ux%u @ %ubpp (pitch %u)\n",
                  g_splash.width, g_splash.height, g_splash.bpp,
                  g_splash.pitch);
    console_print("Active: %s\n", g_splash.initialized ? "yes" : "no");
}
