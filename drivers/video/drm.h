/*
 * NEXUS OS - DRM/KMS Driver
 * drivers/video/drm.h
 *
 * Direct Rendering Manager / Kernel Mode Setting driver
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _DRM_H
#define _DRM_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         DRM CONFIGURATION                                 */
/*===========================================================================*/

#define DRM_MAX_CRTCS               8
#define DRM_MAX_CONNECTORS          16
#define DRM_MAX_ENCODERS            16
#define DRM_MAX_PLANES              32
#define DRM_MAX_BUFFERS             64
#define DRM_MAX_FORMATS             32

/*===========================================================================*/
/*                         DRM CONSTANTS                                     */
/*===========================================================================*/

#define DRM_CONNECTOR_UNKNOWN       0
#define DRM_CONNECTOR_VGA           1
#define DRM_CONNECTOR_DVI_I         2
#define DRM_CONNECTOR_DVI_D         3
#define DRM_CONNECTOR_DP            4
#define DRM_CONNECTOR_HDMI          5
#define DRM_CONNECTOR_LVDS          6
#define DRM_CONNECTOR_eDP           7

#define DRM_ENCODER_NONE            0
#define DRM_ENCODER_DAC             1
#define DRM_ENCODER_TMDS            2
#define DRM_ENCODER_LVDS            3
#define DRM_ENCODER_DSI             4

#define DRM_MODE_TYPE_BUILTIN       0x1
#define DRM_MODE_TYPE_CLOCK_C       0x2
#define DRM_MODE_TYPE_CRTC_C        0x4
#define DRM_MODE_TYPE_PREFERRED     0x8
#define DRM_MODE_TYPE_DEFAULT       0x10
#define DRM_MODE_TYPE_USERDEF       0x20
#define DRM_MODE_TYPE_DRIVER        0x40

#define DRM_MODE_FLAG_PHSYNC        0x1
#define DRM_MODE_FLAG_NHSYNC        0x2
#define DRM_MODE_FLAG_PVSYNC        0x4
#define DRM_MODE_FLAG_NVSYNC        0x8
#define DRM_MODE_FLAG_INTERLACE     0x10
#define DRM_MODE_FLAG_DBLSCAN       0x20
#define DRM_MODE_FLAG_CSYNC         0x40
#define DRM_MODE_FLAG_PCSYNC        0x80
#define DRM_MODE_FLAG_NCSYNC        0x100

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * DRM Mode Info
 */
typedef struct {
    u32 clock;                      /* Clock (kHz) */
    u16 hdisplay;                   /* Horizontal Display */
    u16 hsync_start;                /* Horizontal Sync Start */
    u16 hsync_end;                  /* Horizontal Sync End */
    u16 htotal;                     /* Horizontal Total */
    u16 hskew;                      /* Horizontal Skew */
    u16 vdisplay;                   /* Vertical Display */
    u16 vsync_start;                /* Vertical Sync Start */
    u16 vsync_end;                  /* Vertical Sync End */
    u16 vtotal;                     /* Vertical Total */
    u16 vscan;                      /* Vertical Scan */
    u32 vrefresh;                   /* Vertical Refresh (Hz) */
    u32 flags;                      /* Flags */
    u32 type;                       /* Type */
    char name[32];                  /* Mode Name */
} drm_mode_info_t;

/**
 * DRM Connector Info
 */
typedef struct {
    u32 connector_id;               /* Connector ID */
    u32 type;                       /* Connector Type */
    char type_name[32];             /* Type Name */
    u32 encoder_id;                 /* Encoder ID */
    u32 connection;                 /* Connection Status */
    u32 mm_width;                   /* Width (mm) */
    u32 mm_height;                  /* Height (mm) */
    u32 mode_count;                 /* Mode Count */
    drm_mode_info_t *modes;         /* Modes */
    bool is_connected;              /* Is Connected */
    bool is_enabled;                /* Is Enabled */
} drm_connector_info_t;

/**
 * DRM Encoder Info
 */
typedef struct {
    u32 encoder_id;                 /* Encoder ID */
    u32 type;                       /* Encoder Type */
    u32 crtc_id;                    /* CRTC ID */
    u32 possible_crtcs;             /* Possible CRTCs */
    u32 possible_clones;            /* Possible Clones */
} drm_encoder_info_t;

/**
 * DRM CRTC Info
 */
typedef struct {
    u32 crtc_id;                    /* CRTC ID */
    bool is_active;                 /* Is Active */
    u32 x;                          /* X Position */
    u32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 mode_valid;                 /* Mode Valid */
    drm_mode_info_t mode;           /* Current Mode */
} drm_crtc_info_t;

/**
 * DRM Plane Info
 */
typedef struct {
    u32 plane_id;                   /* Plane ID */
    u32 type;                       /* Plane Type */
    u32 crtc_id;                    /* CRTC ID */
    u32 fb_id;                      /* Framebuffer ID */
    u32 crtc_x;                     /* CRTC X */
    u32 crtc_y;                     /* CRTC Y */
    u32 crtc_w;                     /* CRTC Width */
    u32 crtc_h;                     /* CRTC Height */
    u32 src_x;                      /* Source X */
    u32 src_y;                      /* Source Y */
    u32 src_w;                      /* Source Width */
    u32 src_h;                      /* Source Height */
    u32 format_count;               /* Format Count */
    u32 *formats;                   /* Formats */
} drm_plane_info_t;

/**
 * DRM Framebuffer Info
 */
typedef struct {
    u32 fb_id;                      /* Framebuffer ID */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 stride;                     /* Stride */
    u32 handle;                     /* GEM Handle */
    u32 offset;                     /* Offset */
    u32 format;                     /* Format */
    u32 bpp;                        /* Bits Per Pixel */
    void *map;                      /* Mapped Address */
} drm_framebuffer_t;

/**
 * DRM Device Info
 */
typedef struct {
    u32 device_id;                  /* Device ID */
    u32 vendor_id;                  /* Vendor ID */
    u32 chipset;                    /* Chipset */
    u32 revision;                   /* Revision */
    char driver_name[64];           /* Driver Name */
    char driver_desc[128];          /* Driver Description */
    char driver_date[32];           /* Driver Date */
    u32 available_vram;             /* Available VRAM */
    u32 total_vram;                 /* Total VRAM */
    u32 available_gtt;              /* Available GTT */
    u32 total_gtt;                  /* Total GTT */
    bool has_accel;                 /* Has Acceleration */
    bool has_vsync;                 /* Has VSync */
    bool has_prime;                 /* Has PRIME */
} drm_device_info_t;

/**
 * DRM Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    int fd;                         /* DRM File Descriptor */
    drm_device_info_t device;       /* Device Info */
    drm_connector_info_t connectors[DRM_MAX_CONNECTORS]; /* Connectors */
    u32 connector_count;            /* Connector Count */
    drm_encoder_info_t encoders[DRM_MAX_ENCODERS]; /* Encoders */
    u32 encoder_count;              /* Encoder Count */
    drm_crtc_info_t crtcs[DRM_MAX_CRTCS]; /* CRTCs */
    u32 crtc_count;                 /* CRTC Count */
    drm_plane_info_t planes[DRM_MAX_PLANES]; /* Planes */
    u32 plane_count;                /* Plane Count */
    drm_framebuffer_t framebuffers[DRM_MAX_BUFFERS]; /* Framebuffers */
    u32 framebuffer_count;          /* Framebuffer Count */
    u32 active_crtc;                /* Active CRTC */
    u32 active_connector;           /* Active Connector */
    bool mode_set;                  /* Mode Set */
    bool cursor_visible;            /* Cursor Visible */
    u32 cursor_x;                   /* Cursor X */
    u32 cursor_y;                   /* Cursor Y */
    void *cursor_buffer;            /* Cursor Buffer */
} drm_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int drm_init(void);
int drm_shutdown(void);
bool drm_is_initialized(void);

/* Device management */
int drm_open_device(const char *path);
int drm_close_device(void);
int drm_get_device_info(drm_device_info_t *info);
int drm_list_devices(drm_device_info_t *devices, u32 *count);

/* Connector management */
int drm_get_connectors(drm_connector_info_t *connectors, u32 *count);
int drm_get_connector_info(u32 connector_id, drm_connector_info_t *info);
int drm_set_connector_property(u32 connector_id, const char *name, u64 value);

/* Mode setting */
int drm_get_modes(u32 connector_id, drm_mode_info_t *modes, u32 *count);
int drm_set_mode(u32 crtc_id, u32 connector_id, drm_mode_info_t *mode);
int drm_get_current_mode(u32 crtc_id, drm_mode_info_t *mode);
int drm_restore_mode(void);

/* Framebuffer management */
int drm_create_framebuffer(u32 width, u32 height, u32 format, drm_framebuffer_t *fb);
int drm_destroy_framebuffer(u32 fb_id);
int drm_set_framebuffer(u32 crtc_id, u32 fb_id);
int drm_get_framebuffer(u32 fb_id, drm_framebuffer_t *fb);
int drm_map_framebuffer(drm_framebuffer_t *fb);
int drm_unmap_framebuffer(drm_framebuffer_t *fb);

/* CRTC management */
int drm_get_crtcs(drm_crtc_info_t *crtcs, u32 *count);
int drm_get_crtc_info(u32 crtc_id, drm_crtc_info_t *info);
int drm_set_crtc(u32 crtc_id, u32 fb_id, drm_mode_info_t *mode);

/* Plane management */
int drm_get_planes(drm_plane_info_t *planes, u32 *count);
int drm_get_plane_info(u32 plane_id, drm_plane_info_t *info);
int drm_set_plane(u32 plane_id, u32 crtc_id, u32 fb_id, drm_mode_info_t *mode);

/* Cursor management */
int drm_show_cursor(bool show);
int drm_move_cursor(u32 x, u32 y);
int drm_set_cursor_image(void *image, u32 width, u32 height);

/* Buffer management */
int drm_create_gem_buffer(u32 size, u32 *handle);
int drm_destroy_gem_buffer(u32 handle);
int drm_map_gem_buffer(u32 handle, void **map);
int drm_unmap_gem_buffer(u32 handle);

/* Page flipping */
int drm_page_flip(u32 crtc_id, u32 fb_id);
int drm_wait_vblank(u32 crtc_id);
int drm_enable_vsync(bool enable);

/* Acceleration */
int drm_enable_acceleration(bool enable);
bool drm_has_acceleration(void);
int drm_get_accel_caps(u32 *caps);

/* Utility functions */
const char *drm_get_connector_type_name(u32 type);
const char *drm_get_encoder_type_name(u32 type);
const char *drm_get_format_name(u32 format);
u32 drm_format_bpp(u32 format);

/* Global instance */
drm_driver_t *drm_get_driver(void);

#endif /* _DRM_H */
