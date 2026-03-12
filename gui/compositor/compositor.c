/*
 * NEXUS OS - Hardware-accelerated Compositor Implementation
 * gui/compositor/compositor.c
 *
 * Modern GPU-accelerated compositor with advanced effects
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "compositor.h"
#include "../window-manager/window-manager.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL COMPOSITOR INSTANCE                        */
/*===========================================================================*/

static compositor_t g_compositor;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* Initialization */
static int init_render_backend(compositor_t *comp);
static int init_framebuffers(compositor_t *comp);
static int init_shaders(compositor_t *comp);
static int init_default_surface(compositor_t *comp);

/* Rendering */
static int render_layer(compositor_t *comp, compositor_layer_t *layer);
static int render_cursor(compositor_t *comp);
static int apply_effects(compositor_t *comp);
static int swap_buffers(compositor_t *comp);

/* Damage tracking */
static int calculate_damage(compositor_t *comp);
static bool rect_intersects(rect_t *a, rect_t *b);
static rect_t rect_union(rect_t *a, rect_t *b);

/* GPU operations (stubs for software fallback) */
static int gpu_create_texture(void *data, u32 width, u32 height, u32 *texture_id);
static int gpu_delete_texture(u32 texture_id);
static int gpu_upload_texture(u32 texture_id, const void *data);
static int gpu_draw_quad(float x, float y, float w, float h, u32 texture_id);

/* Utilities */
static void init_software_rendering(compositor_t *comp);
static int software_composite(compositor_t *comp);
static int software_blend_pixel(u8 *dst, const u8 *src, u32 alpha);

/*===========================================================================*/
/*                         COMPOSITOR INITIALIZATION                         */
/*===========================================================================*/

/**
 * compositor_init - Initialize compositor
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_init(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    printk("[COMP] ========================================\n");
    printk("[COMP] NEXUS OS Compositor v%s\n", COMPOSITOR_VERSION);
    printk("[COMP] ========================================\n");
    printk("[COMP] Initializing compositor...\n");
    
    /* Clear structure */
    memset(comp, 0, sizeof(compositor_t));
    comp->initialized = true;
    comp->layer_count = 0;
    comp->next_layer_id = 1;
    comp->surface_count = 0;
    comp->max_surfaces = COMPOSITOR_MAX_SURFACES;
    
    /* Default display settings */
    comp->display_width = 1920;
    comp->display_height = 1080;
    comp->refresh_rate = 60;
    comp->vsync_enabled = true;
    comp->pixel_format = 0x32424752;  /* RGBA8888 */
    comp->color_depth = 32;
    
    /* Default flags */
    comp->flags = COMP_FLAG_VSYNC | COMP_FLAG_SHADOWS | COMP_FLAG_ANIMATIONS;
    
    /* Default effects settings */
    comp->effects_enabled = true;
    comp->blur_radius = 0;
    comp->brightness = 1.0f;
    comp->contrast = 1.0f;
    comp->saturation = 1.0f;
    
    /* Detect render backend */
    printk("[COMP] Detecting render backend...\n");
    comp->render_backend = RENDER_BACKEND_SOFTWARE;
    
    /* Try to initialize hardware backend */
    if (init_render_backend(comp) != 0) {
        printk("[COMP] Hardware acceleration not available, using software rendering\n");
        comp->render_backend = RENDER_BACKEND_SOFTWARE;
        init_software_rendering(comp);
    }
    
    /* Initialize framebuffers */
    printk("[COMP] Initializing framebuffers...\n");
    if (init_framebuffers(comp) != 0) {
        printk("[COMP] Failed to initialize framebuffers\n");
        return -EIO;
    }
    
    /* Initialize shaders (if GPU available) */
    if (comp->render_backend != RENDER_BACKEND_SOFTWARE) {
        init_shaders(comp);
    }
    
    /* Initialize default surface */
    init_default_surface(comp);
    
    /* Initialize cursor */
    comp->cursor_x = comp->display_width / 2;
    comp->cursor_y = comp->display_height / 2;
    comp->cursor_visible = true;
    
    /* Initialize statistics */
    comp->stats.last_frame_time = get_time_ms();
    
    printk("[COMP] Compositor initialized\n");
    printk("[COMP] Backend: %s\n", compositor_get_backend_name(comp->render_backend));
    printk("[COMP] Display: %dx%d@%dHz\n", 
           comp->display_width, comp->display_height, comp->refresh_rate);
    printk("[COMP] VSync: %s\n", comp->vsync_enabled ? "enabled" : "disabled");
    printk("[COMP] ========================================\n");
    
    return 0;
}

/**
 * compositor_run - Start compositor
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_run(compositor_t *comp)
{
    if (!comp || !comp->initialized) {
        return -EINVAL;
    }
    
    printk("[COMP] Starting compositor...\n");
    
    comp->running = true;
    comp->active = true;
    
    return 0;
}

/**
 * compositor_shutdown - Shutdown compositor
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_shutdown(compositor_t *comp)
{
    if (!comp || !comp->initialized) {
        return -EINVAL;
    }
    
    printk("[COMP] Shutting down compositor...\n");
    
    comp->running = false;
    comp->active = false;
    
    /* Destroy all surfaces */
    for (u32 i = 0; i < comp->surface_count; i++) {
        compositor_destroy_surface(comp, &comp->surfaces[i]);
    }
    
    /* Destroy all layers */
    for (u32 i = 0; i < comp->layer_count; i++) {
        compositor_destroy_layer(comp, i);
    }
    
    /* Destroy framebuffers */
    for (u32 i = 0; i < 3; i++) {
        if (comp->framebuffers[i].data) {
            kfree(comp->framebuffers[i].data);
        }
    }
    
    comp->initialized = false;
    
    printk("[COMP] Compositor shutdown complete\n");
    
    return 0;
}

/**
 * compositor_is_running - Check if compositor is running
 * @comp: Pointer to compositor structure
 *
 * Returns: true if running, false otherwise
 */
bool compositor_is_running(compositor_t *comp)
{
    return comp && comp->running;
}

/*===========================================================================*/
/*                         INITIALIZATION HELPERS                            */
/*===========================================================================*/

/**
 * init_render_backend - Initialize render backend
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int init_render_backend(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* In real implementation, would detect and initialize GPU */
    /* For now, return error to fall back to software rendering */
    
    /* Check for OpenGL */
    /* Check for Vulkan */
    /* Check for DRM/KMS */
    
    return -ENOSYS;  /* Not implemented yet */
}

/**
 * init_framebuffers - Initialize framebuffers
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int init_framebuffers(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    u32 fb_size = comp->display_width * comp->display_height * 4;  /* RGBA8888 */
    
    /* Allocate triple buffer */
    for (u32 i = 0; i < 3; i++) {
        framebuffer_t *fb = &comp->framebuffers[i];
        
        fb->fb_id = i;
        fb->width = comp->display_width;
        fb->height = comp->display_height;
        fb->format = comp->pixel_format;
        fb->stride = comp->display_width * 4;
        fb->data = kmalloc(fb_size);
        
        if (!fb->data) {
            printk("[COMP] Failed to allocate framebuffer %d\n", i);
            return -ENOMEM;
        }
        
        memset(fb->data, 0, fb_size);
        fb->is_flipped = (i == 0);
    }
    
    comp->front_buffer = 0;
    comp->back_buffer = 1;
    comp->render_buffer = 2;
    
    printk("[COMP] Allocated 3 framebuffers (%d bytes each)\n", fb_size);
    
    return 0;
}

/**
 * init_shaders - Initialize shaders
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
static int init_shaders(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* In real implementation, would compile GLSL shaders */
    /* For now, just log */
    
    printk("[COMP] Shaders initialized (stub)\n");
    
    return 0;
}

/**
 * init_default_surface - Initialize default surface
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
static int init_default_surface(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* Create default surface for testing */
    gpu_surface_t *surf = compositor_create_surface(comp, 64, 64, comp->pixel_format);
    if (surf) {
        comp->default_texture = surf->gpu_handle;
    }
    
    return 0;
}

/**
 * init_software_rendering - Initialize software rendering
 * @comp: Pointer to compositor structure
 */
static void init_software_rendering(compositor_t *comp)
{
    if (!comp) {
        return;
    }
    
    printk("[COMP] Using software rendering backend\n");
    
    /* Software rendering doesn't need GPU initialization */
}

/*===========================================================================*/
/*                         SURFACE MANAGEMENT                                */
/*===========================================================================*/

/**
 * compositor_create_surface - Create GPU surface
 * @comp: Pointer to compositor structure
 * @width: Surface width
 * @height: Surface height
 * @format: Pixel format
 *
 * Returns: Pointer to created surface, or NULL on failure
 */
gpu_surface_t *compositor_create_surface(compositor_t *comp, u32 width, u32 height, u32 format)
{
    if (!comp || width == 0 || height == 0) {
        return NULL;
    }
    
    if (comp->surface_count >= comp->max_surfaces) {
        printk("[COMP] Maximum surface count reached\n");
        return NULL;
    }
    
    /* Allocate surface */
    gpu_surface_t *surf = &comp->surfaces[comp->surface_count++];
    memset(surf, 0, sizeof(gpu_surface_t));
    
    surf->surface_id = comp->surface_count;
    surf->width = width;
    surf->height = height;
    surf->format = format;
    surf->stride = width * 4;  /* RGBA8888 */
    
    /* Allocate surface data */
    u32 size = width * height * 4;
    surf->data = kmalloc(size);
    if (!surf->data) {
        comp->surface_count--;
        return NULL;
    }
    
    memset(surf->data, 0, size);
    
    /* Create GPU texture if hardware accelerated */
    if (comp->render_backend != RENDER_BACKEND_SOFTWARE) {
        gpu_create_texture(surf->data, width, height, &surf->gpu_handle);
        surf->is_gpu = true;
    }
    
    if (comp->on_surface_created) {
        comp->on_surface_created(surf);
    }
    
    return surf;
}

/**
 * compositor_destroy_surface - Destroy GPU surface
 * @comp: Pointer to compositor structure
 * @surface: Surface to destroy
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_destroy_surface(compositor_t *comp, gpu_surface_t *surface)
{
    if (!comp || !surface) {
        return -EINVAL;
    }
    
    /* Delete GPU texture */
    if (surface->is_gpu && surface->gpu_handle) {
        gpu_delete_texture(surface->gpu_handle);
    }
    
    /* Free surface data */
    if (surface->data) {
        kfree(surface->data);
        surface->data = NULL;
    }
    
    /* Remove from surface pool */
    for (u32 i = 0; i < comp->surface_count; i++) {
        if (&comp->surfaces[i] == surface) {
            /* Shift remaining surfaces */
            for (u32 j = i; j < comp->surface_count - 1; j++) {
                comp->surfaces[j] = comp->surfaces[j + 1];
            }
            comp->surface_count--;
            break;
        }
    }
    
    if (comp->on_surface_destroyed) {
        comp->on_surface_destroyed(surface);
    }
    
    return 0;
}

/**
 * compositor_resize_surface - Resize GPU surface
 * @comp: Pointer to compositor structure
 * @surface: Surface to resize
 * @width: New width
 * @height: New height
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_resize_surface(compositor_t *comp, gpu_surface_t *surface, u32 width, u32 height)
{
    if (!comp || !surface) {
        return -EINVAL;
    }
    
    /* Free old data */
    if (surface->data) {
        kfree(surface->data);
    }
    
    /* Allocate new data */
    u32 size = width * height * 4;
    surface->data = kmalloc(size);
    if (!surface->data) {
        return -ENOMEM;
    }
    
    surface->width = width;
    surface->height = height;
    surface->stride = width * 4;
    
    /* Recreate GPU texture */
    if (surface->is_gpu) {
        gpu_delete_texture(surface->gpu_handle);
        gpu_create_texture(surface->data, width, height, &surface->gpu_handle);
    }
    
    return 0;
}

/**
 * compositor_lock_surface - Lock surface for CPU access
 * @comp: Pointer to compositor structure
 * @surface: Surface to lock
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_lock_surface(compositor_t *comp, gpu_surface_t *surface)
{
    if (!comp || !surface) {
        return -EINVAL;
    }
    
    if (surface->is_locked) {
        return -EBUSY;
    }
    
    /* Download from GPU if needed */
    if (surface->is_gpu) {
        /* In real implementation, would download from GPU */
    }
    
    surface->is_locked = true;
    
    return 0;
}

/**
 * compositor_unlock_surface - Unlock surface
 * @comp: Pointer to compositor structure
 * @surface: Surface to unlock
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_unlock_surface(compositor_t *comp, gpu_surface_t *surface)
{
    if (!comp || !surface) {
        return -EINVAL;
    }
    
    if (!surface->is_locked) {
        return 0;
    }
    
    /* Upload to GPU if needed */
    if (surface->is_gpu) {
        gpu_upload_texture(surface->gpu_handle, surface->data);
    }
    
    surface->is_locked = false;
    
    return 0;
}

/**
 * compositor_upload_surface - Upload data to surface
 * @comp: Pointer to compositor structure
 * @surface: Surface to upload to
 * @data: Data to upload
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_upload_surface(compositor_t *comp, gpu_surface_t *surface, const void *data)
{
    if (!comp || !surface || !data) {
        return -EINVAL;
    }
    
    u32 size = surface->width * surface->height * 4;
    memcpy(surface->data, data, size);
    
    /* Upload to GPU */
    if (surface->is_gpu) {
        gpu_upload_texture(surface->gpu_handle, surface->data);
    }
    
    return 0;
}

/**
 * compositor_download_surface - Download data from surface
 * @comp: Pointer to compositor structure
 * @surface: Surface to download from
 * @data: Buffer to download to
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_download_surface(compositor_t *comp, gpu_surface_t *surface, void *data)
{
    if (!comp || !surface || !data) {
        return -EINVAL;
    }
    
    /* Download from GPU if needed */
    if (surface->is_gpu) {
        /* In real implementation, would download from GPU */
    }
    
    u32 size = surface->width * surface->height * 4;
    memcpy(data, surface->data, size);
    
    return 0;
}

/*===========================================================================*/
/*                         LAYER MANAGEMENT                                  */
/*===========================================================================*/

/**
 * compositor_create_layer - Create compositor layer
 * @comp: Pointer to compositor structure
 * @surface: Surface for layer
 * @layer_id: Pointer to store layer ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_create_layer(compositor_t *comp, gpu_surface_t *surface, u32 *layer_id)
{
    if (!comp || !surface || !layer_id) {
        return -EINVAL;
    }
    
    if (comp->layer_count >= COMPOSITOR_MAX_LAYERS) {
        printk("[COMP] Maximum layer count reached\n");
        return -ENOSPC;
    }
    
    compositor_layer_t *layer = &comp->layers[comp->layer_count++];
    memset(layer, 0, sizeof(compositor_layer_t));
    
    layer->layer_id = comp->next_layer_id++;
    layer->surface = surface;
    layer->z_order = comp->layer_count - 1;
    layer->alpha = 255;
    layer->visible = true;
    layer->enabled = true;
    layer->src_rect.x = 0;
    layer->src_rect.y = 0;
    layer->src_rect.width = surface->width;
    layer->src_rect.height = surface->height;
    layer->dst_rect = layer->src_rect;
    
    *layer_id = layer->layer_id;
    
    return 0;
}

/**
 * compositor_destroy_layer - Destroy compositor layer
 * @comp: Pointer to compositor structure
 * @layer_id: Layer ID to destroy
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_destroy_layer(compositor_t *comp, u32 layer_id)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* Find and remove layer */
    for (u32 i = 0; i < comp->layer_count; i++) {
        if (comp->layers[i].layer_id == layer_id) {
            /* Shift remaining layers */
            for (u32 j = i; j < comp->layer_count - 1; j++) {
                comp->layers[j] = comp->layers[j + 1];
            }
            comp->layer_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * compositor_set_layer_position - Set layer position
 * @comp: Pointer to compositor structure
 * @layer_id: Layer ID
 * @x: X position
 * @y: Y position
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_set_layer_position(compositor_t *comp, u32 layer_id, s32 x, s32 y)
{
    if (!comp) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < comp->layer_count; i++) {
        if (comp->layers[i].layer_id == layer_id) {
            compositor_layer_t *layer = &comp->layers[i];
            layer->dst_rect.x = x;
            layer->dst_rect.y = y;
            
            /* Add damage */
            compositor_add_damage(comp, &layer->dst_rect);
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * compositor_set_layer_size - Set layer size
 * @comp: Pointer to compositor structure
 * @layer_id: Layer ID
 * @width: Width
 * @height: Height
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_set_layer_size(compositor_t *comp, u32 layer_id, u32 width, u32 height)
{
    if (!comp) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < comp->layer_count; i++) {
        if (comp->layers[i].layer_id == layer_id) {
            compositor_layer_t *layer = &comp->layers[i];
            layer->dst_rect.width = width;
            layer->dst_rect.height = height;
            
            /* Add damage */
            compositor_add_damage(comp, &layer->dst_rect);
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * compositor_set_layer_z_order - Set layer Z-order
 * @comp: Pointer to compositor structure
 * @layer_id: Layer ID
 * @z_order: Z-order value
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_set_layer_z_order(compositor_t *comp, u32 layer_id, u32 z_order)
{
    if (!comp) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < comp->layer_count; i++) {
        if (comp->layers[i].layer_id == layer_id) {
            comp->layers[i].z_order = z_order;
            
            /* Sort layers by Z-order */
            /* Simple bubble sort for now */
            for (u32 j = 0; j < comp->layer_count - 1; j++) {
                if (comp->layers[j].z_order > comp->layers[j + 1].z_order) {
                    compositor_layer_t temp = comp->layers[j];
                    comp->layers[j] = comp->layers[j + 1];
                    comp->layers[j + 1] = temp;
                }
            }
            
            /* Full damage for reorder */
            comp->full_damage = true;
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * compositor_set_layer_alpha - Set layer alpha
 * @comp: Pointer to compositor structure
 * @layer_id: Layer ID
 * @alpha: Alpha value (0-255)
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_set_layer_alpha(compositor_t *comp, u32 layer_id, u32 alpha)
{
    if (!comp) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < comp->layer_count; i++) {
        if (comp->layers[i].layer_id == layer_id) {
            comp->layers[i].alpha = alpha & 0xFF;
            compositor_add_damage(comp, &comp->layers[i].dst_rect);
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * compositor_set_layer_visible - Set layer visibility
 * @comp: Pointer to compositor structure
 * @layer_id: Layer ID
 * @visible: Visibility flag
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_set_layer_visible(compositor_t *comp, u32 layer_id, bool visible)
{
    if (!comp) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < comp->layer_count; i++) {
        if (comp->layers[i].layer_id == layer_id) {
            comp->layers[i].visible = visible;
            
            /* Full damage when visibility changes */
            comp->full_damage = true;
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * compositor_set_layer_transform - Set layer transform
 * @comp: Pointer to compositor structure
 * @layer_id: Layer ID
 * @transform: Transform matrix
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_set_layer_transform(compositor_t *comp, u32 layer_id, matrix4_t *transform)
{
    if (!comp || !transform) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < comp->layer_count; i++) {
        if (comp->layers[i].layer_id == layer_id) {
            memcpy(&comp->layers[i].transform, transform, sizeof(matrix4_t));
            comp->full_damage = true;
            return 0;
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

/**
 * compositor_render - Render frame
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int compositor_render(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    u64 frame_start = get_time_ms();
    
    /* Calculate damage */
    calculate_damage(comp);
    
    /* Composite layers */
    compositor_composite_layers(comp);
    
    /* Render cursor */
    render_cursor(comp);
    
    /* Apply effects */
    apply_effects(comp);
    
    /* Present frame */
    compositor_present(comp);
    
    /* Update statistics */
    u64 frame_end = get_time_ms();
    comp->stats.frame_count++;
    comp->stats.render_time_us = (u32)((frame_end - frame_start) * 1000);
    
    /* Calculate FPS */
    u64 elapsed = frame_end - comp->stats.last_frame_time;
    if (elapsed >= 1000) {
        comp->stats.fps = (u32)(comp->stats.frame_count * 1000 / elapsed);
        comp->stats.frame_count = 0;
        comp->stats.last_frame_time = frame_end;
    }
    
    return 0;
}

/**
 * compositor_render_frame - Render single frame
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
int compositor_render_frame(compositor_t *comp)
{
    return compositor_render(comp);
}

/**
 * compositor_composite_layers - Composite all layers
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
int compositor_composite_layers(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* Use software compositing for now */
    if (comp->render_backend == RENDER_BACKEND_SOFTWARE) {
        return software_composite(comp);
    }
    
    /* Hardware compositing */
    for (u32 i = 0; i < comp->layer_count; i++) {
        compositor_layer_t *layer = &comp->layers[i];
        
        if (layer->visible && layer->enabled && layer->surface) {
            render_layer(comp, layer);
            comp->stats.layers_composited++;
        }
    }
    
    return 0;
}

/**
 * render_layer - Render single layer
 * @comp: Pointer to compositor structure
 * @layer: Layer to render
 *
 * Returns: 0 on success
 */
static int render_layer(compositor_t *comp, compositor_layer_t *layer)
{
    if (!comp || !layer) {
        return -EINVAL;
    }
    
    /* Get render buffer */
    framebuffer_t *fb = &comp->framebuffers[comp->render_buffer];
    
    /* In real implementation, would use GPU to render layer */
    /* For now, software blit */
    
    /* Simple blit for software rendering */
    u8 *src = (u8 *)layer->surface->data;
    u8 *dst = (u8 *)fb->data;
    
    s32 src_x = layer->src_rect.x;
    s32 src_y = layer->src_rect.y;
    s32 dst_x = layer->dst_rect.x;
    s32 dst_y = layer->dst_rect.y;
    s32 w = layer->dst_rect.width;
    s32 h = layer->dst_rect.height;
    
    /* Clip to framebuffer */
    if (dst_x < 0) { src_x -= dst_x; w += dst_x; dst_x = 0; }
    if (dst_y < 0) { src_y -= dst_y; h += dst_y; dst_y = 0; }
    if (dst_x + w > (s32)fb->width) w = fb->width - dst_x;
    if (dst_y + h > (s32)fb->height) h = fb->height - dst_y;
    
    if (w <= 0 || h <= 0) return 0;
    
    /* Blit with alpha */
    for (s32 y = 0; y < h; y++) {
        for (s32 x = 0; x < w; x++) {
            u32 src_idx = ((src_y + y) * layer->surface->stride + (src_x + x) * 4);
            u32 dst_idx = ((dst_y + y) * fb->stride + (dst_x + x) * 4);
            
            if (layer->alpha == 255) {
                /* Opaque copy */
                dst[dst_idx + 0] = src[src_idx + 0];
                dst[dst_idx + 1] = src[src_idx + 1];
                dst[dst_idx + 2] = src[src_idx + 2];
                dst[dst_idx + 3] = 255;
            } else {
                /* Alpha blend */
                software_blend_pixel(&dst[dst_idx], &src[src_idx], layer->alpha);
            }
        }
    }
    
    return 0;
}

/**
 * render_cursor - Render cursor
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
static int render_cursor(compositor_t *comp)
{
    if (!comp || !comp->cursor_visible || !comp->cursor_surface) {
        return 0;
    }
    
    /* Render cursor at current position */
    /* In real implementation, would use hardware cursor or overlay */
    
    return 0;
}

/**
 * apply_effects - Apply post-processing effects
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
static int apply_effects(compositor_t *comp)
{
    if (!comp || !comp->effects_enabled) {
        return 0;
    }
    
    /* Apply brightness/contrast/saturation if needed */
    if (comp->brightness != 1.0f || comp->contrast != 1.0f || comp->saturation != 1.0f) {
        /* In real implementation, would use GPU shader */
    }
    
    /* Apply blur if needed */
    if (comp->blur_radius > 0) {
        /* In real implementation, would use Gaussian blur shader */
    }
    
    return 0;
}

/**
 * compositor_present - Present frame to display
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
int compositor_present(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* Swap buffers */
    swap_buffers(comp);
    
    /* Wait for VSync if enabled */
    if (comp->vsync_enabled) {
        compositor_wait_vsync(comp);
    }
    
    return 0;
}

/**
 * swap_buffers - Swap front and back buffers
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
static int swap_buffers(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* Triple buffering */
    u32 temp = comp->front_buffer;
    comp->front_buffer = comp->back_buffer;
    comp->back_buffer = comp->render_buffer;
    comp->render_buffer = temp;
    
    /* Mark old render buffer as clean */
    comp->framebuffers[comp->render_buffer].is_flipped = false;
    
    return 0;
}

/**
 * compositor_wait_vsync - Wait for VSync
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
int compositor_wait_vsync(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    /* In real implementation, would wait for VBlank */
    /* For now, simple delay for 60Hz */
    if (comp->refresh_rate > 0) {
        u32 delay_ms = 1000 / comp->refresh_rate;
        delay_ms(delay_ms);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         DAMAGE TRACKING                                   */
/*===========================================================================*/

/**
 * compositor_add_damage - Add damage rectangle
 * @comp: Pointer to compositor structure
 * @rect: Damage rectangle
 *
 * Returns: 0 on success
 */
int compositor_add_damage(compositor_t *comp, rect_t *rect)
{
    if (!comp || !rect) {
        return -EINVAL;
    }
    
    if (comp->damage.count >= COMPOSITOR_MAX_DIRTY_RECTS) {
        comp->full_damage = true;
        return 0;
    }
    
    comp->damage.rects[comp->damage.count++] = *rect;
    
    /* Update bounding box */
    if (comp->damage.count == 1) {
        comp->damage.bounding_box = *rect;
    } else {
        comp->damage.bounding_box = rect_union(&comp->damage.bounding_box, rect);
    }
    
    return 0;
}

/**
 * compositor_clear_damage - Clear damage
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
int compositor_clear_damage(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    comp->damage.count = 0;
    comp->full_damage = false;
    
    return 0;
}

/**
 * compositor_get_damage - Get damage region
 * @comp: Pointer to compositor structure
 *
 * Returns: Damage region pointer
 */
damage_region_t *compositor_get_damage(compositor_t *comp)
{
    return comp ? &comp->damage : NULL;
}

/**
 * compositor_is_damaged - Check if compositor has damage
 * @comp: Pointer to compositor structure
 *
 * Returns: true if damaged, false otherwise
 */
bool compositor_is_damaged(compositor_t *comp)
{
    return comp && (comp->full_damage || comp->damage.count > 0);
}

/**
 * calculate_damage - Calculate damage for frame
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
static int calculate_damage(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    if (comp->full_damage) {
        /* Full screen damage */
        comp->damage.count = 1;
        comp->damage.rects[0].x = 0;
        comp->damage.rects[0].y = 0;
        comp->damage.rects[0].width = comp->display_width;
        comp->damage.rects[0].height = comp->display_height;
        comp->damage.bounding_box = comp->damage.rects[0];
        comp->full_damage = false;
    }
    
    return 0;
}

/**
 * rect_intersects - Check if two rectangles intersect
 * @a: First rectangle
 * @b: Second rectangle
 *
 * Returns: true if intersect, false otherwise
 */
static bool rect_intersects(rect_t *a, rect_t *b)
{
    return !(a->x + a->width <= b->x ||
             b->x + b->width <= a->x ||
             a->y + a->height <= b->y ||
             b->y + b->height <= a->y);
}

/**
 * rect_union - Get union of two rectangles
 * @a: First rectangle
 * @b: Second rectangle
 *
 * Returns: Union rectangle
 */
static rect_t rect_union(rect_t *a, rect_t *b)
{
    rect_t result;
    result.x = (a->x < b->x) ? a->x : b->x;
    result.y = (a->y < b->y) ? a->y : b->y;
    result.width = ((a->x + a->width > b->x + b->width) ? (a->x + a->width) : (b->x + b->width)) - result.x;
    result.height = ((a->y + a->height > b->y + b->height) ? (a->y + a->height) : (b->y + b->height)) - result.y;
    return result;
}

/*===========================================================================*/
/*                         SOFTWARE RENDERING                                */
/*===========================================================================*/

/**
 * software_composite - Software compositing
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
static int software_composite(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    framebuffer_t *fb = &comp->framebuffers[comp->render_buffer];
    
    /* Clear framebuffer */
    memset(fb->data, 0, fb->width * fb->height * 4);
    
    /* Render all layers */
    for (u32 i = 0; i < comp->layer_count; i++) {
        compositor_layer_t *layer = &comp->layers[i];
        
        if (layer->visible && layer->enabled && layer->surface) {
            render_layer(comp, layer);
            comp->stats.surfaces_rendered++;
        }
    }
    
    return 0;
}

/**
 * software_blend_pixel - Blend pixel with alpha
 * @dst: Destination pixel (RGBA)
 * @src: Source pixel (RGBA)
 * @alpha: Alpha value (0-255)
 *
 * Returns: 0 on success
 */
static int software_blend_pixel(u8 *dst, const u8 *src, u32 alpha)
{
    u32 a = alpha;
    u32 ia = 255 - a;
    
    dst[0] = (src[0] * a + dst[0] * ia) >> 8;
    dst[1] = (src[1] * a + dst[1] * ia) >> 8;
    dst[2] = (src[2] * a + dst[2] * ia) >> 8;
    dst[3] = 255;
    
    return 0;
}

/*===========================================================================*/
/*                         DISPLAY MANAGEMENT                                */
/*===========================================================================*/

/**
 * compositor_set_display_mode - Set display mode
 * @comp: Pointer to compositor structure
 * @width: Width
 * @height: Height
 * @refresh: Refresh rate
 *
 * Returns: 0 on success
 */
int compositor_set_display_mode(compositor_t *comp, u32 width, u32 height, u32 refresh)
{
    if (!comp) {
        return -EINVAL;
    }
    
    comp->display_width = width;
    comp->display_height = height;
    comp->refresh_rate = refresh;
    
    /* Reallocate framebuffers */
    /* In real implementation, would mode-set */
    
    return 0;
}

/**
 * compositor_set_brightness - Set display brightness
 * @comp: Pointer to compositor structure
 * @brightness: Brightness value (0.0-1.0)
 *
 * Returns: 0 on success
 */
int compositor_set_brightness(compositor_t *comp, float brightness)
{
    if (!comp) {
        return -EINVAL;
    }
    
    comp->brightness = (brightness < 0.0f) ? 0.0f : (brightness > 1.0f) ? 1.0f : brightness;
    
    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * compositor_get_stats - Get render statistics
 * @comp: Pointer to compositor structure
 *
 * Returns: Statistics pointer
 */
render_stats_t *compositor_get_stats(compositor_t *comp)
{
    return comp ? &comp->stats : NULL;
}

/**
 * compositor_reset_stats - Reset statistics
 * @comp: Pointer to compositor structure
 *
 * Returns: 0 on success
 */
int compositor_reset_stats(compositor_t *comp)
{
    if (!comp) {
        return -EINVAL;
    }
    
    memset(&comp->stats, 0, sizeof(render_stats_t));
    comp->stats.last_frame_time = get_time_ms();
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * compositor_get_backend_name - Get backend name
 * @backend: Backend type
 *
 * Returns: Backend name string
 */
const char *compositor_get_backend_name(u32 backend)
{
    switch (backend) {
        case RENDER_BACKEND_SOFTWARE:  return "Software";
        case RENDER_BACKEND_OPENGL:    return "OpenGL";
        case RENDER_BACKEND_OPENGL_ES: return "OpenGL ES";
        case RENDER_BACKEND_VULKAN:    return "Vulkan";
        case RENDER_BACKEND_DIRECTFB:  return "DirectFB";
        case RENDER_BACKEND_DRM:       return "DRM/KMS";
        default:                       return "Unknown";
    }
}

/**
 * compositor_get_pixel_format - Get pixel format from name
 * @name: Format name
 *
 * Returns: Pixel format value
 */
u32 compositor_get_pixel_format(const char *name)
{
    if (!name) return 0;
    
    if (strcmp(name, "RGBA8888") == 0) return 0x32424752;
    if (strcmp(name, "RGB888") == 0) return 0x342424;
    if (strcmp(name, "RGB565") == 0) return 0x36314752;
    
    return 0;
}

/**
 * compositor_get_pixel_format_name - Get pixel format name
 * @format: Pixel format
 *
 * Returns: Format name string
 */
const char *compositor_get_pixel_format_name(u32 format)
{
    switch (format) {
        case 0x32424752:  return "RGBA8888";
        case 0x342424:    return "RGB888";
        case 0x36314752:  return "RGB565";
        default:          return "Unknown";
    }
}

/**
 * compositor_get_bpp - Get bits per pixel
 * @format: Pixel format
 *
 * Returns: Bits per pixel
 */
u32 compositor_get_bpp(u32 format)
{
    switch (format) {
        case 0x32424752:  return 32;
        case 0x342424:    return 24;
        case 0x36314752:  return 16;
        default:          return 32;
    }
}

/**
 * compositor_get_stride - Get stride for format
 * @width: Width in pixels
 * @format: Pixel format
 *
 * Returns: Stride in bytes
 */
u32 compositor_get_stride(u32 width, u32 format)
{
    return width * compositor_get_bpp(format) / 8;
}

/*===========================================================================*/
/*                         GPU STUBS                                         */
/*===========================================================================*/

static int gpu_create_texture(void *data, u32 width, u32 height, u32 *texture_id)
{
    (void)data; (void)width; (void)height; (void)texture_id;
    return -ENOSYS;
}

static int gpu_delete_texture(u32 texture_id)
{
    (void)texture_id;
    return -ENOSYS;
}

static int gpu_upload_texture(u32 texture_id, const void *data)
{
    (void)texture_id; (void)data;
    return -ENOSYS;
}

static int gpu_draw_quad(float x, float y, float w, float h, u32 texture_id)
{
    (void)x; (void)y; (void)w; (void)h; (void)texture_id;
    return -ENOSYS;
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * compositor_get_instance - Get global compositor instance
 *
 * Returns: Pointer to global instance
 */
compositor_t *compositor_get_instance(void)
{
    return &g_compositor;
}
