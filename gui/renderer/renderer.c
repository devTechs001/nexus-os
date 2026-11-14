/*
 * NEXUS OS - GUI Framework
 * gui/renderer/renderer.c
 *
 * Graphics Renderer
 *
 * This module implements the graphics renderer which provides
 * 2D drawing primitives, text rendering, and image compositing.
 */

#include "../gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================*/
/*                         RENDERER STATE                                    */
/*===========================================================================*/

static struct {
    bool initialized;
    
    /* Renderer type */
    u32 renderer_type;          /* 0=Software, 1=OpenGL, 2=Vulkan, 3=DirectX */
    bool hardware_accel;
    
    /* Default settings */
    color_t default_foreground;
    color_t default_background;
    u32 default_line_width;
    u32 default_font_size;
    
    /* Font cache */
    struct renderer_font {
        u32 id;
        char name[64];
        u32 size;
        void *data;
    } fonts[64];
    u32 num_fonts;
    u32 next_font_id;
    
    /* Image cache */
    struct renderer_image {
        u32 id;
        char path[256];
        void *surface;
        u32 width;
        u32 height;
    } images[128];
    u32 num_images;
    u32 next_image_id;
    
    /* Statistics */
    u64 draw_calls;
    u64 pixels_drawn;
    u64 text_rendered;
    
    /* Synchronization */
    spinlock_t lock;
} g_renderer = {
    .initialized = false,
    .renderer_type = 0,  /* Software */
    .hardware_accel = false,
    .default_foreground = {{{0, 0, 0, 255}}},
    .default_background = {{{255, 255, 255, 255}}},
    .default_line_width = 1,
    .default_font_size = 12,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static void renderer_draw_pixel_sw(void *surface, u32 width, u32 height,
                                    s32 x, s32 y, color_t color);
static void renderer_draw_line_sw(void *surface, u32 width, u32 height,
                                   s32 x1, s32 y1, s32 x2, s32 y2, color_t color);
static void renderer_draw_rect_sw(void *surface, u32 width, u32 height,
                                   rect_t *rect, color_t color);
static void renderer_fill_rect_sw(void *surface, u32 width, u32 height,
                                   rect_t *rect, color_t color);
static void renderer_blend_pixel(u32 *dst, color_t color);

/*===========================================================================*/
/*                         RENDERER INITIALIZATION                           */
/*===========================================================================*/

/**
 * renderer_init - Initialize the graphics renderer
 * @type: Renderer type (0=Software, 1=OpenGL, 2=Vulkan)
 *
 * Returns: 0 on success, error code on failure
 */
int renderer_init(u32 type)
{
    if (g_renderer.initialized) {
        pr_debug("Renderer already initialized\n");
        return 0;
    }

    spin_lock(&g_renderer.lock);

    pr_info("Initializing graphics renderer (type: %u)...\n", type);

    g_renderer.renderer_type = type;
    g_renderer.hardware_accel = (type > 0);
    g_renderer.num_fonts = 0;
    g_renderer.next_font_id = 1;
    g_renderer.num_images = 0;
    g_renderer.next_image_id = 1;
    g_renderer.draw_calls = 0;
    g_renderer.pixels_drawn = 0;
    g_renderer.text_rendered = 0;

    /* Initialize hardware renderer if requested */
    if (g_renderer.hardware_accel) {
        pr_info("Hardware acceleration enabled\n");
        /* In a real implementation, this would initialize OpenGL/Vulkan */
    } else {
        pr_info("Using software renderer\n");
    }

    g_renderer.initialized = true;

    pr_info("Renderer initialized\n");

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * renderer_shutdown - Shutdown the renderer
 */
void renderer_shutdown(void)
{
    if (!g_renderer.initialized) {
        return;
    }

    spin_lock(&g_renderer.lock);

    pr_info("Shutting down renderer...\n");

    /* Free fonts */
    for (u32 i = 0; i < g_renderer.num_fonts; i++) {
        if (g_renderer.fonts[i].data) {
            kfree(g_renderer.fonts[i].data);
        }
    }

    /* Free images */
    for (u32 i = 0; i < g_renderer.num_images; i++) {
        if (g_renderer.images[i].surface) {
            kfree(g_renderer.images[i].surface);
        }
    }

    g_renderer.initialized = false;

    pr_info("Renderer shutdown complete\n");

    spin_unlock(&g_renderer.lock);
}

/*===========================================================================*/
/*                         GRAPHICS CONTEXT                                  */
/*===========================================================================*/

/**
 * graphics_create_context - Create a graphics context
 * @surface: Drawing surface
 *
 * Returns: Pointer to created context, or NULL on failure
 */
graphics_context_t *graphics_create_context(void *surface)
{
    graphics_context_t *ctx;

    if (!g_renderer.initialized) {
        return NULL;
    }

    ctx = (graphics_context_t *)kzalloc(sizeof(graphics_context_t));
    if (!ctx) {
        return NULL;
    }

    ctx->surface = surface;
    /*
     * Surface geometry must be set here: graphics_fill_rect() and friends draw
     * using ctx->width/ctx->height, not the clip rectangle. Leaving them at
     * zero meant every fill was clipped away to nothing, so a widget that
     * painted correctly produced no pixels at all.
     *
     * Default to the primary display size; callers that render into a
     * sub-surface (a window backing store, a single scanline) override these
     * along with clip_rect.
     */
    {
        u32 w = 1024;
        u32 h = 768;

        boot_params_t *bp = get_boot_params();
        if (bp && bp->gfx_width && bp->gfx_height) {
            w = bp->gfx_width;
            h = bp->gfx_height;
        }

        ctx->width = w;
        ctx->height = h;
        ctx->clip_rect = (rect_t){0, 0, (s32)w, (s32)h};
    }
    ctx->foreground = g_renderer.default_foreground;
    ctx->background = g_renderer.default_background;
    ctx->line_width = g_renderer.default_line_width;
    ctx->font_id = 0;
    ctx->font_size = g_renderer.default_font_size;
    ctx->antialias = 1;
    ctx->alpha = 255;

    /* Initialize transform matrix to identity */
    memset(&ctx->transform, 0, sizeof(ctx->transform));
    for (int i = 0; i < 4; i++) {
        ctx->transform.m[i * 5] = 1.0f;
    }

    return ctx;
}

/**
 * graphics_destroy_context - Destroy a graphics context
 * @ctx: Context to destroy
 */
void graphics_destroy_context(graphics_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    kfree(ctx);
}

/*===========================================================================*/
/*                         DRAWING PRIMITIVES                                */
/*===========================================================================*/

/**
 * graphics_clear - Clear a surface with a color
 * @ctx: Graphics context
 * @color: Clear color
 *
 * Returns: 0 on success, error code on failure
 */
int graphics_clear(graphics_context_t *ctx, color_t color)
{
    if (!ctx || !ctx->surface) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    /* Get surface dimensions (assumed from context or default) */
    u32 width = 800;
    u32 height = 600;

    if (g_renderer.hardware_accel) {
        /* Hardware clear */
        /* In real implementation, would use GPU clear */
    }

    /* Software clear */
    renderer_fill_rect_sw(ctx->surface, width, height,
                          &(rect_t){0, 0, width, height}, color);

    g_renderer.draw_calls++;
    g_renderer.pixels_drawn += width * height;

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * graphics_draw_pixel - Draw a single pixel
 * @ctx: Graphics context
 * @x: X coordinate
 * @y: Y coordinate
 * @color: Pixel color
 *
 * Returns: 0 on success, error code on failure
 */
int graphics_draw_pixel(graphics_context_t *ctx, s32 x, s32 y, color_t color)
{
    if (!ctx || !ctx->surface) {
        return -1;
    }

    /* Clip check */
    if (x < ctx->clip_rect.x || x >= ctx->clip_rect.x + ctx->clip_rect.width ||
        y < ctx->clip_rect.y || y >= ctx->clip_rect.y + ctx->clip_rect.height) {
        return 0;
    }

    spin_lock(&g_renderer.lock);

    renderer_draw_pixel_sw(ctx->surface, ctx->clip_rect.width, ctx->clip_rect.height,
                           x - ctx->clip_rect.x, y - ctx->clip_rect.y, color);

    g_renderer.draw_calls++;
    g_renderer.pixels_drawn++;

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * graphics_draw_line - Draw a line
 * @ctx: Graphics context
 * @x1: Start X
 * @y1: Start Y
 * @x2: End X
 * @y2: End Y
 *
 * Returns: 0 on success, error code on failure
 */
int graphics_draw_line(graphics_context_t *ctx, s32 x1, s32 y1, s32 x2, s32 y2)
{
    if (!ctx || !ctx->surface) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    color_t color = ctx->foreground;
    
    /* Apply alpha */
    color.a = (color.a * ctx->alpha) / 255;

    renderer_draw_line_sw(ctx->surface, ctx->clip_rect.width, ctx->clip_rect.height,
                          x1, y1, x2, y2, color);

    g_renderer.draw_calls++;

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * color_from_rgba - Create color from RGBA values
 */
color_t color_from_rgba(u8 r, u8 g, u8 b, u8 a)
{
    color_t color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    return color;
}

/**
 * graphics_draw_rect - Draw a rectangle outline
 * @ctx: Graphics context
 * @rect: Rectangle
 *
 * Returns: 0 on success, error code on failure
 */
int graphics_draw_rect(graphics_context_t *ctx, rect_t *rect)
{
    if (!ctx || !ctx->surface || !rect) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    color_t color = ctx->foreground;
    color.a = (color.a * ctx->alpha) / 255;

    /* Draw four lines */
    renderer_draw_line_sw(ctx->surface, ctx->width, ctx->height,
                          rect->x, rect->y, rect->x + rect->width, rect->y, color);
    renderer_draw_line_sw(ctx->surface, ctx->width, ctx->height,
                          rect->x + rect->width, rect->y, 
                          rect->x + rect->width, rect->y + rect->height, color);
    renderer_draw_line_sw(ctx->surface, ctx->width, ctx->height,
                          rect->x + rect->width, rect->y + rect->height,
                          rect->x, rect->y + rect->height, color);
    renderer_draw_line_sw(ctx->surface, ctx->width, ctx->height,
                          rect->x, rect->y + rect->height, rect->x, rect->y, color);

    g_renderer.draw_calls += 4;

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * graphics_fill_rect - Fill a rectangle
 * @ctx: Graphics context
 * @rect: Rectangle
 * @color: Fill color
 *
 * Returns: 0 on success, error code on failure
 */
/*
 * Fill a rectangle honouring the context's clip rectangle.
 *
 * This is the scanline-safe path: a context may describe a surface only one
 * row tall (the window manager paints a screen row at a time), in which case
 * the rectangle must be clipped to that row instead of being indexed with the
 * full screen height and walking off the end of the buffer.
 */
int graphics_fill_rect_clipped(graphics_context_t *ctx, rect_t *rect, color_t color)
{
    if (!ctx || !ctx->surface || !rect) {
        return -1;
    }

    rect_t clip = ctx->clip_rect;

    s32 x0 = rect->x;
    s32 y0 = rect->y;
    s32 x1 = rect->x + rect->width;
    s32 y1 = rect->y + rect->height;

    if (x0 < clip.x) x0 = clip.x;
    if (y0 < clip.y) y0 = clip.y;
    if (x1 > clip.x + clip.width)  x1 = clip.x + clip.width;
    if (y1 > clip.y + clip.height) y1 = clip.y + clip.height;

    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }

    spin_lock(&g_renderer.lock);

    /* Subtract the clip origin so the fill lands on the right pixels of a
     * sub-surface. */
    rect_t clipped = {x0 - clip.x, y0 - clip.y, x1 - x0, y1 - y0};
    color.a = (color.a * ctx->alpha) / 255;
    renderer_fill_rect_sw(ctx->surface, ctx->width, ctx->height, &clipped, color);

    g_renderer.draw_calls++;
    g_renderer.pixels_drawn += (u32)(clipped.width * clipped.height);

    spin_unlock(&g_renderer.lock);

    return 0;
}

int graphics_fill_rect(graphics_context_t *ctx, rect_t *rect, color_t color)
{
    if (!ctx || !ctx->surface || !rect) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    /* Apply alpha */
    color.a = (color.a * ctx->alpha) / 255;

    renderer_fill_rect_sw(ctx->surface, ctx->width, ctx->height,
                          rect, color);

    g_renderer.draw_calls++;
    g_renderer.pixels_drawn += rect->width * rect->height;

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * graphics_stroke_rect - Draw a rectangle outline
 * @ctx:  Graphics context
 * @rect: Rectangle to outline
 * @color: Outline colour
 *
 * The renderer had no way to stroke a rectangle: renderer_draw_rect_sw() was
 * a complete implementation with no caller, so every caller that wanted a
 * border had to fill four separate rectangles itself.
 */
int graphics_stroke_rect(graphics_context_t *ctx, rect_t *rect, color_t color)
{
    if (!ctx || !ctx->surface || !rect) {
        return -1;
    }

    if (rect->width <= 0 || rect->height <= 0) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    /* Apply the context alpha, exactly as graphics_fill_rect() does. */
    color.a = (u8)((color.a * ctx->alpha) / 255);

    renderer_draw_rect_sw(ctx->surface, ctx->width, ctx->height, rect, color);

    g_renderer.draw_calls++;
    /* An outline touches the perimeter, not the area. */
    g_renderer.pixels_drawn += 2 * (u32)(rect->width + rect->height);

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * graphics_draw_text - Draw text
 * @ctx: Graphics context
 * @x: X position
 * @y: Y position
 * @text: Text string
 *
 * Returns: 0 on success, error code on failure
 */
int graphics_draw_text(graphics_context_t *ctx, s32 x, s32 y, const char *text)
{
    if (!ctx || !ctx->surface || !text) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    /* Simplified text rendering - draw each character as a box */
    /* In a real implementation, this would use a font rasterizer */
    
    color_t color = ctx->foreground;
    color.a = (color.a * ctx->alpha) / 255;

    s32 char_width = ctx->font_size;
    s32 char_height = ctx->font_size;

    /*
     * Draw the run of glyph cells clipped against both the context geometry
     * and its clip rectangle. Using clip_rect.width as the surface width while
     * indexing a surface only one row tall is what allowed text to walk off
     * the end of a scanline buffer.
     */
    while (*text) {
        if (*text != ' ') {
            rect_t char_rect = {x, y, char_width - 2, char_height};

            s32 cx0 = char_rect.x;
            s32 cy0 = char_rect.y;
            s32 cx1 = char_rect.x + char_rect.width;
            s32 cy1 = char_rect.y + char_rect.height;

            if (cx0 < 0) cx0 = 0;
            if (cy0 < 0) cy0 = 0;
            if (cx1 > (s32)ctx->clip_rect.width)  cx1 = (s32)ctx->clip_rect.width;
            if (cy1 > (s32)ctx->clip_rect.height) cy1 = (s32)ctx->clip_rect.height;

            if (cx1 > cx0 && cy1 > cy0) {
                rect_t clipped = {cx0, cy0, cx1 - cx0, cy1 - cy0};
                renderer_fill_rect_sw(ctx->surface, ctx->width,
                                       ctx->height, &clipped, color);
            }
        }
        x += char_width;
        text++;
    }

    g_renderer.draw_calls++;
    g_renderer.text_rendered++;

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * graphics_blit - Blit a surface
 * @ctx: Graphics context
 * @src: Source surface
 * @src_rect: Source rectangle
 * @dst_x: Destination X
 * @dst_y: Destination Y
 *
 * Returns: 0 on success, error code on failure
 */
int graphics_blit(graphics_context_t *ctx, void *src, rect_t *src_rect, 
                  s32 dst_x, s32 dst_y)
{
    if (!ctx || !ctx->surface || !src || !src_rect) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    u32 *src_pixels = (u32 *)src;
    u32 *dst_pixels = (u32 *)ctx->surface;

    /* Simple blit with alpha blending */
    for (s32 sy = 0; sy < src_rect->height; sy++) {
        for (s32 sx = 0; sx < src_rect->width; sx++) {
            s32 dx = dst_x + sx;
            s32 dy = dst_y + sy;

            if (dx < ctx->clip_rect.x || dx >= ctx->clip_rect.x + ctx->clip_rect.width ||
                dy < ctx->clip_rect.y || dy >= ctx->clip_rect.y + ctx->clip_rect.height) {
                continue;
            }

            u32 src_pixel = src_pixels[sy * src_rect->width + sx];
            u8 src_a = (src_pixel >> 24) & 0xFF;

            if (src_a > 0) {
                u32 *dst = &dst_pixels[dy * ctx->clip_rect.width + dx];
                renderer_blend_pixel(dst, (color_t){{{
                    (src_pixel >> 16) & 0xFF,
                    (src_pixel >> 8) & 0xFF,
                    src_pixel & 0xFF,
                    src_a
                }}});
            }
        }
    }

    g_renderer.draw_calls++;
    g_renderer.pixels_drawn += src_rect->width * src_rect->height;

    spin_unlock(&g_renderer.lock);

    return 0;
}

/**
 * graphics_present - Present a surface to display
 * @surface: Surface to present
 * @dirty_rects: Dirty rectangles
 * @num_rects: Number of dirty rectangles
 *
 * Returns: 0 on success, error code on failure
 */
int graphics_present(void *surface, rect_t *dirty_rects, u32 num_rects)
{
    if (!surface) {
        return -1;
    }

    /* In a real implementation, this would:
     * - Wait for VSync
     * - Copy to front buffer
     * - Issue flip command
     */
    
    (void)dirty_rects;
    (void)num_rects;

    return 0;
}

/*===========================================================================*/
/*                         SOFTWARE RENDERER                                 */
/*===========================================================================*/

/**
 * renderer_draw_pixel_sw - Software pixel drawing
 */
/*
 * Framebuffer depth.
 *
 * Every primitive below used to write through a u32* unconditionally, so the
 * renderer only ever worked on a 32bpp surface. The GUI loop then refused to
 * render at all unless gfx_depth was 0 or 32 - and set gfxmode=1024x768 in
 * grub.cfg lets GRUB pick its own default depth, commonly 24bpp. The result was
 * a completely blank screen with a working GUI underneath it.
 *
 * These helpers write the right pixel format for the active depth. Set once by
 * graphics_set_target_depth() at the start of each composite.
 */
static u32 g_target_bpp = 32;

void graphics_set_target_depth(u32 bpp)
{
    if (bpp == 0) {
        bpp = 32;
    }
    if (bpp == 16 || bpp == 24 || bpp == 32) {
        g_target_bpp = bpp;
    }
}

static inline void renderer_store(void *surface, u32 stride_px,
                                  s32 x, s32 y, u32 argb)
{
    switch (g_target_bpp) {
    case 16: {
        u8 *row = (u8 *)surface + (u32)y * stride_px * 2;
        u16 *p = (u16 *)row;
        p[x] = (u16)(((argb >> 19) & 0x1F) << 11 |
                     ((argb >> 10) & 0x3F) << 5  |
                     ((argb >> 3)  & 0x1F));
        break;
    }
    case 24: {
        u8 *row = (u8 *)surface + (u32)y * stride_px * 3;
        u8 *p = row + (u32)x * 3;
        p[0] = (u8)(argb);
        p[1] = (u8)(argb >> 8);
        p[2] = (u8)(argb >> 16);
        break;
    }
    default: {
        u32 *row = (u32 *)surface + (u32)y * stride_px;
        row[x] = argb;
        break;
    }
    }
}

static inline u32 renderer_load(void *surface, u32 stride_px, s32 x, s32 y)
{
    switch (g_target_bpp) {
    case 16: {
        u16 *row = (u16 *)((u8 *)surface + (u32)y * stride_px * 2);
        u16 v = row[x];
        u32 r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
        return ((r << 3) | (r >> 2)) << 16 |
               ((g << 2) | (g >> 4)) << 8  |
               ((b << 3) | (b >> 2));
    }
    case 24: {
        u8 *p = (u8 *)surface + (u32)y * stride_px * 3 + (u32)x * 3;
        return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16);
    }
    default:
        return ((u32 *)surface)[(u32)y * stride_px + (u32)x];
    }
}

/* Blend a colour over a depth-appropriate destination pixel. */
static void renderer_blend_at(void *surface, u32 stride_px, s32 x, s32 y,
                              color_t color)
{
    if (g_target_bpp == 32) {
        u32 *pixels = (u32 *)surface;
        renderer_blend_pixel(&pixels[(u32)y * stride_px + (u32)x], color);
        return;
    }

    u32 argb;
    if (color.a == 255) {
        renderer_store(surface, stride_px, x, y,
                       (0xFFu << 24) | (color.r << 16) | (color.g << 8) | color.b);
        return;
    }
    if (color.a == 0) {
        return;
    }

    u32 dst = renderer_load(surface, stride_px, x, y);
    u8 dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    u8 a = color.a;

    u8 r = (u8)((color.r * a + dr * (255 - a)) / 255);
    u8 g = (u8)((color.g * a + dg * (255 - a)) / 255);
    u8 b = (u8)((color.b * a + db * (255 - a)) / 255);

    argb = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    renderer_store(surface, stride_px, x, y, argb);
}

static void renderer_draw_pixel_sw(void *surface, u32 width, u32 height,
                                    s32 x, s32 y, color_t color)
{
    if (x < 0 || x >= (s32)width || y < 0 || y >= (s32)height) {
        return;
    }

    renderer_blend_at(surface, width, x, y, color);
}

/**
 * renderer_draw_line_sw - Software line drawing (Bresenham's algorithm)
 */
static void renderer_draw_line_sw(void *surface, u32 width, u32 height,
                                   s32 x1, s32 y1, s32 x2, s32 y2, color_t color)
{
    s32 dx = abs(x2 - x1);
    s32 dy = abs(y2 - y1);
    s32 sx = (x1 < x2) ? 1 : -1;
    s32 sy = (y1 < y2) ? 1 : -1;
    s32 err = dx - dy;

    while (1) {
        renderer_draw_pixel_sw(surface, width, height, x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        s32 e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * renderer_draw_rect_sw - Software rectangle outline drawing
 */
static void renderer_draw_rect_sw(void *surface, u32 width, u32 height,
                                   rect_t *rect, color_t color)
{
    renderer_draw_line_sw(surface, width, height,
                          rect->x, rect->y, rect->x + rect->width, rect->y, color);
    renderer_draw_line_sw(surface, width, height,
                          rect->x + rect->width, rect->y,
                          rect->x + rect->width, rect->y + rect->height, color);
    renderer_draw_line_sw(surface, width, height,
                          rect->x + rect->width, rect->y + rect->height,
                          rect->x, rect->y + rect->height, color);
    renderer_draw_line_sw(surface, width, height,
                          rect->x, rect->y + rect->height, rect->x, rect->y, color);
}

/**
 * renderer_fill_rect_sw - Software rectangle filling
 */
static void renderer_fill_rect_sw(void *surface, u32 width, u32 height,
                                   rect_t *rect, color_t color)
{
    for (s32 y = rect->y; y < rect->y + rect->height; y++) {
        for (s32 x = rect->x; x < rect->x + rect->width; x++) {
            if (x >= 0 && x < (s32)width && y >= 0 && y < (s32)height) {
                renderer_blend_at(surface, width, x, y, color);
            }
        }
    }
}

/**
 * renderer_blend_pixel - Blend a pixel with destination
 */
static void renderer_blend_pixel(u32 *dst, color_t color)
{
    if (color.a == 255) {
        *dst = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
    } else if (color.a > 0) {
        u32 src = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;
        
        u8 src_a = (src >> 24) & 0xFF;
        u8 src_r = (src >> 16) & 0xFF;
        u8 src_g = (src >> 8) & 0xFF;
        u8 src_b = src & 0xFF;

        u8 dst_a = (*dst >> 24) & 0xFF;
        u8 dst_r = (*dst >> 16) & 0xFF;
        u8 dst_g = (*dst >> 8) & 0xFF;
        u8 dst_b = *dst & 0xFF;

        u8 out_a = src_a + dst_a * (255 - src_a) / 255;
        if (out_a == 0) {
            *dst = 0;
            return;
        }

        u8 out_r = (src_r * src_a + dst_r * dst_a * (255 - src_a) / 255) / out_a;
        u8 out_g = (src_g * src_a + dst_g * dst_a * (255 - src_a) / 255) / out_a;
        u8 out_b = (src_b * src_a + dst_b * dst_a * (255 - src_a) / 255) / out_a;

        *dst = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
    }
}

/*===========================================================================*/
/*                         FONT MANAGEMENT                                   */
/*===========================================================================*/

/**
 * renderer_load_font - Load a font
 * @name: Font name
 * @size: Font size
 *
 * Returns: Font ID, or 0 on failure
 */
u32 renderer_load_font(const char *name, u32 size)
{
    if (!name || g_renderer.num_fonts >= 64) {
        return 0;
    }

    spin_lock(&g_renderer.lock);

    u32 font_id = g_renderer.next_font_id++;
    struct renderer_font *font = &g_renderer.fonts[g_renderer.num_fonts];

    font->id = font_id;
    strncpy(font->name, name, sizeof(font->name) - 1);
    font->size = size;
    font->data = NULL;  /* Would load font data in real implementation */

    g_renderer.num_fonts++;

    pr_debug("Font loaded: %s %u (ID: %u)\n", name, size, font_id);

    spin_unlock(&g_renderer.lock);

    return font_id;
}

/*===========================================================================*/
/*                         IMAGE MANAGEMENT                                  */
/*===========================================================================*/

/**
 * renderer_load_image - Load an image
 * @path: Image path
 *
 * Returns: Image ID, or 0 on failure
 */
u32 renderer_load_image(const char *path)
{
    if (!path || g_renderer.num_images >= 128) {
        return 0;
    }

    spin_lock(&g_renderer.lock);

    u32 image_id = g_renderer.next_image_id++;
    struct renderer_image *img = &g_renderer.images[g_renderer.num_images];

    img->id = image_id;
    strncpy(img->path, path, sizeof(img->path) - 1);
    img->width = 100;  /* Simulated */
    img->height = 100;
    img->surface = NULL;  /* Would load image in real implementation */

    g_renderer.num_images++;

    pr_debug("Image loaded: %s (ID: %u)\n", path, image_id);

    spin_unlock(&g_renderer.lock);

    return image_id;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * renderer_get_stats - Get renderer statistics
 * @draw_calls: Output draw call count
 * @pixels_drawn: Output pixel count
 * @text_rendered: Output text render count
 */
void renderer_get_stats(u64 *draw_calls, u64 *pixels_drawn, u64 *text_rendered)
{
    spin_lock(&g_renderer.lock);

    if (draw_calls) *draw_calls = g_renderer.draw_calls;
    if (pixels_drawn) *pixels_drawn = g_renderer.pixels_drawn;
    if (text_rendered) *text_rendered = g_renderer.text_rendered;

    spin_unlock(&g_renderer.lock);
}

/**
 * renderer_reset_stats - Reset renderer statistics
 */
void renderer_reset_stats(void)
{
    spin_lock(&g_renderer.lock);

    g_renderer.draw_calls = 0;
    g_renderer.pixels_drawn = 0;
    g_renderer.text_rendered = 0;

    spin_unlock(&g_renderer.lock);
}
