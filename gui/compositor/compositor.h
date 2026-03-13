/*
 * NEXUS OS - Hardware-accelerated Compositor
 * gui/compositor/compositor.h
 *
 * Modern GPU-accelerated compositor with advanced effects
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _COMPOSITOR_H
#define _COMPOSITOR_H

#include "../gui.h"
#include "../window-manager/window-manager.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         COMPOSITOR CONFIGURATION                          */
/*===========================================================================*/

#define COMPOSITOR_VERSION          "1.0.0"
#define COMPOSITOR_MAX_LAYERS       32
#define COMPOSITOR_MAX_SURFACES     256
#define COMPOSITOR_VSYNC            1
#define COMPOSITOR_TRIPLE_BUFFER    1
#define COMPOSITOR_MAX_DIRTY_RECTS  128

/*===========================================================================*/
/*                         RENDERING BACKENDS                                */
/*===========================================================================*/

#define RENDER_BACKEND_SOFTWARE      0
#define RENDER_BACKEND_OPENGL        1
#define RENDER_BACKEND_OPENGL_ES     2
#define RENDER_BACKEND_VULKAN        3
#define RENDER_BACKEND_DIRECTFB      4
#define RENDER_BACKEND_DRM           5

/*===========================================================================*/
/*                         COMPOSITING FLAGS                                 */
/*===========================================================================*/

#define COMP_FLAG_VSYNC             (1 << 0)
#define COMP_FLAG_TRIPLE_BUFFER     (1 << 1)
#define COMP_FLAG_SHADOWS           (1 << 2)
#define COMP_FLAG_ANIMATIONS        (1 << 3)
#define COMP_FLAG_BLUR              (1 << 4)
#define COMP_FLAG_HDR               (1 << 5)
#define COMP_FLAG_WIDE_GAMUT        (1 << 6)

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * GPU Surface
 */
typedef struct {
    u32 surface_id;                 /* Surface ID */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 format;                     /* Pixel Format */
    u32 stride;                     /* Stride */
    void *data;                     /* Surface Data */
    phys_addr_t phys_addr;          /* Physical Address */
    u32 gpu_handle;                 /* GPU Handle */
    bool is_gpu;                    /* Is GPU Surface */
    bool is_locked;                 /* Is Locked */
} gpu_surface_t;

/**
 * Compositor Layer
 */
typedef struct {
    u32 layer_id;                   /* Layer ID */
    u32 z_order;                    /* Z-Order */
    gpu_surface_t *surface;         /* Surface */
    rect_t src_rect;                /* Source Rectangle */
    rect_t dst_rect;                /* Destination Rectangle */
    u32 alpha;                      /* Alpha (0-255) */
    u32 flags;                      /* Layer Flags */
    bool visible;                   /* Is Visible */
    bool enabled;                   /* Is Enabled */
    matrix4_t transform;            /* Transform Matrix */
    color_t color_key;              /* Color Key */
    bool use_color_key;             /* Use Color Key */
} compositor_layer_t;

/**
 * Damage Region
 */
typedef struct {
    rect_t rects[COMPOSITOR_MAX_DIRTY_RECTS];
    u32 count;
    rect_t bounding_box;
} damage_region_t;

/**
 * Framebuffer
 */
typedef struct {
    u32 fb_id;                      /* Framebuffer ID */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 format;                     /* Pixel Format */
    u32 stride;                     /* Stride */
    void *data;                     /* Framebuffer Data */
    phys_addr_t phys_addr;          /* Physical Address */
    gpu_surface_t *surface;         /* GPU Surface */
    bool is_flipped;                /* Is Front Buffer */
} framebuffer_t;

/**
 * Render Statistics
 */
typedef struct {
    u64 frame_count;                /* Frame Count */
    u64 last_frame_time;            /* Last Frame Time */
    u32 fps;                        /* Current FPS */
    u32 avg_fps;                    /* Average FPS */
    u32 render_time_us;             /* Render Time (μs) */
    u32 composite_time_us;          /* Composite Time (μs) */
    u32 gpu_time_us;                /* GPU Time (μs) */
    u32 vsync_misses;               /* VSync Misses */
    u32 frames_dropped;             /* Frames Dropped */
    u32 surfaces_rendered;          /* Surfaces Rendered */
    u32 layers_composited;          /* Layers Composited */
    u64 total_render_time;          /* Total Render Time */
} render_stats_t;

/**
 * Compositor State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool running;                   /* Is Running */
    bool active;                    /* Is Active */
    
    /* Rendering */
    u32 render_backend;             /* Render Backend */
    u32 flags;                      /* Compositor Flags */
    u32 pixel_format;               /* Pixel Format */
    u32 color_depth;                /* Color Depth */
    
    /* Display */
    u32 display_width;              /* Display Width */
    u32 display_height;             /* Display Height */
    u32 refresh_rate;               /* Refresh Rate */
    bool vsync_enabled;             /* VSync Enabled */
    
    /* Framebuffers */
    framebuffer_t framebuffers[3];  /* Triple Buffer */
    u32 front_buffer;               /* Front Buffer Index */
    u32 back_buffer;                /* Back Buffer Index */
    u32 render_buffer;              /* Render Buffer Index */
    
    /* Layers */
    compositor_layer_t layers[COMPOSITOR_MAX_LAYERS];
    u32 layer_count;
    u32 next_layer_id;
    
    /* Surfaces */
    gpu_surface_t *surfaces;        /* Surface Pool */
    u32 surface_count;
    u32 max_surfaces;
    
    /* Damage Tracking */
    damage_region_t damage;         /* Current Damage */
    damage_region_t frame_damage;   /* Frame Damage */
    bool full_damage;               /* Full Damage */
    
    /* Window Manager Integration */
    window_manager_t *wm;           /* Window Manager */
    
    /* Statistics */
    render_stats_t stats;           /* Render Statistics */
    
    /* GPU Context */
    void *gl_context;               /* OpenGL Context */
    void *vk_device;                /* Vulkan Device */
    void *vk_queue;                 /* Vulkan Queue */
    u32 gpu_vendor;                 /* GPU Vendor */
    char gpu_name[128];             /* GPU Name */
    
    /* Shaders */
    u32 shader_program;             /* Shader Program */
    u32 vertex_shader;              /* Vertex Shader */
    u32 fragment_shader;            /* Fragment Shader */
    u32 blur_shader;                /* Blur Shader */
    u32 shadow_shader;              /* Shadow Shader */
    
    /* Textures */
    u32 default_texture;            /* Default Texture */
    u32 cursor_texture;             /* Cursor Texture */
    
    /* Buffers */
    u32 vao;                        /* Vertex Array Object */
    u32 vbo;                        /* Vertex Buffer Object */
    u32 ebo;                        /* Element Buffer Object */
    
    /* Cursor */
    s32 cursor_x;                   /* Cursor X */
    s32 cursor_y;                   /* Cursor Y */
    bool cursor_visible;            /* Cursor Visible */
    gpu_surface_t *cursor_surface;  /* Cursor Surface */
    
    /* Effects */
    bool effects_enabled;           /* Effects Enabled */
    u32 blur_radius;                /* Blur Radius */
    float brightness;               /* Brightness */
    float contrast;                 /* Contrast */
    float saturation;               /* Saturation */
    
    /* Callbacks */
    int (*on_frame_render)(struct compositor *);
    int (*on_vsync)(struct compositor *);
    int (*on_surface_created)(gpu_surface_t *);
    int (*on_surface_destroyed)(gpu_surface_t *);
    
    /* User Data */
    void *user_data;
    
} compositor_t;

/**
 * Vertex Structure (for GPU rendering)
 */
typedef struct {
    float x, y, z;                  /* Position */
    float u, v;                     /* Texture Coordinates */
    u8 r, g, b, a;                  /* Color */
} vertex_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Compositor lifecycle - internal API (public API in gui.h) */
/* compositor_init is declared in gui.h as int compositor_init(void) */
int compositor_run(compositor_t *comp);
/* compositor_shutdown is declared in gui.h as void compositor_shutdown(void) */
bool compositor_is_running(compositor_t *comp);

/* Surface management */
gpu_surface_t *compositor_create_surface(compositor_t *comp, u32 width, u32 height, u32 format);
int compositor_destroy_surface(compositor_t *comp, gpu_surface_t *surface);
int compositor_resize_surface(compositor_t *comp, gpu_surface_t *surface, u32 width, u32 height);
int compositor_lock_surface(compositor_t *comp, gpu_surface_t *surface);
int compositor_unlock_surface(compositor_t *comp, gpu_surface_t *surface);
int compositor_upload_surface(compositor_t *comp, gpu_surface_t *surface, const void *data);
int compositor_download_surface(compositor_t *comp, gpu_surface_t *surface, void *data);

/* Layer management */
int compositor_create_layer(compositor_t *comp, gpu_surface_t *surface, u32 *layer_id);
int compositor_destroy_layer(compositor_t *comp, u32 layer_id);
int compositor_set_layer_position(compositor_t *comp, u32 layer_id, s32 x, s32 y);
int compositor_set_layer_size(compositor_t *comp, u32 layer_id, u32 width, u32 height);
int compositor_set_layer_z_order(compositor_t *comp, u32 layer_id, u32 z_order);
int compositor_set_layer_alpha(compositor_t *comp, u32 layer_id, u32 alpha);
int compositor_set_layer_visible(compositor_t *comp, u32 layer_id, bool visible);
int compositor_set_layer_transform(compositor_t *comp, u32 layer_id, matrix4_t *transform);

/* Rendering */
int compositor_render(compositor_t *comp);
int compositor_render_frame(compositor_t *comp);
int compositor_composite_layers(compositor_t *comp);
int compositor_present(compositor_t *comp);
int compositor_wait_vsync(compositor_t *comp);

/* Damage tracking */
int compositor_add_damage(compositor_t *comp, rect_t *rect);
int compositor_clear_damage(compositor_t *comp);
damage_region_t *compositor_get_damage(compositor_t *comp);
bool compositor_is_damaged(compositor_t *comp);

/* Display management */
int compositor_set_display_mode(compositor_t *comp, u32 width, u32 height, u32 refresh);
int compositor_get_display_modes(compositor_t *comp, void *modes, u32 *count);
int compositor_set_gamma(compositor_t *comp, float r, float g, float b);
int compositor_set_brightness(compositor_t *comp, float brightness);

/* Effects */
int compositor_enable_effects(compositor_t *comp, bool enabled);
int compositor_set_blur(compositor_t *comp, u32 radius);
int compositor_set_color_correction(compositor_t *comp, float brightness, float contrast, float saturation);

/* Cursor */
int compositor_set_cursor(compositor_t *comp, gpu_surface_t *surface);
int compositor_set_cursor_position(compositor_t *comp, s32 x, s32 y);
int compositor_show_cursor(compositor_t *comp, bool show);

/* Statistics */
render_stats_t *compositor_get_stats(compositor_t *comp);
int compositor_reset_stats(compositor_t *comp);
const char *compositor_get_backend_name(u32 backend);

/* Utility functions */
u32 compositor_get_pixel_format(const char *name);
const char *compositor_get_pixel_format_name(u32 format);
u32 compositor_get_bpp(u32 format);
u32 compositor_get_stride(u32 width, u32 format);

/* Global instance */
compositor_t *compositor_get_instance(void);

#endif /* _COMPOSITOR_H */
