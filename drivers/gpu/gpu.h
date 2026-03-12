/*
 * NEXUS OS - GPU Graphics Driver
 * drivers/gpu/gpu.h
 *
 * GPU driver with OpenGL/Vulkan support
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _GPU_H
#define _GPU_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         GPU CONFIGURATION                                 */
/*===========================================================================*/

#define GPU_MAX_ADAPTERS            8
#define GPU_MAX_MODES               128
#define GPU_MAX_SURFACES            256
#define GPU_MAX_BUFFERS             64
#define GPU_MAX_TEXTURES            1024
#define GPU_MAX_SHADERS             256

/*===========================================================================*/
/*                         GPU VENDOR IDS                                    */
/*===========================================================================*/

#define GPU_VENDOR_UNKNOWN          0
#define GPU_VENDOR_NVIDIA           0x10DE
#define GPU_VENDOR_AMD              0x1002
#define GPU_VENDOR_INTEL            0x8086
#define GPU_VENDOR_ARM              0x13B5
#define GPU_VENDOR_QUALCOMM         0x5143
#define GPU_VENDOR_BROADCOM         0x14E4
#define GPU_VENDOR_VIA              0x1106

/*===========================================================================*/
/*                         GPU API TYPES                                     */
/*===========================================================================*/

#define GPU_API_NONE                0
#define GPU_API_OPENGL              1
#define GPU_API_OPENGL_ES           2
#define GPU_API_VULKAN              3
#define GPU_API_DIRECTX             4
#define GPU_API_METAL               5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * GPU Display Mode
 */
typedef struct {
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 refresh_rate;               /* Refresh Rate (Hz) */
    u32 bpp;                        /* Bits Per Pixel */
    u32 pixel_format;               /* Pixel Format */
    bool interlaced;                /* Interlaced */
} gpu_mode_t;

/**
 * GPU Adapter Info
 */
typedef struct {
    u32 adapter_id;                 /* Adapter ID */
    u32 vendor_id;                  /* Vendor ID */
    u32 device_id;                  /* Device ID */
    u32 revision;                   /* Revision */
    char name[128];                 /* Adapter Name */
    char vendor_name[64];           /* Vendor Name */
    u32 api_support;                /* API Support */
    u64 vram_size;                  /* VRAM Size */
    u64 vram_used;                  /* VRAM Used */
    u32 max_texture_size;           /* Max Texture Size */
    u32 max_viewport;               /* Max Viewport */
    u32 max_render_targets;         /* Max Render Targets */
    bool is_integrated;             /* Is Integrated */
    bool is_active;                 /* Is Active */
    gpu_mode_t *modes;              /* Display Modes */
    u32 mode_count;                 /* Mode Count */
} gpu_adapter_t;

/**
 * GPU Buffer
 */
typedef struct {
    u32 buffer_id;                  /* Buffer ID */
    u32 size;                       /* Size */
    u32 usage;                      /* Usage Flags */
    void *data;                     /* Data */
    u64 gpu_address;                /* GPU Address */
    bool is_mapped;                 /* Is Mapped */
} gpu_buffer_t;

/**
 * GPU Texture
 */
typedef struct {
    u32 texture_id;                 /* Texture ID */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 depth;                      /* Depth */
    u32 mip_levels;                 /* Mip Levels */
    u32 format;                     /* Format */
    u32 usage;                      /* Usage Flags */
    u64 gpu_address;                /* GPU Address */
    bool is_mapped;                 /* Is Mapped */
} gpu_texture_t;

/**
 * GPU Shader
 */
typedef struct {
    u32 shader_id;                  /* Shader ID */
    u32 type;                       /* Shader Type */
    char source[65536];             /* Shader Source */
    u32 compiled;                   /* Is Compiled */
    char error_log[1024];           /* Error Log */
} gpu_shader_t;

/**
 * GPU Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    gpu_adapter_t adapters[GPU_MAX_ADAPTERS]; /* Adapters */
    u32 adapter_count;              /* Adapter Count */
    u32 active_adapter;             /* Active Adapter */
    gpu_buffer_t buffers[GPU_MAX_BUFFERS]; /* Buffers */
    u32 buffer_count;               /* Buffer Count */
    gpu_texture_t textures[GPU_MAX_TEXTURES]; /* Textures */
    u32 texture_count;              /* Texture Count */
    gpu_shader_t shaders[GPU_MAX_SHADERS]; /* Shaders */
    u32 shader_count;               /* Shader Count */
} gpu_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int gpu_init(void);
int gpu_shutdown(void);
bool gpu_is_initialized(void);

/* Adapter management */
int gpu_enumerate_adapters(void);
gpu_adapter_t *gpu_get_adapter(u32 adapter_id);
gpu_adapter_t *gpu_get_active_adapter(void);
int gpu_set_active_adapter(u32 adapter_id);
int gpu_get_adapter_modes(u32 adapter_id, gpu_mode_t *modes, u32 *count);
int gpu_set_display_mode(u32 adapter_id, gpu_mode_t *mode);

/* Buffer management */
gpu_buffer_t *gpu_create_buffer(u32 size, u32 usage);
int gpu_destroy_buffer(u32 buffer_id);
int gpu_map_buffer(u32 buffer_id, void **data);
int gpu_unmap_buffer(u32 buffer_id);
int gpu_upload_buffer(u32 buffer_id, const void *data, u32 size);
int gpu_download_buffer(u32 buffer_id, void *data, u32 size);

/* Texture management */
gpu_texture_t *gpu_create_texture(u32 width, u32 height, u32 format, u32 usage);
int gpu_destroy_texture(u32 texture_id);
int gpu_upload_texture(u32 texture_id, const void *data, u32 size);
int gpu_download_texture(u32 texture_id, void *data, u32 size);
int gpu_generate_mipmaps(u32 texture_id);

/* Shader management */
gpu_shader_t *gpu_create_shader(u32 type, const char *source);
int gpu_destroy_shader(u32 shader_id);
int gpu_compile_shader(u32 shader_id);
int gpu_get_shader_log(u32 shader_id, char *log, u32 size);

/* Rendering */
int gpu_clear(u32 buffer_id, float r, float g, float b, float a);
int gpu_draw_arrays(u32 buffer_id, u32 first, u32 count);
int gpu_draw_elements(u32 buffer_id, u32 count, u32 index_type);
int gpu_present(u32 buffer_id);

/* Utility functions */
const char *gpu_get_vendor_name(u32 vendor_id);
const char *gpu_get_api_name(u32 api);
u32 gpu_get_format_size(u32 format);

/* Global instance */
gpu_driver_t *gpu_get_driver(void);

#endif /* _GPU_H */
