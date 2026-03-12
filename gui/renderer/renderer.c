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
    struct {
        u32 id;
        char name[64];
        u32 size;
        void *data;
    } fonts[64];
    u32 num_fonts;
    u32 next_font_id;
    
    /* Image cache */
    struct {
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
    .default_foreground = {0, 0, 0, 255},
    .default_background = {255, 255, 255, 255},
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
    ctx->clip_rect = (rect_t){0, 0, 4096, 4096};  /* Large default clip */
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
    renderer_draw_line_sw(ctx->surface, ctx->clip_rect.width, ctx->clip_rect.height,
                          rect->x, rect->y, rect->x + rect->width, rect->y, color);
    renderer_draw_line_sw(ctx->surface, ctx->clip_rect.width, ctx->clip_rect.height,
                          rect->x + rect->width, rect->y, 
                          rect->x + rect->width, rect->y + rect->height, color);
    renderer_draw_line_sw(ctx->surface, ctx->clip_rect.width, ctx->clip_rect.height,
                          rect->x + rect->width, rect->y + rect->height,
                          rect->x, rect->y + rect->height, color);
    renderer_draw_line_sw(ctx->surface, ctx->clip_rect.width, ctx->clip_rect.height,
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
int graphics_fill_rect(graphics_context_t *ctx, rect_t *rect, color_t color)
{
    if (!ctx || !ctx->surface || !rect) {
        return -1;
    }

    spin_lock(&g_renderer.lock);

    /* Apply alpha */
    color.a = (color.a * ctx->alpha) / 255;

    renderer_fill_rect_sw(ctx->surface, ctx->clip_rect.width, ctx->clip_rect.height,
                          rect, color);

    g_renderer.draw_calls++;
    g_renderer.pixels_drawn += rect->width * rect->height;

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

    while (*text) {
        if (*text != ' ') {
            rect_t char_rect = {x, y, char_width - 2, char_height};
            renderer_fill_rect_sw(ctx->surface, ctx->clip_rect.width, 
                                   ctx->clip_rect.height, &char_rect, color);
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
                renderer_blend_pixel(dst, (color_t){
                    (src_pixel >> 16) & 0xFF,
                    (src_pixel >> 8) & 0xFF,
                    src_pixel & 0xFF,
                    src_a
                });
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
static void renderer_draw_pixel_sw(void *surface, u32 width, u32 height,
                                    s32 x, s32 y, color_t color)
{
    if (x < 0 || x >= (s32)width || y < 0 || y >= (s32)height) {
        return;
    }

    u32 *pixels = (u32 *)surface;
    renderer_blend_pixel(&pixels[y * width + x], color);
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
    u32 *pixels = (u32 *)surface;

    for (s32 y = rect->y; y < rect->y + rect->height; y++) {
        for (s32 x = rect->x; x < rect->x + rect->width; x++) {
            if (x >= 0 && x < (s32)width && y >= 0 && y < (s32)height) {
                renderer_blend_pixel(&pixels[y * width + x], color);
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
    struct {
        u32 id;
        char name[64];
        u32 size;
        void *data;
    } *font = &g_renderer.fonts[g_renderer.num_fonts];

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
    struct {
        u32 id;
        char path[256];
        void *surface;
        u32 width;
        u32 height;
    } *img = &g_renderer.images[g_renderer.num_images];

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
