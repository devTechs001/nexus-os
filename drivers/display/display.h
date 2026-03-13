/*
 * NEXUS OS - Display Drivers
 * drivers/display/display.h
 *
 * Comprehensive display driver support for VGA, SVGA, VESA, Framebuffer,
 * DisplayPort, HDMI, and touchscreen
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         DISPLAY CONFIGURATION                             */
/*===========================================================================*/

#define DISPLAY_MAX_ADAPTERS        16
#define DISPLAY_MAX_MODES           256
#define DISPLAY_MAX_CONNECTORS      32
#define DISPLAY_MAX_ENCODERS        32
#define DISPLAY_MAX_CRTCS           16
#define DISPLAY_MAX_PLANES          64
#define DISPLAY_MAX_FRAMEBUFFERS    128

/*===========================================================================*/
/*                         DISPLAY TYPES                                     */
/*===========================================================================*/

#define DISPLAY_TYPE_UNKNOWN        0
#define DISPLAY_TYPE_VGA            1
#define DISPLAY_TYPE_SVGA           2
#define DISPLAY_TYPE_VESA           3
#define DISPLAY_TYPE_FRAMEBUFFER    4
#define DISPLAY_TYPE_DISPLAYPORT    5
#define DISPLAY_TYPE_HDMI           6
#define DISPLAY_TYPE_DVI            7
#define DISPLAY_TYPE_LVDS           8
#define DISPLAY_TYPE_eDP            9
#define DISPLAY_TYPE_TOUCHSCREEN    10

/*===========================================================================*/
/*                         CONNECTOR TYPES                                   */
/*===========================================================================*/

#define CONNECTOR_TYPE_VGA          0
#define CONNECTOR_TYPE_DVI_I        1
#define CONNECTOR_TYPE_DVI_D        2
#define CONNECTOR_TYPE_DP           3
#define CONNECTOR_TYPE_HDMI_A       4
#define CONNECTOR_TYPE_HDMI_B       5
#define CONNECTOR_TYPE_eDP          6
#define CONNECTOR_TYPE_LVDS         7
#define CONNECTOR_TYPE_DSI          8
#define CONNECTOR_TYPE_USB_C        9

/*===========================================================================*/
/*                         PIXEL FORMATS                                     */
/*===========================================================================*/

#define PIXEL_FORMAT_UNKNOWN        0
#define PIXEL_FORMAT_RGB565         1
#define PIXEL_FORMAT_RGB888         2
#define PIXEL_FORMAT_BGR888         3
#define PIXEL_FORMAT_ARGB8888       4
#define PIXEL_FORMAT_ABGR8888       5
#define PIXEL_FORMAT_XRGB8888       6
#define PIXEL_FORMAT_XBGR8888       7
#define PIXEL_FORMAT_RGBX8888       8
#define PIXEL_FORMAT_BGRX8888       9
#define PIXEL_FORMAT_ARGB2101010    10
#define PIXEL_FORMAT_ABGR2101010    11
#define PIXEL_FORMAT_RGB8           12
#define PIXEL_FORMAT_GRAY8          13

/*===========================================================================*/
/*                         DISPLAY CAPABILITIES                              */
/*===========================================================================*/

#define DISPLAY_CAP_NONE            0x0000
#define DISPLAY_CAP_VSYNC           0x0001
#define DISPLAY_CAP_CURSOR          0x0002
#define DISPLAY_CAP_OVERLAY         0x0004
#define DISPLAY_CAP_SCALING         0x0008
#define DISPLAY_CAP_ROTATION        0x00010
#define DISPLAY_CAP_HDR             0x00020
#define DISPLAY_CAP_ADAPTIVE_SYNC   0x00040
#define DISPLAY_CAP_MULTIMONITOR    0x00080
#define DISPLAY_CAP_HIDPI           0x00100
#define DISPLAY_CAP_TOUCH           0x00200

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Display Mode Information
 */
typedef struct {
    u32 id;                         /* Mode ID */
    u32 clock;                      /* Pixel Clock (kHz) */
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
    u32 width_mm;                   /* Width (mm) */
    u32 height_mm;                  /* Height (mm) */
    char name[64];                  /* Mode Name */
    bool is_preferred;              /* Is Preferred Mode */
    bool is_interlaced;             /* Is Interlaced */
    bool is_doublescan;             /* Is Doublescan */
} display_mode_t;

/**
 * Display Connector Info
 */
typedef struct {
    u32 connector_id;               /* Connector ID */
    u32 type;                       /* Connector Type */
    char type_name[32];             /* Type Name */
    u32 encoder_id;                 /* Encoder ID */
    u32 status;                     /* Connection Status */
    u32 mm_width;                   /* Width (mm) */
    u32 mm_height;                  /* Height (mm) */
    u32 mode_count;                 /* Mode Count */
    display_mode_t *modes;          /* Supported Modes */
    bool is_connected;              /* Is Connected */
    bool is_enabled;                /* Is Enabled */
    bool is_dpms;                   /* DPMS Support */
    u32 dpms_state;                 /* DPMS State */
    u32 subpixel;                   /* Subpixel Order */
    u32 link_status;                /* Link Status */
    u32 max_bpc;                    /* Max Bits Per Component */
    u32 scaling_mode;               /* Scaling Mode */
    u32 aspect_ratio;               /* Aspect Ratio */
} display_connector_t;

/**
 * Display Encoder Info
 */
typedef struct {
    u32 encoder_id;                 /* Encoder ID */
    u32 type;                       /* Encoder Type */
    u32 crtc_id;                    /* CRTC ID */
    u32 possible_crtcs;             /* Possible CRTCs */
    u32 possible_clones;            /* Possible Clones */
} display_encoder_t;

/**
 * Display CRTC (Cathode Ray Tube Controller) Info
 */
typedef struct {
    u32 crtc_id;                    /* CRTC ID */
    bool is_active;                 /* Is Active */
    u32 x;                          /* X Position */
    u32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 mode_valid;                 /* Mode Valid */
    display_mode_t mode;            /* Current Mode */
    u32 gamma_size;                 /* Gamma Table Size */
    u32 *gamma_r;                   /* Gamma Red Table */
    u32 *gamma_g;                   /* Gamma Green Table */
    u32 *gamma_b;                   /* Gamma Blue Table */
} display_crtc_t;

/**
 * Display Plane Info
 */
typedef struct {
    u32 plane_id;                   /* Plane ID */
    u32 type;                       /* Plane Type (Primary, Cursor, Overlay) */
    u32 crtc_id;                    /* CRTC ID */
    u32 fb_id;                      /* Framebuffer ID */
    u32 crtc_x;                     /* CRTC X */
    u32 crtc_y;                     /* CRTC Y */
    u32 crtc_w;                     /* CRTC Width */
    u32 crtc_h;                     /* CRTC Height */
    u32 src_x;                      /* Source X (16.16 fixed) */
    u32 src_y;                      /* Source Y (16.16 fixed) */
    u32 src_w;                      /* Source Width (16.16 fixed) */
    u32 src_h;                      /* Source Height (16.16 fixed) */
    u32 format_count;               /* Supported Format Count */
    u32 *formats;                   /* Supported Formats */
    u32 rotation;                   /* Rotation */
    u32 alpha;                      /* Alpha */
    u32 zpos;                       /* Z Position */
} display_plane_t;

/**
 * Framebuffer Info
 */
typedef struct {
    u32 fb_id;                      /* Framebuffer ID */
    void *base;                     /* Base Address */
    phys_addr_t phys_base;          /* Physical Base Address */
    u32 size;                       /* Size */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 stride;                     /* Stride (bytes per line) */
    u32 bpp;                        /* Bits Per Pixel */
    u32 format;                     /* Pixel Format */
    bool is_linear;                 /* Is Linear */
    bool is_mapped;                 /* Is Mapped */
    u32 offset;                     /* Offset */
    u32 handle;                     /* GEM Handle */
} display_framebuffer_t;

/**
 * Display Adapter Info
 */
typedef struct {
    u32 adapter_id;                 /* Adapter ID */
    u32 type;                       /* Display Type */
    char name[128];                 /* Adapter Name */
    char vendor[64];                /* Vendor Name */
    u32 vendor_id;                  /* Vendor ID */
    u32 device_id;                  /* Device ID */
    u32 revision;                   /* Revision */
    u32 capabilities;               /* Capabilities */
    u64 vram_size;                  /* VRAM Size */
    u64 vram_used;                  /* VRAM Used */
    u64 gtt_size;                   /* GTT Size */
    u32 connector_count;            /* Connector Count */
    u32 encoder_count;              /* Encoder Count */
    u32 crtc_count;                 /* CRTC Count */
    u32 plane_count;                /* Plane Count */
    display_connector_t *connectors; /* Connectors */
    display_encoder_t *encoders;    /* Encoders */
    display_crtc_t *crtcs;          /* CRTCs */
    display_plane_t *planes;        /* Planes */
    bool is_active;                 /* Is Active */
    bool is_primary;                /* Is Primary */
    bool is_boot_device;            /* Is Boot Device */
    u32 cursor_width;               /* Cursor Width */
    u32 cursor_height;              /* Cursor Height */
    u32 cursor_x;                   /* Cursor X */
    u32 cursor_y;                   /* Cursor Y */
    bool cursor_visible;            /* Cursor Visible */
    void *cursor_buffer;            /* Cursor Buffer */
} display_adapter_t;

/**
 * Display Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    display_adapter_t adapters[DISPLAY_MAX_ADAPTERS]; /* Adapters */
    u32 adapter_count;              /* Adapter Count */
    u32 primary_adapter;            /* Primary Adapter */
    display_framebuffer_t framebuffers[DISPLAY_MAX_FRAMEBUFFERS]; /* Framebuffers */
    u32 framebuffer_count;          /* Framebuffer Count */
    u32 active_mode;                /* Active Mode */
    u32 vsync_enabled;              /* VSync Enabled */
    u32 gamma_r[256];               /* Gamma Red Table */
    u32 gamma_g[256];               /* Gamma Green Table */
    u32 gamma_b[256];               /* Gamma Blue Table */
    u32 brightness;                 /* Brightness (0-100) */
    u32 contrast;                   /* Contrast (0-100) */
    u32 saturation;                 /* Saturation (0-100) */
    bool dpms_enabled;              /* DPMS Enabled */
    u32 dpms_state;                 /* DPMS State */
    u32 power_state;                /* Power State */
    void (*set_pixel)(u32 x, u32 y, u32 color);
    u32 (*get_pixel)(u32 x, u32 y);
    void (*fill_rect)(u32 x, u32 y, u32 w, u32 h, u32 color);
    void (*blit)(void *src, u32 x, u32 y, u32 w, u32 h);
} display_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int display_init(void);
int display_shutdown(void);
bool display_is_initialized(void);

/* Adapter management */
int display_enumerate_adapters(void);
display_adapter_t *display_get_adapter(u32 adapter_id);
display_adapter_t *display_get_primary_adapter(void);
display_adapter_t *display_get_active_adapter(void);
int display_set_active_adapter(u32 adapter_id);
int display_list_adapters(display_adapter_t *adapters, u32 *count);

/* Connector management */
int display_get_connectors(u32 adapter_id, display_connector_t *connectors, u32 *count);
int display_get_connector_status(u32 connector_id);
int display_set_connector_property(u32 connector_id, const char *name, u64 value);
int display_detect_connectors(u32 adapter_id);

/* Mode setting */
int display_get_modes(u32 connector_id, display_mode_t *modes, u32 *count);
int display_set_mode(u32 crtc_id, u32 connector_id, display_mode_t *mode);
int display_get_current_mode(u32 crtc_id, display_mode_t *mode);
int display_restore_mode(void);
int display_get_preferred_mode(u32 connector_id, display_mode_t *mode);
int display_get_best_mode(u32 connector_id, u32 width, u32 height, display_mode_t *mode);

/* Framebuffer management */
int display_create_framebuffer(u32 width, u32 height, u32 format, display_framebuffer_t *fb);
int display_destroy_framebuffer(u32 fb_id);
int display_set_framebuffer(u32 crtc_id, u32 fb_id);
int display_get_framebuffer(u32 fb_id, display_framebuffer_t *fb);
int display_map_framebuffer(u32 fb_id, void **map);
int display_unmap_framebuffer(u32 fb_id);
display_framebuffer_t *display_get_primary_framebuffer(void);

/* CRTC management */
int display_get_crtcs(u32 adapter_id, display_crtc_t *crtcs, u32 *count);
int display_get_crtc_info(u32 crtc_id, display_crtc_t *info);
int display_set_crtc(u32 crtc_id, u32 fb_id, display_mode_t *mode);
int display_enable_crtc(u32 crtc_id);
int display_disable_crtc(u32 crtc_id);
int display_set_crtc_gamma(u32 crtc_id, u32 *r, u32 *g, u32 *b, u32 size);

/* Plane management */
int display_get_planes(u32 adapter_id, display_plane_t *planes, u32 *count);
int display_get_plane_info(u32 plane_id, display_plane_t *info);
int display_set_plane(u32 plane_id, u32 crtc_id, u32 fb_id, display_mode_t *mode);
int display_enable_plane(u32 plane_id);
int display_disable_plane(u32 plane_id);

/* Cursor management */
int display_show_cursor(bool show);
int display_move_cursor(u32 x, u32 y);
int display_set_cursor_image(void *image, u32 width, u32 height);
int display_get_cursor_position(u32 *x, u32 *y);
int display_set_cursor_visible(bool visible);

/* Drawing primitives */
int display_set_pixel(u32 x, u32 y, u32 color);
u32 display_get_pixel(u32 x, u32 y);
int display_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
int display_draw_line(u32 x0, u32 y0, u32 x1, u32 y1, u32 color);
int display_draw_circle(u32 cx, u32 cy, u32 r, u32 color);
int display_draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
int display_blit(void *src, u32 x, u32 y, u32 w, u32 h);
int display_scroll(s32 dx, s32 dy);

/* VSync and page flipping */
int display_enable_vsync(bool enable);
bool display_is_vsync_enabled(void);
int display_wait_vsync(void);
int display_wait_vblank(u32 crtc_id);
int display_page_flip(u32 crtc_id, u32 fb_id);

/* Power management */
int display_set_power_state(u32 state);
int display_get_power_state(void);
int display_set_dpms(u32 state);
int display_get_dpms(void);
int display_set_brightness(u32 brightness);
int display_set_contrast(u32 contrast);
int display_set_saturation(u32 saturation);

/* Color management */
int display_set_gamma(u32 *r, u32 *g, u32 *b, u32 size);
int display_get_gamma(u32 *r, u32 *g, u32 *b, u32 size);
int display_set_color_calibration(u32 profile_id);
int display_get_color_info(u32 *brightness, u32 *contrast, u32 *saturation);

/* Multi-monitor support */
int display_get_monitor_count(void);
int display_get_monitor_info(u32 index, display_connector_t *info);
int display_set_monitor_position(u32 index, s32 x, s32 y);
int display_set_monitor_primary(u32 index);
int display_clone_monitor(u32 source, u32 target);
int display_extend_monitor(u32 source, u32 target, u32 position);

/* HiDPI support */
int display_set_scale_factor(u32 adapter_id, u32 scale_x, u32 scale_y);
int display_get_scale_factor(u32 adapter_id, u32 *scale_x, u32 *scale_y);
int display_get_dpi(u32 adapter_id, u32 *dpi_x, u32 *dpi_y);

/* Utility functions */
const char *display_get_type_name(u32 type);
const char *display_get_connector_type_name(u32 type);
const char *display_get_pixel_format_name(u32 format);
u32 display_get_format_bpp(u32 format);
u32 display_get_format_size(u32 format);
u32 display_pack_color(u8 r, u8 g, u8 b, u32 format);
void display_unpack_color(u32 color, u8 *r, u8 *g, u8 *b, u32 format);

/* Global instance */
display_driver_t *display_get_driver(void);

#endif /* _DISPLAY_H */
