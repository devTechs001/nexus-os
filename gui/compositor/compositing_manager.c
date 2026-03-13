/*
 * NEXUS OS - Compositing Window Manager
 * gui/compositor/compositing_manager.c
 *
 * Window decorations, animations, transparency effects, compositing
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "compositor.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         COMPOSITOR CONFIGURATION                          */
/*===========================================================================*/

#define COMP_MAX_WINDOWS          256
#define COMP_MAX_ANIMATIONS       64
#define COMP_VSYNC_INTERVAL       1
#define COMP_SHADOW_SIZE          20
#define COMP_BLUR_RADIUS          10

/*===========================================================================*/
/*                         ANIMATION TYPES                                   */
/*===========================================================================*/

#define ANIM_NONE                 0
#define ANIM_FADE                 1
#define ANIM_SLIDE                2
#define ANIM_ZOOM                 3
#define ANIM_SCALE                4
#define ANIM_FLIP                 5
#define ANIM_GEL                  6  /* Mac-like gelatinous effect */

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 anim_id;
    u32 type;
    u32 target_type;              /* WINDOW/ICON/TASKBAR */
    u32 target_id;
    u32 duration_ms;
    u32 start_time;
    u32 progress;  /* 0-256 range */
    float start_value;
    float end_value;
    bool running;
    bool reverse;
    int (*easing)(int);
} comp_animation_t;

typedef struct {
    u32 window_id;
    void *texture;
    rect_t geometry;
    rect_t shadow_rect;
    color_t shadow_color;
    u32 shadow_blur;
    u32 corner_radius;
    u32 border_width;
    color_t border_color;
    u32 opacity;
    bool has_shadow;
    bool has_border;
    bool rounded_corners;
} comp_window_decor_t;

typedef struct {
    bool initialized;
    bool compositing_enabled;
    bool hw_accel;
    bool vsync_enabled;
    u32 refresh_rate;
    u64 frame_count;
    u64 last_frame_time;
    u32 fps;
    comp_window_decor_t decorations[COMP_MAX_WINDOWS];
    u32 decor_count;
    comp_animation_t animations[COMP_MAX_ANIMATIONS];
    u32 anim_count;
    u32 active_animations;
    bool animations_enabled;
    u32 animation_speed;          /* 50-200% */
    color_t clear_color;
    void *output_surface;
    void *vsync_event;
    spinlock_t lock;
} compositor_state_t;

static compositor_state_t g_comp;

/*===========================================================================*/
/*                         EASING FUNCTIONS                                  */
/*===========================================================================*/

/* Integer-based easing functions to avoid SSE on x86_64 */
static int ease_linear(int t)
{
    return t;
}

static int ease_in_quad(int t)
{
    return (t * t) / 256;
}

static int ease_out_quad(int t)
{
    return (t * (512 - t)) / 256;
}

static int ease_in_out_quad(int t)
{
    if (t < 128) return (2 * t * t) / 256;
    return (-1 + (8 - 2 * t) * t / 256);
}

static int ease_out_cubic(int t)
{
    int inv = 256 - t;
    return 256 - (inv * inv * inv) / 65536;
}

static int ease_in_out_cubic(int t)
{
    if (t < 128) return (4 * t * t * t) / 65536;
    return 256 - (pow(-2 * t + 512, 3) / 2) / 65536;
}

static int ease_out_elastic(int t)
{
    /* Simplified elastic easing using integer math */
    if (t == 0 || t == 256) return t;
    return 256 + (t * (256 - t)) / 64;
}

static int ease_out_bounce(int t)
{
    if (t < 93) return (7 * t * t) / 256;
    if (t < 186) return (7 * (t - 140) * (t - 140)) / 256 + 192;
    return (7 * (t - 232) * (t - 232)) / 256 + 240;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int compositor_init(void)
{
    printk("[COMP] ========================================\n");
    printk("[COMP] NEXUS OS Compositing Manager\n");
    printk("[COMP] ========================================\n");
    
    memset(&g_comp, 0, sizeof(compositor_state_t));
    g_comp.initialized = true;
    g_comp.compositing_enabled = true;
    g_comp.hw_accel = true;
    g_comp.vsync_enabled = true;
    g_comp.refresh_rate = 60;
    g_comp.animations_enabled = true;
    g_comp.animation_speed = 100;
    spinlock_init(&g_comp.lock);
    
    /* Default clear color (dark) */
    g_comp.clear_color.r = 30;
    g_comp.clear_color.g = 30;
    g_comp.clear_color.b = 35;
    g_comp.clear_color.a = 255;
    
    printk("[COMP] Compositor initialized\n");
    printk("[COMP]   Hardware acceleration: %s\n", g_comp.hw_accel ? "ON" : "OFF");
    printk("[COMP]   VSync: %s\n", g_comp.vsync_enabled ? "ON" : "OFF");
    printk("[COMP]   Animations: %s\n", g_comp.animations_enabled ? "ON" : "OFF");
    
    return 0;
}

void compositor_shutdown(void)
{
    printk("[COMP] Shutting down compositor...\n");

    g_comp.compositing_enabled = false;
    g_comp.initialized = false;
}

/*===========================================================================*/
/*                         WINDOW DECORATIONS                                */
/*===========================================================================*/

comp_window_decor_t *compositor_create_decoration(u32 window_id)
{
    spinlock_lock(&g_comp.lock);
    
    if (g_comp.decor_count >= COMP_MAX_WINDOWS) {
        spinlock_unlock(&g_comp.lock);
        return NULL;
    }
    
    comp_window_decor_t *decor = &g_comp.decorations[g_comp.decor_count++];
    decor->window_id = window_id;
    decor->shadow_color.r = 0;
    decor->shadow_color.g = 0;
    decor->shadow_color.b = 0;
    decor->shadow_color.a = 128;
    decor->shadow_blur = COMP_SHADOW_SIZE;
    decor->corner_radius = 8;
    decor->border_width = 1;
    decor->border_color.r = 60;
    decor->border_color.g = 60;
    decor->border_color.b = 65;
    decor->border_color.a = 255;
    decor->opacity = 255;
    decor->has_shadow = true;
    decor->has_border = true;
    decor->rounded_corners = true;
    
    spinlock_unlock(&g_comp.lock);
    return decor;
}

int compositor_set_shadow(u32 window_id, bool enabled, u32 blur, color_t *color)
{
    for (u32 i = 0; i < g_comp.decor_count; i++) {
        if (g_comp.decorations[i].window_id == window_id) {
            comp_window_decor_t *d = &g_comp.decorations[i];
            d->has_shadow = enabled;
            if (blur > 0) d->shadow_blur = blur;
            if (color) d->shadow_color = *color;
            return 0;
        }
    }
    return -ENOENT;
}

int compositor_set_corner_radius(u32 window_id, u32 radius)
{
    for (u32 i = 0; i < g_comp.decor_count; i++) {
        if (g_comp.decorations[i].window_id == window_id) {
            g_comp.decorations[i].corner_radius = radius;
            return 0;
        }
    }
    return -ENOENT;
}

int compositor_set_border(u32 window_id, u32 width, color_t *color)
{
    for (u32 i = 0; i < g_comp.decor_count; i++) {
        if (g_comp.decorations[i].window_id == window_id) {
            comp_window_decor_t *d = &g_comp.decorations[i];
            d->has_border = (width > 0);
            d->border_width = width;
            if (color) d->border_color = *color;
            return 0;
        }
    }
    return -ENOENT;
}

int compositor_set_opacity(u32 window_id, u32 opacity)
{
    for (u32 i = 0; i < g_comp.decor_count; i++) {
        if (g_comp.decorations[i].window_id == window_id) {
            g_comp.decorations[i].opacity = (opacity > 255) ? 255 : opacity;
            return 0;
        }
    }
    return -ENOENT;
}

/*===========================================================================*/
/*                         ANIMATION SYSTEM                                  */
/*===========================================================================*/

comp_animation_t *compositor_create_animation(u32 type, u32 target_id, 
                                               u32 duration_ms)
{
    spinlock_lock(&g_comp.lock);
    
    if (g_comp.anim_count >= COMP_MAX_ANIMATIONS) {
        spinlock_unlock(&g_comp.lock);
        return NULL;
    }
    
    comp_animation_t *anim = &g_comp.animations[g_comp.anim_count++];
    anim->anim_id = g_comp.anim_count;
    anim->type = type;
    anim->target_type = 1;  /* WINDOW */
    anim->target_id = target_id;
    anim->duration_ms = duration_ms * 100 / g_comp.animation_speed;
    anim->start_time = ktime_get_ms();
    anim->progress = 0;
    anim->start_value = 0;
    anim->end_value = 1;
    anim->running = true;
    anim->reverse = false;
    anim->easing = ease_out_cubic;  /* Default easing */
    
    g_comp.active_animations++;
    spinlock_unlock(&g_comp.lock);
    
    return anim;
}

int compositor_set_animation_easing(comp_animation_t *anim, int (*easing)(int))
{
    if (!anim) return -EINVAL;
    anim->easing = easing;
    return 0;
}

int compositor_stop_animation(u32 anim_id)
{
    for (u32 i = 0; i < g_comp.anim_count; i++) {
        if (g_comp.animations[i].anim_id == anim_id) {
            g_comp.animations[i].running = false;
            g_comp.active_animations--;
            return 0;
        }
    }
    return -ENOENT;
}

int compositor_stop_all_animations(void)
{
    for (u32 i = 0; i < g_comp.anim_count; i++) {
        if (g_comp.animations[i].running) {
            g_comp.animations[i].running = false;
        }
    }
    g_comp.active_animations = 0;
    return 0;
}

static void update_animations(void)
{
    u32 active = 0;

    for (u32 i = 0; i < g_comp.anim_count; i++) {
        comp_animation_t *anim = &g_comp.animations[i];

        if (!anim->running) continue;

        u32 elapsed = ktime_get_ms() - anim->start_time;

        if (elapsed >= anim->duration_ms) {
            anim->progress = anim->reverse ? 0 : 256;
            anim->running = false;
            g_comp.active_animations--;
        } else {
            /* Calculate progress as 0-256 range */
            u32 t = (elapsed * 256) / anim->duration_ms;
            if (anim->easing) {
                anim->progress = anim->easing(t);
            } else {
                anim->progress = t;
            }

            if (anim->reverse) {
                anim->progress = 256 - anim->progress;
            }
            active++;
        }
    }

    g_comp.active_animations = active;
}

/*===========================================================================*/
/*                         WINDOW ANIMATIONS                                 */
/*===========================================================================*/

int compositor_animate_open(u32 window_id, u32 type)
{
    comp_animation_t *anim;
    
    switch (type) {
        case ANIM_FADE:
            anim = compositor_create_animation(ANIM_FADE, window_id, 200);
            if (anim) {
                anim->start_value = 0;
                anim->end_value = 255;  /* Opacity */
                anim->easing = ease_out_cubic;
            }
            break;
            
        case ANIM_ZOOM:
            anim = compositor_create_animation(ANIM_ZOOM, window_id, 250);
            if (anim) {
                anim->start_value = 0.8f;  /* Scale */
                anim->end_value = 1.0f;
                anim->easing = ease_out_cubic;
            }
            break;
            
        case ANIM_GEL:
            anim = compositor_create_animation(ANIM_GEL, window_id, 400);
            if (anim) {
                anim->easing = ease_out_elastic;
            }
            break;
            
        default:
            anim = compositor_create_animation(type, window_id, 200);
            break;
    }
    
    return anim ? 0 : -ENOMEM;
}

int compositor_animate_close(u32 window_id, u32 type)
{
    comp_animation_t *anim;
    
    switch (type) {
        case ANIM_FADE:
            anim = compositor_create_animation(ANIM_FADE, window_id, 150);
            if (anim) {
                anim->start_value = 255;
                anim->end_value = 0;
                anim->easing = ease_in_quad;
            }
            break;
            
        case ANIM_ZOOM:
            anim = compositor_create_animation(ANIM_ZOOM, window_id, 200);
            if (anim) {
                anim->start_value = 1.0f;
                anim->end_value = 0.8f;
                anim->easing = ease_in_quad;
                anim->reverse = true;
            }
            break;
            
        default:
            anim = compositor_create_animation(type, window_id, 150);
            break;
    }
    
    return anim ? 0 : -ENOMEM;
}

int compositor_animate_minimize(u32 window_id, rect_t *target_rect)
{
    (void)target_rect;
    
    /* Scale down animation */
    comp_animation_t *anim = compositor_create_animation(ANIM_SCALE, window_id, 200);
    if (anim) {
        anim->start_value = 1.0f;
        anim->end_value = 0.1f;
        anim->easing = ease_in_out_cubic;
    }
    
    return anim ? 0 : -ENOMEM;
}

int compositor_animate_maximize(u32 window_id)
{
    comp_animation_t *anim = compositor_create_animation(ANIM_SCALE, window_id, 250);
    if (anim) {
        anim->start_value = 0.9f;
        anim->end_value = 1.0f;
        anim->easing = ease_out_cubic;
    }
    
    return anim ? 0 : -ENOMEM;
}

/*===========================================================================*/
/*                         COMPOSITING                                       */
/*===========================================================================*/

static void render_shadow(comp_window_decor_t *decor, void *surface)
{
    if (!decor->has_shadow) return;
    
    /* In real implementation, would render blurred shadow */
    /* For now, just a placeholder */
    (void)decor; (void)surface;
}

static void render_border(comp_window_decor_t *decor, void *surface)
{
    if (!decor->has_border) return;
    
    /* In real implementation, would render border */
    (void)decor; (void)surface;
}

int compositor_composite(void *output_surface)
{
    (void)output_surface;  /* Use parameter */
    if (!g_comp.initialized || !g_comp.compositing_enabled) {
        return -EINVAL;
    }

    spinlock_lock(&g_comp.lock);

    /* Update animations */
    if (g_comp.animations_enabled) {
        update_animations();
    }

    /* Clear output */
    /* In real implementation, would clear surface */

    /* Render all windows with decorations */
    for (u32 i = 0; i < g_comp.decor_count; i++) {
        comp_window_decor_t *decor = &g_comp.decorations[i];
        
        /* Apply animation transform */
        u32 scale = 256;  /* 1.0 = 256 */
        u32 opacity = decor->opacity;

        for (u32 j = 0; j < g_comp.anim_count; j++) {
            comp_animation_t *anim = &g_comp.animations[j];
            if (anim->running && anim->target_id == decor->window_id) {
                if (anim->type == ANIM_SCALE || anim->type == ANIM_ZOOM) {
                    /* Integer interpolation: progress is 0-256 */
                    scale = (anim->start_value * (256 - anim->progress) +
                             anim->end_value * anim->progress) / 256;
                } else if (anim->type == ANIM_FADE) {
                    opacity = (u32)((anim->start_value * (256 - anim->progress) +
                             anim->end_value * anim->progress) / 256);
                }
            }
        }
        
        /* Render shadow */
        render_shadow(decor, g_comp.output_surface);
        
        /* Render window content */
        /* In real implementation, would composite window texture */
        
        /* Render border */
        render_border(decor, g_comp.output_surface);
    }
    
    /* Update frame timing */
    u64 now = ktime_get_us();
    if (g_comp.last_frame_time > 0) {
        u64 delta = now - g_comp.last_frame_time;
        if (delta > 0) {
            g_comp.fps = 1000000 / delta;
        }
    }
    g_comp.last_frame_time = now;
    g_comp.frame_count++;
    
    spinlock_unlock(&g_comp.lock);
    
    return 0;
}

int compositor_wait_vsync(compositor_t *comp)
{
    (void)comp;  /* Use parameter */
    if (!g_comp.vsync_enabled) return 0;

    /* In real implementation, would wait for VBlank */
    /* Mock: just return */
    return 0;
}

/*===========================================================================*/
/*                         SETTINGS                                          */
/*===========================================================================*/

int compositor_enable(bool enable)
{
    g_comp.compositing_enabled = enable;
    printk("[COMP] Compositing %s\n", enable ? "enabled" : "disabled");
    return 0;
}

int compositor_enable_hw_accel(bool enable)
{
    g_comp.hw_accel = enable;
    printk("[COMP] Hardware acceleration %s\n", enable ? "enabled" : "disabled");
    return 0;
}

int compositor_enable_vsync(bool enable)
{
    g_comp.vsync_enabled = enable;
    printk("[COMP] VSync %s\n", enable ? "enabled" : "disabled");
    return 0;
}

int compositor_enable_animations(bool enable)
{
    g_comp.animations_enabled = enable;
    printk("[COMP] Animations %s\n", enable ? "enabled" : "disabled");
    return 0;
}

int compositor_set_animation_speed(u32 speed)
{
    if (speed < 50) speed = 50;
    if (speed > 200) speed = 200;
    g_comp.animation_speed = speed;
    printk("[COMP] Animation speed: %d%%\n", speed);
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

u32 compositor_get_fps(void)
{
    return (u32)g_comp.fps;
}

u32 compositor_get_active_animations(void)
{
    return g_comp.active_animations;
}

compositor_state_t *compositor_get_state(void)
{
    return &g_comp;
}
