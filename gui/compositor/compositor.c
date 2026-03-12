/*
 * NEXUS OS - GUI Framework
 * gui/compositor/compositor.c
 *
 * Window Compositor
 *
 * This module implements the window compositor which is responsible
 * for combining multiple window surfaces into the final display output,
 * handling transparency, shadows, and visual effects.
 */

#include "../gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================*/
/*                         COMPOSITOR STATE                                  */
/*===========================================================================*/

static struct {
    bool initialized;
    
    /* Composition settings */
    bool enabled;
    bool hardware_accel;
    bool vsync_enabled;
    u32 target_fps;
    
    /* Visual effects */
    bool shadows_enabled;
    bool animations_enabled;
    bool transparency_enabled;
    u32 shadow_blur_radius;
    u32 shadow_offset;
    
    /* Framebuffer */
    void *front_buffer;
    void *back_buffer;
    void *current_buffer;
    u32 buffer_width;
    u32 buffer_height;
    u32 buffer_stride;
    
    /* Damage tracking */
    rect_t damage_rects[MAX_DIRTY_RECTS];
    u32 num_damage_rects;
    bool full_damage;
    
    /* Window list (z-ordered) */
    window_t *window_list[MAX_WINDOWS];
    u32 num_windows;
    
    /* Layers */
    struct {
        u32 id;
        bool visible;
        u32 z_order;
        void *surface;
        rect_t bounds;
    } layers[MAX_LAYERS];
    u32 num_layers;
    
    /* Performance */
    u64 compose_time_us;
    u64 frame_count;
    u32 fps;
    
    /* Callbacks */
    void (*on_frame_complete)(void);
    
    /* Synchronization */
    spinlock_t lock;
} g_compositor = {
    .initialized = false,
    .enabled = true,
    .hardware_accel = true,
    .vsync_enabled = true,
    .target_fps = 60,
    .shadows_enabled = true,
    .animations_enabled = true,
    .transparency_enabled = true,
    .shadow_blur_radius = 8,
    .shadow_offset = 4,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int compositor_allocate_buffers(u32 width, u32 height);
static void compositor_free_buffers(void);
static void compositor_sort_windows(void);
static int compositor_composite_window(window_t *window, void *dst);
static void compositor_apply_shadow(window_t *window, void *dst);
static void compositor_apply_effects(window_t *window, void *dst);
static void compositor_update_damage(void);

/*===========================================================================*/
/*                         COMPOSITOR INITIALIZATION                         */
/*===========================================================================*/

/**
 * compositor_init - Initialize the compositor
 *
 * Returns: 0 on success, error code on failure
 */
int compositor_init(void)
{
    if (g_compositor.initialized) {
        pr_debug("Compositor already initialized\n");
        return 0;
    }

    spin_lock(&g_compositor.lock);

    pr_info("Initializing window compositor...\n");

    /* Get display info */
    display_info_t display;
    if (display_get_primary(&display) == 0) {
        g_compositor.buffer_width = display.width;
        g_compositor.buffer_height = display.height;
    } else {
        g_compositor.buffer_width = 1920;
        g_compositor.buffer_height = 1080;
    }

    /* Allocate framebuffers */
    if (compositor_allocate_buffers(g_compositor.buffer_width, 
                                     g_compositor.buffer_height) != 0) {
        pr_err("Failed to allocate compositor buffers\n");
        spin_unlock(&g_compositor.lock);
        return -1;
    }

    /* Initialize state */
    g_compositor.num_windows = 0;
    g_compositor.num_layers = 0;
    g_compositor.num_damage_rects = 0;
    g_compositor.full_damage = true;
    g_compositor.frame_count = 0;
    g_compositor.fps = 0;
    g_compositor.compose_time_us = 0;

    g_compositor.initialized = true;

    pr_info("Compositor initialized (%ux%u, HW accel: %s)\n",
            g_compositor.buffer_width, g_compositor.buffer_height,
            g_compositor.hardware_accel ? "yes" : "no");

    spin_unlock(&g_compositor.lock);

    return 0;
}

/**
 * compositor_shutdown - Shutdown the compositor
 */
void compositor_shutdown(void)
{
    if (!g_compositor.initialized) {
        return;
    }

    spin_lock(&g_compositor.lock);

    pr_info("Shutting down compositor...\n");

    /* Free buffers */
    compositor_free_buffers();

    g_compositor.initialized = false;

    pr_info("Compositor shutdown complete\n");

    spin_unlock(&g_compositor.lock);
}

/**
 * compositor_allocate_buffers - Allocate compositor buffers
 * @width: Buffer width
 * @height: Buffer height
 *
 * Returns: 0 on success, error code on failure
 */
static int compositor_allocate_buffers(u32 width, u32 height)
{
    size_t buffer_size = width * height * 4;  /* RGBA */

    /* Allocate front buffer */
    g_compositor.front_buffer = kmalloc(buffer_size);
    if (!g_compositor.front_buffer) {
        return -1;
    }
    memset(g_compositor.front_buffer, 0, buffer_size);

    /* Allocate back buffer */
    g_compositor.back_buffer = kmalloc(buffer_size);
    if (!g_compositor.back_buffer) {
        kfree(g_compositor.front_buffer);
        return -1;
    }
    memset(g_compositor.back_buffer, 0, buffer_size);

    g_compositor.current_buffer = g_compositor.back_buffer;
    g_compositor.buffer_width = width;
    g_compositor.buffer_height = height;
    g_compositor.buffer_stride = width * 4;

    pr_debug("Compositor buffers allocated: %ux%u (%zu KB)\n",
             width, height, buffer_size / 1024);

    return 0;
}

/**
 * compositor_free_buffers - Free compositor buffers
 */
static void compositor_free_buffers(void)
{
    if (g_compositor.front_buffer) {
        kfree(g_compositor.front_buffer);
        g_compositor.front_buffer = NULL;
    }
    if (g_compositor.back_buffer) {
        kfree(g_compositor.back_buffer);
        g_compositor.back_buffer = NULL;
    }
    g_compositor.current_buffer = NULL;
}

/*===========================================================================*/
/*                         WINDOW MANAGEMENT                                 */
/*===========================================================================*/

/**
 * compositor_add_window - Add a window to the compositor
 * @window: Window to add
 *
 * Returns: 0 on success, error code on failure
 */
int compositor_add_window(window_t *window)
{
    if (!window) {
        return -1;
    }

    if (!g_compositor.initialized) {
        return -1;
    }

    spin_lock(&g_compositor.lock);

    if (g_compositor.num_windows >= MAX_WINDOWS) {
        pr_err("Maximum window count reached\n");
        spin_unlock(&g_compositor.lock);
        return -1;
    }

    /* Add to window list */
    g_compositor.window_list[g_compositor.num_windows] = window;
    g_compositor.num_windows++;

    /* Mark full damage for redraw */
    g_compositor.full_damage = true;

    pr_debug("Window %u added to compositor\n", window->window_id);

    spin_unlock(&g_compositor.lock);

    return 0;
}

/**
 * compositor_remove_window - Remove a window from the compositor
 * @window: Window to remove
 *
 * Returns: 0 on success, error code on failure
 */
int compositor_remove_window(window_t *window)
{
    if (!window) {
        return -1;
    }

    if (!g_compositor.initialized) {
        return -1;
    }

    spin_lock(&g_compositor.lock);

    /* Find and remove window */
    for (u32 i = 0; i < g_compositor.num_windows; i++) {
        if (g_compositor.window_list[i] == window) {
            /* Shift remaining windows */
            for (u32 j = i; j < g_compositor.num_windows - 1; j++) {
                g_compositor.window_list[j] = g_compositor.window_list[j + 1];
            }
            g_compositor.num_windows--;

            /* Mark full damage for redraw */
            g_compositor.full_damage = true;

            pr_debug("Window %u removed from compositor\n", window->window_id);
            spin_unlock(&g_compositor.lock);
            return 0;
        }
    }

    spin_unlock(&g_compositor.lock);
    return -1;  /* Window not found */
}

/**
 * compositor_update_window - Update a window in the compositor
 * @window: Window to update
 *
 * Marks the window's area as damaged for redrawing.
 *
 * Returns: 0 on success, error code on failure
 */
int compositor_update_window(window_t *window)
{
    if (!window) {
        return -1;
    }

    if (!g_compositor.initialized) {
        return -1;
    }

    spin_lock(&g_compositor.lock);

    if (!window->visible) {
        spin_unlock(&g_compositor.lock);
        return 0;
    }

    /* Add window bounds to damage list */
    if (g_compositor.num_damage_rects < MAX_DIRTY_RECTS) {
        g_compositor.damage_rects[g_compositor.num_damage_rects] = window->props.bounds;
        g_compositor.num_damage_rects++;
    } else {
        g_compositor.full_damage = true;
    }

    spin_unlock(&g_compositor.lock);

    return 0;
}

/**
 * compositor_sort_windows - Sort windows by z-order
 */
static void compositor_sort_windows(void)
{
    /* Simple bubble sort by z-order */
    for (u32 i = 0; i < g_compositor.num_windows - 1; i++) {
        for (u32 j = 0; j < g_compositor.num_windows - i - 1; j++) {
            if (g_compositor.window_list[j]->z_order > 
                g_compositor.window_list[j + 1]->z_order) {
                window_t *temp = g_compositor.window_list[j];
                g_compositor.window_list[j] = g_compositor.window_list[j + 1];
                g_compositor.window_list[j + 1] = temp;
            }
        }
    }
}

/*===========================================================================*/
/*                         COMPOSITION                                       */
/*===========================================================================*/

/**
 * compositor_composite - Composite all windows to output
 * @output_surface: Output surface
 *
 * Returns: 0 on success, error code on failure
 */
int compositor_composite(void *output_surface)
{
    u64 start_time;
    u64 elapsed_time;

    if (!g_compositor.initialized) {
        return -1;
    }

    spin_lock(&g_compositor.lock);

    start_time = get_time_ms() * 1000;  /* Microseconds */

    /* Sort windows by z-order */
    compositor_sort_windows();

    /* Update damage regions */
    compositor_update_damage();

    /* Clear back buffer if full damage */
    if (g_compositor.full_damage) {
        memset(g_compositor.back_buffer, 0, 
               g_compositor.buffer_width * g_compositor.buffer_height * 4);
    }

    /* Composite each window */
    for (u32 i = 0; i < g_compositor.num_windows; i++) {
        window_t *window = g_compositor.window_list[i];
        
        if (!window->visible) {
            continue;
        }

        /* Apply shadow if enabled */
        if (g_compositor.shadows_enabled) {
            compositor_apply_shadow(window, g_compositor.back_buffer);
        }

        /* Composite window */
        compositor_composite_window(window, g_compositor.back_buffer);

        /* Apply effects */
        if (g_compositor.transparency_enabled || g_compositor.animations_enabled) {
            compositor_apply_effects(window, g_compositor.back_buffer);
        }
    }

    /* Swap buffers */
    void *temp = g_compositor.front_buffer;
    g_compositor.front_buffer = g_compositor.back_buffer;
    g_compositor.back_buffer = temp;
    g_compositor.current_buffer = g_compositor.front_buffer;

    /* Present to output */
    if (output_surface) {
        memcpy(output_surface, g_compositor.current_buffer,
               g_compositor.buffer_width * g_compositor.buffer_height * 4);
    }

    /* Update statistics */
    elapsed_time = get_time_ms() * 1000 - start_time;
    g_compositor.compose_time_us = elapsed_time;
    g_compositor.frame_count++;

    /* Calculate FPS */
    g_compositor.fps = (g_compositor.frame_count * 1000000) / 
                       (get_time_ms() * 1000);

    /* Reset damage */
    g_compositor.num_damage_rects = 0;
    g_compositor.full_damage = false;

    spin_unlock(&g_compositor.lock);

    /* Notify frame complete */
    if (g_compositor.on_frame_complete) {
        g_compositor.on_frame_complete();
    }

    return 0;
}

/**
 * compositor_update_damage - Update damage tracking
 */
static void compositor_update_damage(void)
{
    /* Check for dirty windows */
    for (u32 i = 0; i < g_compositor.num_windows; i++) {
        window_t *window = g_compositor.window_list[i];
        
        if (window->dirty && window->visible) {
            if (g_compositor.num_damage_rects < MAX_DIRTY_RECTS) {
                g_compositor.damage_rects[g_compositor.num_damage_rects] = 
                    window->props.bounds;
                g_compositor.num_damage_rects++;
            } else {
                g_compositor.full_damage = true;
                return;
            }
            window->dirty = false;
        }
    }
}

/**
 * compositor_composite_window - Composite a single window
 * @window: Window to composite
 * @dst: Destination buffer
 *
 * Returns: 0 on success, error code on failure
 */
static int compositor_composite_window(window_t *window, void *dst)
{
    rect_t *bounds = &window->props.bounds;
    u32 *dst_pixels = (u32 *)dst;
    u32 *src_pixels = (u32 *)window->surface;
    
    if (!src_pixels) {
        /* No surface, fill with background color */
        color_t bg = window->props.background;
        for (s32 y = bounds->y; y < bounds->y + bounds->height; y++) {
            for (s32 x = bounds->x; x < bounds->x + bounds->width; x++) {
                if (x >= 0 && x < g_compositor.buffer_width &&
                    y >= 0 && y < g_compositor.buffer_height) {
                    u32 color = (bg.a << 24) | (bg.b << 16) | (bg.g << 8) | bg.r;
                    dst_pixels[y * g_compositor.buffer_width + x] = color;
                }
            }
        }
        return 0;
    }

    /* Copy window surface to destination with alpha blending */
    u32 alpha = window->props.opacity;
    
    for (s32 y = 0; y < bounds->height; y++) {
        for (s32 x = 0; x < bounds->width; x++) {
            s32 dst_x = bounds->x + x;
            s32 dst_y = bounds->y + y;
            
            if (dst_x < 0 || dst_x >= (s32)g_compositor.buffer_width ||
                dst_y < 0 || dst_y >= (s32)g_compositor.buffer_height) {
                continue;
            }

            u32 src_pixel = src_pixels[y * bounds->width + x];
            u32 dst_pixel = dst_pixels[dst_y * g_compositor.buffer_width + dst_x];

            /* Alpha blend */
            u8 src_a = (src_pixel >> 24) & 0xFF;
            u8 src_r = (src_pixel >> 16) & 0xFF;
            u8 src_g = (src_pixel >> 8) & 0xFF;
            u8 src_b = src_pixel & 0xFF;

            /* Apply window opacity */
            src_a = (src_a * alpha) / 255;

            if (src_a == 255) {
                dst_pixels[dst_y * g_compositor.buffer_width + dst_x] = src_pixel;
            } else if (src_a > 0) {
                u8 dst_a = (dst_pixel >> 24) & 0xFF;
                u8 dst_r = (dst_pixel >> 16) & 0xFF;
                u8 dst_g = (dst_pixel >> 8) & 0xFF;
                u8 dst_b = dst_pixel & 0xFF;

                u8 out_a = src_a + dst_a * (255 - src_a) / 255;
                u8 out_r = (src_r * src_a + dst_r * dst_a * (255 - src_a) / 255) / out_a;
                u8 out_g = (src_g * src_a + dst_g * dst_a * (255 - src_a) / 255) / out_a;
                u8 out_b = (src_b * src_a + dst_b * dst_a * (255 - src_a) / 255) / out_a;

                dst_pixels[dst_y * g_compositor.buffer_width + dst_x] =
                    (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
            }
        }
    }

    return 0;
}

/**
 * compositor_apply_shadow - Apply drop shadow to window
 * @window: Window
 * @dst: Destination buffer
 */
static void compositor_apply_shadow(window_t *window, void *dst)
{
    /* Simplified shadow implementation */
    /* In a real implementation, this would:
     * - Render blurred shadow to a separate surface
     * - Offset shadow behind window
     * - Composite shadow before window
     */
    (void)window;
    (void)dst;
}

/**
 * compositor_apply_effects - Apply visual effects to window
 * @window: Window
 * @dst: Destination buffer
 */
static void compositor_apply_effects(window_t *window, void *dst)
{
    /* Apply effects based on window state and settings */
    /* Effects may include:
     * - Opacity animation
     * - Scale animation
     * - Blur (for acrylic/accent effects)
     * - Rounded corners
     */
    (void)window;
    (void)dst;
}

/*===========================================================================*/
/*                         LAYER MANAGEMENT                                  */
/*===========================================================================*/

/**
 * compositor_create_layer - Create a compositor layer
 *
 * Returns: Layer ID, or 0 on failure
 */
u32 compositor_create_layer(void)
{
    if (!g_compositor.initialized) {
        return 0;
    }

    spin_lock(&g_compositor.lock);

    if (g_compositor.num_layers >= MAX_LAYERS) {
        spin_unlock(&g_compositor.lock);
        return 0;
    }

    u32 layer_id = g_compositor.num_layers + 1;
    struct {
        u32 id;
        bool visible;
        u32 z_order;
        void *surface;
        rect_t bounds;
    } *layer = &g_compositor.layers[g_compositor.num_layers];

    layer->id = layer_id;
    layer->visible = true;
    layer->z_order = layer_id;
    layer->surface = NULL;
    layer->bounds = (rect_t){0, 0, g_compositor.buffer_width, g_compositor.buffer_height};

    g_compositor.num_layers++;

    spin_unlock(&g_compositor.lock);

    return layer_id;
}

/**
 * compositor_destroy_layer - Destroy a compositor layer
 * @layer_id: Layer ID
 *
 * Returns: 0 on success, error code on failure
 */
int compositor_destroy_layer(u32 layer_id)
{
    if (layer_id == 0 || layer_id > g_compositor.num_layers) {
        return -1;
    }

    spin_lock(&g_compositor.lock);

    /* Find and remove layer */
    for (u32 i = 0; i < g_compositor.num_layers; i++) {
        if (g_compositor.layers[i].id == layer_id) {
            /* Shift remaining layers */
            for (u32 j = i; j < g_compositor.num_layers - 1; j++) {
                g_compositor.layers[j] = g_compositor.layers[j + 1];
            }
            g_compositor.num_layers--;

            spin_unlock(&g_compositor.lock);
            return 0;
        }
    }

    spin_unlock(&g_compositor.lock);
    return -1;
}

/*===========================================================================*/
/*                         SETTINGS                                          */
/*===========================================================================*/

/**
 * compositor_set_enabled - Enable/disable compositor
 * @enabled: Enable compositor
 */
void compositor_set_enabled(bool enabled)
{
    spin_lock(&g_compositor.lock);
    g_compositor.enabled = enabled;
    spin_unlock(&g_compositor.lock);
}

/**
 * compositor_is_enabled - Check if compositor is enabled
 *
 * Returns: true if enabled, false otherwise
 */
bool compositor_is_enabled(void)
{
    return g_compositor.enabled;
}

/**
 * compositor_set_shadows_enabled - Enable/disable shadows
 * @enabled: Enable shadows
 */
void compositor_set_shadows_enabled(bool enabled)
{
    spin_lock(&g_compositor.lock);
    g_compositor.shadows_enabled = enabled;
    spin_unlock(&g_compositor.lock);
}

/**
 * compositor_set_vsync_enabled - Enable/disable VSync
 * @enabled: Enable VSync
 */
void compositor_set_vsync_enabled(bool enabled)
{
    spin_lock(&g_compositor.lock);
    g_compositor.vsync_enabled = enabled;
    spin_unlock(&g_compositor.lock);
}

/**
 * compositor_get_fps - Get current FPS
 *
 * Returns: Frames per second
 */
u32 compositor_get_fps(void)
{
    return g_compositor.fps;
}

/**
 * compositor_get_compose_time - Get last composition time
 *
 * Returns: Composition time in microseconds
 */
u64 compositor_get_compose_time(void)
{
    return g_compositor.compose_time_us;
}
