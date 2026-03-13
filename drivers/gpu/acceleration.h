/*
 * NEXUS OS - Acceleration Drivers
 * drivers/gpu/acceleration.h
 *
 * Acceleration driver support for OpenGL, Vulkan, DRM, KMS,
 * 2D and 3D acceleration
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _ACCELERATION_H
#define _ACCELERATION_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ACCELERATION CONFIGURATION                        */
/*===========================================================================*/

#define ACCEL_MAX_CONTEXTS          256
#define ACCEL_MAX_BUFFERS           1024
#define ACCEL_MAX_TEXTURES          4096
#define ACCEL_MAX_SHADERS           512
#define ACCEL_MAX_PROGRAMS          256
#define ACCEL_MAX_FBO               256
#define ACCEL_MAX_VAO               512
#define ACCEL_MAX_VBO               1024

/*===========================================================================*/
/*                         ACCELERATION API TYPES                            */
/*===========================================================================*/

#define ACCEL_API_NONE              0
#define ACCEL_API_OPENGL            1
#define ACCEL_API_OPENGL_ES         2
#define ACCEL_API_VULKAN            3
#define ACCEL_API_DIRECTX           4
#define ACCEL_API_METAL             5
#define ACCEL_API_OPENCL            6
#define ACCEL_API_CUDA              7

/*===========================================================================*/
/*                         OpenGL CONSTANTS                                  */
/*===========================================================================*/

/* OpenGL Error Codes */
#define GL_NO_ERROR                   0
#define GL_INVALID_ENUM               0x0500
#define GL_INVALID_VALUE              0x0501
#define GL_INVALID_OPERATION          0x0502
#define GL_OUT_OF_MEMORY              0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506

/* OpenGL Data Types */
#define GL_BYTE                       0x1400
#define GL_UNSIGNED_BYTE              0x1401
#define GL_SHORT                      0x1402
#define GL_UNSIGNED_SHORT             0x1403
#define GL_INT                        0x1404
#define GL_UNSIGNED_INT               0x1405
#define GL_FLOAT                      0x1406
#define GL_DOUBLE                     0x140A

/* OpenGL Primitive Types */
#define GL_POINTS                     0x0000
#define GL_LINES                      0x0001
#define GL_LINE_LOOP                  0x0002
#define GL_LINE_STRIP                 0x0003
#define GL_TRIANGLES                  0x0004
#define GL_TRIANGLE_STRIP             0x0005
#define GL_TRIANGLE_FAN               0x0006
#define GL_QUADS                      0x0007

/* OpenGL Buffer Usage */
#define GL_STREAM_DRAW                0x88E0
#define GL_STATIC_DRAW                0x88E4
#define GL_DYNAMIC_DRAW               0x88E8

/* OpenGL Texture Targets */
#define GL_TEXTURE_2D                 0x0DE1
#define GL_TEXTURE_3D                 0x806F
#define GL_TEXTURE_CUBE_MAP           0x8513
#define GL_TEXTURE_2D_ARRAY           0x8C1A

/* OpenGL Shader Types */
#define GL_VERTEX_SHADER              0x8B31
#define GL_FRAGMENT_SHADER            0x8B30
#define GL_GEOMETRY_SHADER            0x8DD9
#define GL_TESS_CONTROL_SHADER        0x8E88
#define GL_TESS_EVALUATION_SHADER     0x8E87
#define GL_COMPUTE_SHADER             0x91B9

/*===========================================================================*/
/*                         Vulkan CONSTANTS                                  */
/*===========================================================================*/

#define VK_MAX_PHYSICAL_GPUS          16
#define VK_MAX_QUEUE_FAMILIES         32
#define VK_MAX_EXTENSIONS             256
#define VK_MAX_LAYERS                 64

/*===========================================================================*/
/*                         DRM/KMS CONSTANTS                                 */
/*===========================================================================*/

#define DRM_CAP_DUMB_BUFFER           0x1
#define DRM_CAP_VBLANK_HIGH_CRTC      0x2
#define DRM_CAP_PRIME                 0x3
#define DRM_CAP_TIMESTAMP_MONOTONIC   0x4
#define DRM_CAP_ASYNC_PAGE_FLIP       0x5
#define DRM_CAP_CURSOR_WIDTH          0x6
#define DRM_CAP_CURSOR_HEIGHT         0x7
#define DRM_CAP_ADDFB2_MODIFIERS      0x10

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * OpenGL Context Info
 */
typedef struct {
    u32 context_id;                 /* Context ID */
    u32 api_version;                /* API Version (major << 16 | minor) */
    u32 profile;                    /* Profile (Core, Compatibility, ES) */
    u32 flags;                      /* Flags (Debug, Forward Compatible) */
    u32 framebuffer_width;          /* Framebuffer Width */
    u32 framebuffer_height;         /* Framebuffer Height */
    u32 samples;                    /* MSAA Samples */
    bool is_current;                /* Is Current Context */
    bool is_valid;                  /* Is Valid */
    void *driver_data;              /* Driver Private Data */
} gl_context_t;

/**
 * OpenGL Buffer Object
 */
typedef struct {
    u32 buffer_id;                  /* Buffer ID */
    u32 target;                     /* Buffer Target */
    u32 size;                       /* Buffer Size */
    u32 usage;                      /* Buffer Usage */
    void *data;                     /* Data Pointer */
    u64 gpu_address;                /* GPU Address */
    bool is_mapped;                 /* Is Mapped */
    bool is_valid;                  /* Is Valid */
} gl_buffer_t;

/**
 * OpenGL Shader
 */
typedef struct {
    u32 shader_id;                  /* Shader ID */
    u32 type;                       /* Shader Type */
    char source[131072];            /* Shader Source */
    u32 compiled;                   /* Is Compiled */
    u32 log_length;                 /* Info Log Length */
    char info_log[4096];            /* Info Log */
    bool is_valid;                  /* Is Valid */
} gl_shader_t;

/**
 * OpenGL Program
 */
typedef struct {
    u32 program_id;                 /* Program ID */
    u32 attached_shaders[16];       /* Attached Shaders */
    u32 shader_count;               /* Attached Shader Count */
    u32 linked;                     /* Is Linked */
    u32 validated;                  /* Is Validated */
    u32 log_length;                 /* Info Log Length */
    char info_log[4096];            /* Info Log */
    u32 uniform_count;              /* Uniform Count */
    u32 attrib_count;               /* Attribute Count */
    bool is_valid;                  /* Is Valid */
} gl_program_t;

/**
 * OpenGL Texture
 */
typedef struct {
    u32 texture_id;                 /* Texture ID */
    u32 target;                     /* Texture Target */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 depth;                      /* Depth */
    u32 mip_levels;                 /* Mipmap Levels */
    u32 samples;                    /* MSAA Samples */
    u32 internal_format;            /* Internal Format */
    u32 format;                     /* Format */
    u32 type;                       /* Type */
    u32 wrap_s;                     /* Wrap S */
    u32 wrap_t;                     /* Wrap T */
    u32 wrap_r;                     /* Wrap R */
    u32 min_filter;                 /* Min Filter */
    u32 mag_filter;                 /* Mag Filter */
    bool is_valid;                  /* Is Valid */
} gl_texture_t;

/**
 * Vulkan Instance Info
 */
typedef struct {
    u32 instance_id;                /* Instance ID */
    u32 api_version;                /* Vulkan API Version */
    char engine_name[256];          /* Engine Name */
    char app_name[256];             /* Application Name */
    u32 enabled_layers;             /* Enabled Layer Count */
    char *enabled_layer_names;      /* Enabled Layer Names */
    u32 enabled_extensions;         /* Enabled Extension Count */
    char *enabled_extension_names;  /* Enabled Extension Names */
    bool is_valid;                  /* Is Valid */
} vk_instance_t;

/**
 * Vulkan Physical Device
 */
typedef struct {
    u32 device_id;                  /* Device ID */
    u32 vendor_id;                  /* Vendor ID */
    u32 pci_device;                 /* PCI Device ID */
    char device_name[256];          /* Device Name */
    u32 api_version;                /* API Version */
    u32 driver_version;             /* Driver Version */
    u64 memory_size;                /* Memory Size */
    u32 queue_family_count;         /* Queue Family Count */
    u32 extension_count;            /* Extension Count */
    u32 layer_count;                /* Layer Count */
    bool is_discrete;               /* Is Discrete GPU */
    bool is_valid;                  /* Is Valid */
} vk_physical_device_t;

/**
 * DRM Device Info
 */
typedef struct {
    int fd;                         /* DRM File Descriptor */
    char path[64];                  /* Device Path */
    char driver_name[64];           /* Driver Name */
    char driver_desc[128];          /* Driver Description */
    u32 vendor_id;                  /* Vendor ID */
    u32 device_id;                  /* Device ID */
    u32 revision;                   /* Revision */
    u32 available_vram;             /* Available VRAM */
    u32 total_vram;                 /* Total VRAM */
    u32 capabilities;               /* Capabilities */
    bool has_vsync;                 /* Has VSync */
    bool has_prime;                 /* Has PRIME */
    bool has_async_flip;            /* Has Async Page Flip */
    bool is_valid;                  /* Is Valid */
} drm_device_t;

/**
 * 2D Acceleration Engine
 */
typedef struct {
    u32 engine_id;                  /* Engine ID */
    char name[64];                  /* Engine Name */
    u32 capabilities;               /* Capabilities */
    u32 max_texture_size;           /* Max Texture Size */
    u32 max_viewport_width;         /* Max Viewport Width */
    u32 max_viewport_height;        /* Max Viewport Height */
    bool is_initialized;            /* Is Initialized */
    void (*fill_rect)(void *dst, u32 x, u32 y, u32 w, u32 h, u32 color);
    void (*copy_rect)(void *dst, u32 dx, u32 dy, void *src, u32 sx, u32 sy, u32 w, u32 h);
    void (*blend_rect)(void *dst, u32 x, u32 y, u32 w, u32 h, u32 color, u32 alpha);
    void (*draw_line)(void *dst, u32 x0, u32 y0, u32 x1, u32 y1, u32 color);
    void (*blit)(void *dst, u32 dx, u32 dy, void *src, u32 sx, u32 sy, u32 w, u32 h);
    void (*scale)(void *dst, u32 dw, u32 dh, void *src, u32 sw, u32 sh);
    void (*rotate)(void *dst, u32 w, u32 h, void *src, u32 angle);
} accel_2d_engine_t;

/**
 * 3D Acceleration Engine
 */
typedef struct {
    u32 engine_id;                  /* Engine ID */
    char name[64];                  /* Engine Name */
    u32 api_support;                /* API Support */
    u32 max_texture_size;           /* Max Texture Size */
    u32 max_texture_units;          /* Max Texture Units */
    u32 max_varying_vectors;        /* Max Varying Vectors */
    u32 max_vertex_attribs;         /* Max Vertex Attributes */
    u32 max_vertex_uniforms;        /* Max Vertex Uniforms */
    u32 max_fragment_uniforms;      /* Max Fragment Uniforms */
    u32 max_draw_buffers;           /* Max Draw Buffers */
    u32 max_samples;                /* Max MSAA Samples */
    bool is_initialized;            /* Is Initialized */
    void (*begin_scene)(void);
    void (*end_scene)(void);
    void (*set_viewport)(u32 x, u32 y, u32 w, u32 h);
    void (*set_projection)(float *matrix);
    void (*set_modelview)(float *matrix);
} accel_3d_engine_t;

/**
 * Acceleration Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    u32 active_api;                 /* Active API */
    gl_context_t gl_contexts[ACCEL_MAX_CONTEXTS]; /* OpenGL Contexts */
    u32 gl_context_count;           /* OpenGL Context Count */
    u32 current_gl_context;         /* Current OpenGL Context */
    gl_buffer_t gl_buffers[ACCEL_MAX_BUFFERS]; /* OpenGL Buffers */
    u32 gl_buffer_count;            /* OpenGL Buffer Count */
    gl_texture_t gl_textures[ACCEL_MAX_TEXTURES]; /* OpenGL Textures */
    u32 gl_texture_count;           /* OpenGL Texture Count */
    gl_shader_t gl_shaders[ACCEL_MAX_SHADERS]; /* OpenGL Shaders */
    u32 gl_shader_count;            /* OpenGL Shader Count */
    gl_program_t gl_programs[ACCEL_MAX_PROGRAMS]; /* OpenGL Programs */
    u32 gl_program_count;           /* OpenGL Program Count */
    vk_instance_t vk_instances[16]; /* Vulkan Instances */
    u32 vk_instance_count;          /* Vulkan Instance Count */
    vk_physical_device_t vk_devices[VK_MAX_PHYSICAL_GPUS]; /* Vulkan Devices */
    u32 vk_device_count;            /* Vulkan Device Count */
    drm_device_t drm_device;        /* DRM Device */
    accel_2d_engine_t accel_2d;     /* 2D Acceleration Engine */
    accel_3d_engine_t accel_3d;     /* 3D Acceleration Engine */
    bool vsync_enabled;             /* VSync Enabled */
    bool hw_accel_enabled;          /* Hardware Acceleration Enabled */
} accel_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int accel_init(void);
int accel_shutdown(void);
bool accel_is_initialized(void);

/* API management */
int accel_set_active_api(u32 api);
u32 accel_get_active_api(void);
int accel_init_api(u32 api);

/* OpenGL functions */
int gl_init(void);
gl_context_t *gl_create_context(u32 width, u32 height, u32 version, u32 flags);
int gl_destroy_context(u32 context_id);
int gl_make_current(u32 context_id);
int gl_swap_buffers(void);
u32 gl_create_buffer(u32 target, u32 size, const void *data, u32 usage);
int gl_delete_buffer(u32 buffer_id);
u32 gl_create_texture(u32 target, u32 width, u32 height, u32 format);
int gl_delete_texture(u32 texture_id);
u32 gl_create_shader(u32 type, const char *source);
int gl_delete_shader(u32 shader_id);
u32 gl_create_program(void);
int gl_delete_program(u32 program_id);
int gl_link_program(u32 program_id);
int gl_use_program(u32 program_id);
void gl_clear(u32 mask);
void gl_draw_arrays(u32 mode, s32 first, s32 count);
void gl_draw_elements(u32 mode, s32 count, u32 type, const void *indices);
void gl_viewport(s32 x, s32 y, s32 w, s32 h);
void gl_enable(u32 cap);
void gl_disable(u32 cap);
int gl_get_error(void);

/* Vulkan functions */
int vk_init(void);
vk_instance_t *vk_create_instance(const char *app_name, const char *engine_name);
int vk_destroy_instance(u32 instance_id);
int vk_enumerate_physical_devices(vk_physical_device_t *devices, u32 *count);
vk_physical_device_t *vk_get_physical_device(u32 device_id);

/* DRM functions */
int drm_open(const char *path);
int drm_close(void);
int drm_get_device_info(drm_device_t *info);
int drm_get_capabilities(u32 *caps);
int drm_set_master(void);
int drm_drop_master(void);
int drm_wait_vblank(u32 crtc_id);
int drm_page_flip(u32 crtc_id, u32 fb_id);

/* 2D Acceleration */
int accel_2d_init(void);
bool accel_2d_is_available(void);
int accel_2d_fill_rect(void *dst, u32 x, u32 y, u32 w, u32 h, u32 color);
int accel_2d_copy_rect(void *dst, u32 dx, u32 dy, void *src, u32 sx, u32 sy, u32 w, u32 h);
int accel_2d_blend_rect(void *dst, u32 x, u32 y, u32 w, u32 h, u32 color, u32 alpha);
int accel_2d_draw_line(void *dst, u32 x0, u32 y0, u32 x1, u32 y1, u32 color);
int accel_2d_blit(void *dst, u32 dx, u32 dy, void *src, u32 sx, u32 sy, u32 w, u32 h);
int accel_2d_scale(void *dst, u32 dw, u32 dh, void *src, u32 sw, u32 sh);
int accel_2d_rotate(void *dst, u32 w, u32 h, void *src, u32 angle);

/* 3D Acceleration */
int accel_3d_init(void);
bool accel_3d_is_available(void);
int accel_3d_begin_scene(void);
int accel_3d_end_scene(void);
int accel_3d_set_viewport(u32 x, u32 y, u32 w, u32 h);
int accel_3d_set_projection(float *matrix);
int accel_3d_set_modelview(float *matrix);

/* VSync */
int accel_enable_vsync(bool enable);
bool accel_is_vsync_enabled(void);
int accel_wait_vsync(void);

/* Utility functions */
const char *accel_get_api_name(u32 api);
u32 accel_get_gl_version(void);
u32 accel_get_vk_version(void);
const char *accel_get_error_string(u32 error);

/* Global instance */
accel_driver_t *accel_get_driver(void);

#endif /* _ACCELERATION_H */
