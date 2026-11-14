/*
 * NEXUS OS - Framebuffer Text Console
 * drivers/video/fbconsole.c
 *
 * Draws the kernel log as text directly into the linear framebuffer.
 *
 * WHY THIS EXISTS
 *
 * Every diagnostic the kernel prints went to the VGA text buffer at 0xB8000
 * and, later, to COM1. Neither is available to someone running the OS on a
 * laptop or in a VM window: once GRUB switches to a linear framebuffer that
 * text memory is not scanned out, and a serial console needs a port, a cable
 * and a terminal on the other end.
 *
 * The practical consequence was an undiagnosable class of bug. A splash that
 * failed to initialise, a framebuffer at an address that is not mapped, and a
 * 16bpp mode were all reported as "the screen is blank", because the one
 * channel that could explain it was invisible. Telling a user to "check the
 * serial log" is not a fix when they have no serial console.
 *
 * This module keeps the last chunk of the log in a RAM ring buffer and can
 * paint it into whatever framebuffer we have, at 16, 24 or 32bpp. If the
 * splash cannot draw, the boot log can still appear on the actual screen.
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "font8x16.h"

/*===========================================================================*/
/*                           CONFIGURATION                                   */
/*===========================================================================*/

#define FBCON_LOG_BYTES     4096    /* ring buffer holding recent log text  */
#define FBCON_CHAR_W        8
#define FBCON_CHAR_H        16
#define FBCON_MARGIN_X      16
#define FBCON_MARGIN_Y      16

typedef struct {
    u32  *framebuffer;      /* treated as a byte base internally */
    u32   width;
    u32   height;
    u32   pitch;            /* bytes per scanline */
    u32   bpp;
    bool  ready;

    char  log[FBCON_LOG_BYTES];
    u32   log_head;         /* next write index */
    u32   log_len;          /* how much is valid */
} fbconsole_state_t;

static fbconsole_state_t g_fbcon;

/*===========================================================================*/
/*                          PIXEL PRIMITIVES                                 */
/*===========================================================================*/

/* Same depth handling as the splash: the surface byte layout varies. */
static inline void fbcon_store(s32 x, s32 y, u32 color)
{
    if (x < 0 || y < 0 || (u32)x >= g_fbcon.width || (u32)y >= g_fbcon.height)
        return;

    u8 *row = (u8 *)g_fbcon.framebuffer + (u32)y * g_fbcon.pitch;

    switch (g_fbcon.bpp) {
    case 16:
        ((u16 *)row)[x] = (u16)(((color >> 19) & 0x1F) << 11 |
                                ((color >> 10) & 0x3F) << 5  |
                                ((color >> 3)  & 0x1F));
        break;
    case 24: {
        u8 *p = row + (u32)x * 3;
        p[0] = (u8)(color);
        p[1] = (u8)(color >> 8);
        p[2] = (u8)(color >> 16);
        break;
    }
    default:
        ((u32 *)row)[x] = color;
        break;
    }
}

static void fbcon_fill(s32 x, s32 y, u32 w, u32 h, u32 color)
{
    for (u32 py = 0; py < h; py++)
        for (u32 px = 0; px < w; px++)
            fbcon_store(x + (s32)px, y + (s32)py, color);
}

static void fbcon_char(s32 x, s32 y, char c, u32 color)
{
    const u8 *glyph = font8x16_glyph(c);

    for (u32 row = 0; row < FONT8X16_HEIGHT; row++) {
        u8 bits = glyph[row];
        if (!bits)
            continue;

        for (u32 col = 0; col < FONT8X16_WIDTH; col++) {
            if (bits & (0x80 >> col))
                fbcon_store(x + (s32)col, y + (s32)row, color);
        }
    }
}

/*===========================================================================*/
/*                            LOG CAPTURE                                    */
/*===========================================================================*/

/**
 * fbcon_capture_char - Record one character of kernel output
 * @c: character printed by printk
 *
 * Called for every character the kernel prints, so the on-screen console can
 * show what happened even when no other output channel is visible. Cheap: a
 * ring buffer append.
 */
void fbcon_capture_char(char c)
{
    if (c == '\r')
        return;

    g_fbcon.log[g_fbcon.log_head] = c;
    g_fbcon.log_head = (g_fbcon.log_head + 1) % FBCON_LOG_BYTES;

    if (g_fbcon.log_len < FBCON_LOG_BYTES)
        g_fbcon.log_len++;
}

/*===========================================================================*/
/*                              PUBLIC API                                   */
/*===========================================================================*/

/**
 * fbcon_set_framebuffer - Point the text console at a surface
 * @fb: linear framebuffer
 * @width: pixels per scanline
 * @height: scanlines
 * @pitch: bytes per scanline
 * @bpp: bits per pixel (16, 24 or 32 supported)
 */
void fbcon_set_framebuffer(void *fb, u32 width, u32 height, u32 pitch, u32 bpp)
{
    if (!fb || !width || !height)
        return;
    if (bpp != 16 && bpp != 24 && bpp != 32)
        return;

    g_fbcon.framebuffer = (u32 *)fb;
    g_fbcon.width = width;
    g_fbcon.height = height;
    g_fbcon.pitch = pitch ? pitch : width * (bpp / 8);
    g_fbcon.bpp = bpp;
    g_fbcon.ready = true;
}

bool fbcon_is_ready(void)
{
    return g_fbcon.ready;
}

/**
 * fbcon_clear - Fill the screen with a solid colour
 */
void fbcon_clear(u32 color)
{
    if (!g_fbcon.ready)
        return;

    fbcon_fill(0, 0, g_fbcon.width, g_fbcon.height, color);
}

/**
 * fbcon_render - Paint the tail of the kernel log on screen
 *
 * Shows as many whole lines as fit, newest at the bottom, so the most recent
 * (and usually most diagnostic) output is what is visible.
 */
void fbcon_render(u32 bg, u32 fg)
{
    if (!g_fbcon.ready)
        return;

    fbcon_clear(bg);

    u32 cols = (g_fbcon.width > FBCON_MARGIN_X * 2)
                   ? (g_fbcon.width - FBCON_MARGIN_X * 2) / FBCON_CHAR_W : 80;
    u32 rows = (g_fbcon.height > FBCON_MARGIN_Y * 2)
                   ? (g_fbcon.height - FBCON_MARGIN_Y * 2) / FBCON_CHAR_H : 25;

    if (cols == 0 || rows == 0)
        return;

    /*
     * Walk backwards from the newest byte to the start of the selected
     * window, then print forwards. Doing it forwards from the whole 4KB log
     * would always show the oldest lines and hide the failure.
     */
    u32 start = (g_fbcon.log_head + FBCON_LOG_BYTES - g_fbcon.log_len)
                % FBCON_LOG_BYTES;

    /*
     * Count displayable lines.
     *
     * A trailing newline must not be counted as an extra line: kernel output
     * almost always ends with one, and counting it made the window one row
     * shorter than it should be - the newest line was pushed off the bottom
     * and replaced by a blank one, which is precisely the line carrying the
     * error we are trying to show.
     */
    u32 total_lines = (g_fbcon.log_len > 0) ? 1 : 0;
    for (u32 i = 0; i < g_fbcon.log_len; i++) {
        if (g_fbcon.log[(start + i) % FBCON_LOG_BYTES] == '\n') {
            /* Only count a line break if more text follows it. */
            if (i + 1 < g_fbcon.log_len)
                total_lines++;
        }
    }

    u32 skip = (total_lines > rows) ? (total_lines - rows) : 0;

    u32 cx = 0, cy = 0, line = 0;
    for (u32 i = 0; i < g_fbcon.log_len; i++) {
        char c = g_fbcon.log[(start + i) % FBCON_LOG_BYTES];

        if (c == '\n') {
            line++;
            cx = 0;
            continue;
        }

        if (line < skip)
            continue;

        u32 out_row = line - skip;
        if (out_row >= rows)
            break;

        if (cx >= cols) {
            /* Hard wrap without eating the character. */
            cx = 0;
            out_row++;
            if (out_row >= rows)
                break;
        }

        fbcon_char((s32)(FBCON_MARGIN_X + cx * FBCON_CHAR_W),
                   (s32)(FBCON_MARGIN_Y + out_row * FBCON_CHAR_H),
                   c, fg);
        cx++;
    }
}

/**
 * fbcon_render_message - Show a single line, e.g. when init cannot proceed
 */
void fbcon_render_message(const char *msg, u32 bg, u32 fg)
{
    if (!g_fbcon.ready)
        return;

    fbcon_clear(bg);

    if (!msg)
        return;

    u32 col = 0;
    for (const char *p = msg; *p; p++) {
        if (*p == '\n') {
            break;
        }
        fbcon_char((s32)(FBCON_MARGIN_X + col * FBCON_CHAR_W),
                   FBCON_MARGIN_Y, *p, fg);
        col++;
    }
}
