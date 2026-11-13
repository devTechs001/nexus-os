/*
 * NEXUS OS - VM Manager GUI Render Backend
 * gui/virtualization/virt_render.c
 *
 * Real 32bpp framebuffer drawing for the Virtualization Manager.
 *
 * Every gui_draw_* entry point in virt_manager_gui.c used to be a printk()
 * stub, so the "GUI" could never actually appear on screen. This file
 * provides the pixel-level primitives those functions need: clipped
 * rectangles, rounded rectangles, lines, gradients, circles and text via
 * the 8x16 bitmap font.
 *
 * All primitives clip against the destination surface, so a panel that
 * runs past the right edge of the framebuffer is simply cut off instead
 * of scribbling over unrelated memory.
 *
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include "../../drivers/video/font8x16.h"
#include "virt_render.h"

/* Active render target. NULL means "nothing to draw on". */
static virt_surface_t g_surface;
static bool g_surface_ready = false;

/*===========================================================================*/
/*                         SURFACE MANAGEMENT                                */
/*===========================================================================*/

int virt_render_init(u32 *fb, u32 width, u32 height, u32 pitch)
{
    if (!fb || width == 0 || height == 0) {
        g_surface_ready = false;
        return -1;
    }

    g_surface.fb = fb;
    g_surface.width = width;
    g_surface.height = height;
    g_surface.pitch = pitch ? pitch : (width * 4);
    g_surface.ready = true;
    g_surface_ready = true;

    printk("[VIRT-RENDER] Target %ux%u, pitch %u bytes (%u px)\n",
           width, height, g_surface.pitch, g_surface.pitch / 4);

    return 0;
}

/* Pull the framebuffer the bootloader/display layer already set up. */
int virt_render_attach_display(void)
{
    display_framebuffer_t *fb = display_get_primary_framebuffer();

    if (fb && fb->base && fb->width && fb->height) {
        return virt_render_init((u32 *)fb->base, fb->width, fb->height, fb->stride);
    }

    printk("[VIRT-RENDER] No graphics framebuffer available, GUI stays headless\n");
    g_surface_ready = false;
    return -1;
}

bool virt_render_ready(void)
{
    return g_surface_ready && g_surface.fb != NULL;
}

u32 virt_render_width(void)
{
    return g_surface_ready ? g_surface.width : 0;
}

u32 virt_render_height(void)
{
    return g_surface_ready ? g_surface.height : 0;
}

virt_surface_t *virt_render_surface(void)
{
    return virt_render_ready() ? &g_surface : NULL;
}

/*===========================================================================*/
/*                         PIXEL PRIMITIVES                                  */
/*===========================================================================*/

bool virt_clip_visible(s32 x, s32 y)
{
    return g_surface_ready &&
           x >= 0 && y >= 0 &&
           (u32)x < g_surface.width && (u32)y < g_surface.height;
}

void virt_pixel(s32 x, s32 y, u32 color)
{
    if (!virt_clip_visible(x, y)) {
        return;
    }

    /* pitch is in bytes; the framebuffer is 32bpp */
    u32 *row = (u32 *)((u8 *)g_surface.fb + (u32)y * g_surface.pitch);
    row[x] = color;
}

void virt_blend_pixel(s32 x, s32 y, u32 color, u8 alpha)
{
    if (!virt_clip_visible(x, y)) {
        return;
    }

    if (alpha >= 255) {
        virt_pixel(x, y, color);
        return;
    }
    if (alpha == 0) {
        return;
    }

    u32 *row = (u32 *)((u8 *)g_surface.fb + (u32)y * g_surface.pitch);
    u32 dst = row[x];

    /* Integer 8-bit alpha blend, per channel */
    u32 sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    u32 dr = (dst >> 16) & 0xFF,   dg = (dst >> 8) & 0xFF,   db = dst & 0xFF;

    u32 r = (sr * alpha + dr * (255 - alpha)) / 255;
    u32 g = (sg * alpha + dg * (255 - alpha)) / 255;
    u32 b = (sb * alpha + db * (255 - alpha)) / 255;

    row[x] = (r << 16) | (g << 8) | b;
}

void virt_fill_rect(s32 x, s32 y, u32 width, u32 height, u32 color)
{
    if (!g_surface_ready || width == 0 || height == 0) {
        return;
    }

    s32 x0 = x < 0 ? 0 : x;
    s32 y0 = y < 0 ? 0 : y;
    s32 x1 = (s32)(x + (s32)width);
    s32 y1 = (s32)(y + (s32)height);

    if (x1 > (s32)g_surface.width)  x1 = (s32)g_surface.width;
    if (y1 > (s32)g_surface.height) y1 = (s32)g_surface.height;
    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    for (s32 py = y0; py < y1; py++) {
        u32 *row = (u32 *)((u8 *)g_surface.fb + (u32)py * g_surface.pitch);
        for (s32 px = x0; px < x1; px++) {
            row[px] = color;
        }
    }
}

void virt_gradient_rect(s32 x, s32 y, u32 width, u32 height, u32 top, u32 bottom)
{
    if (!g_surface_ready || height == 0) {
        return;
    }

    /* Per-row interpolation between the two colours */
    for (u32 i = 0; i < height; i++) {
        u32 t = (height > 1) ? (i * 255) / (height - 1) : 0;

        u32 tr = (top >> 16) & 0xFF, tg = (top >> 8) & 0xFF, tb = top & 0xFF;
        u32 br = (bottom >> 16) & 0xFF, bg = (bottom >> 8) & 0xFF, bb = bottom & 0xFF;

        u32 r = (tr * (255 - t) + br * t) / 255;
        u32 g = (tg * (255 - t) + bg * t) / 255;
        u32 b = (tb * (255 - t) + bb * t) / 255;

        virt_fill_rect(x, (s32)(y + i), width, 1, (r << 16) | (g << 8) | b);
    }
}

void virt_rect_outline(s32 x, s32 y, u32 width, u32 height, u32 thickness, u32 color)
{
    if (thickness == 0) {
        return;
    }

    virt_fill_rect(x, y, width, thickness, color);                     /* top */
    virt_fill_rect(x, (s32)(y + height - thickness), width, thickness, color); /* bottom */
    virt_fill_rect(x, y, thickness, height, color);                    /* left */
    virt_fill_rect((s32)(x + width - thickness), y, thickness, height, color); /* right */
}

/*
 * Rounded rectangle.
 *
 * Only the four corner "caps" need per-pixel work; the middle of the
 * shape is three plain rectangles. Testing against a squared radius
 * avoids a square root per pixel.
 */
void virt_rounded_rect(s32 x, s32 y, u32 width, u32 height, u32 radius, u32 color)
{
    if (!g_surface_ready || width == 0 || height == 0) {
        return;
    }

    /* Radius cannot exceed half of either dimension */
    u32 max_r = (width < height ? width : height) / 2;
    if (radius > max_r) {
        radius = max_r;
    }

    if (radius == 0) {
        virt_fill_rect(x, y, width, height, color);
        return;
    }

    /* Middle band */
    virt_fill_rect(x, (s32)(y + radius), width, height - 2 * radius, color);
    /* Top and bottom bands, shortened by the corner radius */
    virt_fill_rect((s32)(x + radius), y, width - 2 * radius, radius, color);
    virt_fill_rect((s32)(x + radius), (s32)(y + height - radius),
                   width - 2 * radius, radius, color);

    /* Corner caps */
    s32 r2 = (s32)(radius * radius);
    s32 cx_left  = (s32)(x + radius);
    s32 cx_right = (s32)(x + width - radius - 1);
    s32 cy_top   = (s32)(y + radius);
    s32 cy_bot   = (s32)(y + height - radius - 1);

    for (u32 dy = 0; dy < radius; dy++) {
        for (u32 dx = 0; dx < radius; dx++) {
            s32 ox = (s32)(radius - dx);
            s32 oy = (s32)(radius - dy);

            /* Both offsets are non-negative, so squares never overflow */
            s32 d2 = ox * ox + oy * oy;
            if (d2 > r2) {
                continue;   /* outside the corner arc */
            }

            virt_pixel(cx_left + (s32)dx,  cy_top + (s32)dy, color);  /* top-left */
            virt_pixel(cx_right - (s32)dx, cy_top + (s32)dy, color);  /* top-right */
            virt_pixel(cx_left + (s32)dx,  cy_bot - (s32)dy, color);  /* bottom-left */
            virt_pixel(cx_right - (s32)dx, cy_bot - (s32)dy, color);  /* bottom-right */
        }
    }
}

/*===========================================================================*/
/*                         LINE / SHAPE PRIMITIVES                           */
/*===========================================================================*/

/* Bresenham, with clipping per pixel */
void virt_line(s32 x0, s32 y0, s32 x1, s32 y1, u32 color)
{
    if (!g_surface_ready) {
        return;
    }

    s32 dx = x1 > x0 ? x1 - x0 : x0 - x1;
    s32 dy = y1 > y0 ? y1 - y0 : y0 - y1;
    s32 sx = x0 < x1 ? 1 : -1;
    s32 sy = y0 < y1 ? 1 : -1;
    s32 err = dx - dy;

    for (;;) {
        virt_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        s32 e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void virt_circle(s32 cx, s32 cy, u32 radius, u32 color, bool filled)
{
    if (!g_surface_ready) {
        return;
    }

    s32 r = (s32)radius;
    s32 x = r;
    s32 y = 0;
    s32 err = 1 - r;

    while (x >= y) {
        if (filled) {
            virt_fill_rect(cx - x, cy + y, (u32)(2 * x + 1), 1, color);
            virt_fill_rect(cx - x, cy - y, (u32)(2 * x + 1), 1, color);
            virt_fill_rect(cx - y, cy + x, (u32)(2 * y + 1), 1, color);
            virt_fill_rect(cx - y, cy - x, (u32)(2 * y + 1), 1, color);
        } else {
            virt_pixel(cx + x, cy + y, color);
            virt_pixel(cx + y, cy + x, color);
            virt_pixel(cx - y, cy + x, color);
            virt_pixel(cx - x, cy + y, color);
            virt_pixel(cx - x, cy - y, color);
            virt_pixel(cx - y, cy - x, color);
            virt_pixel(cx + y, cy - x, color);
            virt_pixel(cx + x, cy - y, color);
        }

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/*===========================================================================*/
/*                         TEXT                                              */
/*===========================================================================*/

void virt_text(s32 x, s32 y, const char *text, u32 color, u32 scale, u8 opacity)
{
    if (!virt_render_ready() || !text) {
        return;
    }

    font8x16_draw_string(g_surface.fb, g_surface.width, g_surface.height,
                         g_surface.pitch, x, y, text,
                         color, scale ? scale : 1, opacity);
}

u32 virt_text_width(const char *text, u32 scale)
{
    return font8x16_string_width(text, scale ? scale : 1);
}

void virt_text_centered(s32 cx, s32 y, const char *text, u32 color, u32 scale, u8 opacity)
{
    if (!virt_render_ready() || !text) {
        return;
    }

    font8x16_draw_string_centered(g_surface.fb, g_surface.width, g_surface.height,
                                  g_surface.pitch, cx, y, text,
                                  color, scale ? scale : 1, opacity);
}
