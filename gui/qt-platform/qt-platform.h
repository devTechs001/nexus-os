/*
 * NEXUS OS - Qt Platform Plugin
 * gui/qt-platform/qt-platform.h
 *
 * Qt platform abstraction plugin for NEXUS OS
 * Supports LinuxFB, EGLFS, and Wayland
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _QT_PLATFORM_H
#define _QT_PLATFORM_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         QT PLATFORM CONFIGURATION                         */
/*===========================================================================*/

#define QT_PLATFORM_NAME            "nexus"
#define QT_PLATFORM_VERSION_MAJOR   1
#define QT_PLATFORM_VERSION_MINOR   0
#define QT_MAX_PLATFORMS            8
#define QT_MAX_WINDOWS              256
#define QT_MAX_SURFACES             512

/*===========================================================================*/
/*                         QT PLATFORM TYPES                                 */
/*===========================================================================*/

#define QT_PLATFORM_UNKNOWN         0
#define QT_PLATFORM_LINUXFB         1   /* Linux Framebuffer */
#define QT_PLATFORM_EGLFS           2   /* EGL Full Screen */
#define QT_PLATFORM_WAYLAND         3   /* Wayland */
#define QT_PLATFORM_XCB             4   /* X11 via XCB */
#define QT_PLATFORM_OFFSCREEN       5   /* Offscreen */
#define QT_PLATFORM_VNC             6   /* VNC */
#define QT_PLATFORM_NEXUS           7   /* Native NEXUS */

/*===========================================================================*/
/*                         EGL CONFIGURATION                                 */
/*===========================================================================*/

#define EGL_DEFAULT_DISPLAY         NULL
#define EGL_NO_CONTEXT              0
#define EGL_NO_SURFACE              0
#define EGL_NO_DISPLAY              0

/* EGL Error Codes */
#define EGL_SUCCESS                 0x3000
#define EGL_NOT_INITIALIZED         0x3001
#define EGL_BAD_ACCESS              0x3002
#define EGL_BAD_ALLOC               0x3003
#define EGL_BAD_ATTRIBUTE           0x3004
#define EGL_BAD_CONTEXT             0x3005
#define EGL_BAD_CONFIG              0x3006
#define EGL_BAD_CURRENT_SURFACE     0x3007
#define EGL_BAD_DISPLAY             0x3008
#define EGL_BAD_SURFACE             0x3009
#define EGL_BAD_MATCH               0x300A
#define EGL_BAD_PARAMETER           0x300B
#define EGL_BAD_NATIVE_PIXMAP       0x300C
#define EGL_BAD_NATIVE_WINDOW       0x300D
#define EGL_CONTEXT_LOST          0x300E

/* EGL Config Attributes */
#define EGL_BUFFER_SIZE             0x3020
#define EGL_ALPHA_SIZE              0x3021
#define EGL_BLUE_SIZE               0x3022
#define EGL_GREEN_SIZE              0x3023
#define EGL_RED_SIZE                0x3024
#define EGL_DEPTH_SIZE              0x3025
#define EGL_STENCIL_SIZE            0x3026
#define EGL_CONFIG_CAVEAT           0x3027
#define EGL_CONFIG_ID               0x3028
#define EGL_LEVEL                   0x3029
#define EGL_MAX_PBUFFER_HEIGHT      0x302A
#define EGL_MAX_PBUFFER_PIXELS      0x302B
#define EGL_MAX_PBUFFER_WIDTH       0x302C
#define EGL_NATIVE_RENDERABLE       0x302D
#define EGL_NATIVE_VISUAL_ID        0x302E
#define EGL_SAMPLES                 0x3031
#define EGL_SAMPLE_BUFFERS          0x3032
#define EGL_SURFACE_TYPE            0x3033
#define EGL_TRANSPARENT_TYPE        0x3034
#define EGL_TRANSPARENT_BLUE_VALUE  0x3035
#define EGL_TRANSPARENT_GREEN_VALUE 0x3036
#define EGL_TRANSPARENT_RED_VALUE   0x3037
#define EGL_BIND_TO_TEXTURE_RGB     0x3039
#define EGL_BIND_TO_TEXTURE_RGBA    0x303A
#define EGL_MIN_SWAP_INTERVAL       0x303B
#define EGL_MAX_SWAP_INTERVAL       0x303C
#define EGL_LUMINANCE_SIZE          0x303D
#define EGL_ALPHA_MASK_SIZE         0x303E
#define EGL_COLOR_BUFFER_TYPE       0x303F
#define EGL_RENDERABLE_TYPE         0x3040

/* EGL Surface Attributes */
#define EGL_RENDER_BUFFER             0x3086
#define EGL_COLORSPACE                0x3087
#define EGL_ALPHA_FORMAT              0x3088
#define EGL_COLORSPACE_LINEAR         0x308A
#define EGL_ALPHA_FORMAT_NONPRE       0x308B
#define EGL_ALPHA_FORMAT_PRE          0x308C
#define EGL_MIPMAP_TEXTURE            0x3082
#define EGL_MIPMAP_LEVEL              0x3083
#define EGL_BACK_BUFFER               0x3084
#define EGL_SINGLE_BUFFER             0x3085

/* EGL Context Attributes */
#define EGL_CONTEXT_CLIENT_TYPE       0x3097
#define EGL_CONTEXT_CLIENT_VERSION    0x3098

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * EGL Display
 */
typedef struct {
    u32 display_id;                 /* Display ID */
    void *native_display;           /* Native Display */
    bool is_initialized;            /* Is Initialized */
    char *extensions;               /* Extensions String */
    u32 config_count;               /* Config Count */
} egl_display_t;

/**
 * EGL Config
 */
typedef struct {
    u32 config_id;                  /* Config ID */
    u32 buffer_size;                /* Buffer Size */
    u32 alpha_size;                 /* Alpha Size */
    u32 blue_size;                  /* Blue Size */
    u32 green_size;                 /* Green Size */
    u32 red_size;                   /* Red Size */
    u32 depth_size;                 /* Depth Size */
    u32 stencil_size;               /* Stencil Size */
    u32 config_caveat;              /* Config Caveat */
    u32 native_visual_id;           /* Native Visual ID */
    u32 renderable_type;            /* Renderable Type */
    u32 surface_type;               /* Surface Type */
    u32 transparent_type;           /* Transparent Type */
    u32 samples;                    /* Samples */
    u32 sample_buffers;             /* Sample Buffers */
    bool is_valid;                  /* Is Valid */
} egl_config_t;

/**
 * EGL Surface
 */
typedef struct {
    u32 surface_id;                 /* Surface ID */
    egl_display_t *display;         /* Display */
    egl_config_t *config;           /* Config */
    void *native_window;            /* Native Window */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 format;                     /* Format */
    u32 buffer_age;                 /* Buffer Age */
    bool is_valid;                  /* Is Valid */
} egl_surface_t;

/**
 * EGL Context
 */
typedef struct {
    u32 context_id;                 /* Context ID */
    egl_display_t *display;         /* Display */
    egl_config_t *config;           /* Config */
    egl_surface_t *read_surface;    /* Read Surface */
    egl_surface_t *draw_surface;    /* Draw Surface */
    u32 client_type;                /* Client Type */
    u32 client_version;             /* Client Version */
    u32 error;                      /* Last Error */
    bool is_current;                /* Is Current */
    bool is_valid;                  /* Is Valid */
} egl_context_t;

/**
 * Qt Platform Window
 */
typedef struct {
    u32 window_id;                  /* Window ID */
    char title[256];                /* Window Title */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 depth;                      /* Depth */
    u32 format;                     /* Pixel Format */
    u32 flags;                      /* Window Flags */
    void *buffer;                   /* Framebuffer */
    u32 buffer_size;                /* Buffer Size */
    egl_surface_t *egl_surface;     /* EGL Surface */
    bool is_visible;                /* Is Visible */
    bool is_active;                 /* Is Active */
    bool is_minimized;              /* Is Minimized */
    bool is_maximized;              /* Is Maximized */
    bool is_fullscreen;             /* Is Fullscreen */
    bool is_transparent;            /* Is Transparent */
    void *user_data;                /* User Data */
} qt_window_t;

/**
 * Qt Platform Integration
 */
typedef struct {
    u32 platform_id;                /* Platform ID */
    u32 type;                       /* Platform Type */
    char name[64];                  /* Platform Name */
    char version[32];               /* Version String */
    bool is_initialized;            /* Is Initialized */
    egl_display_t *egl_display;     /* EGL Display */
    egl_context_t *egl_context;     /* EGL Context */
    egl_surface_t *egl_surface;     /* EGL Surface */
    void *native_display;           /* Native Display */
    void *native_window;            /* Native Window */
    u32 screen_width;               /* Screen Width */
    u32 screen_height;              /* Screen Height */
    u32 screen_depth;               /* Screen Depth */
    u32 screen_refresh_rate;        /* Refresh Rate (Hz) */
    u32 dpi_x;                      /* DPI X */
    u32 dpi_y;                      /* DPI Y */
    u32 cursor_x;                   /* Cursor X */
    u32 cursor_y;                   /* Cursor Y */
    bool cursor_visible;            /* Cursor Visible */
    void *cursor_data;              /* Cursor Data */
    u32 cursor_width;               /* Cursor Width */
    u32 cursor_height;              /* Cursor Height */
    qt_window_t *windows[QT_MAX_WINDOWS]; /* Windows */
    u32 window_count;               /* Window Count */
    void (*on_window_create)(qt_window_t *);
    void (*on_window_destroy)(qt_window_t *);
    void (*on_window_resize)(qt_window_t *, u32, u32);
    void (*on_window_move)(qt_window_t *, s32, s32);
    void (*on_key_event)(u32, u32, u32);
    void (*on_mouse_event)(u32, s32, s32, u32);
} qt_platform_t;

/**
 * Qt Platform Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    qt_platform_t platforms[QT_MAX_PLATFORMS]; /* Platforms */
    u32 platform_count;             /* Platform Count */
    u32 active_platform;            /* Active Platform */
    qt_window_t windows[QT_MAX_WINDOWS]; /* Windows */
    u32 window_count;               /* Window Count */
    egl_display_t egl_displays[8]; /* EGL Displays */
    u32 egl_display_count;          /* EGL Display Count */
    egl_config_t egl_configs[64];  /* EGL Configs */
    u32 egl_config_count;           /* EGL Config Count */
    egl_surface_t egl_surfaces[QT_MAX_SURFACES]; /* EGL Surfaces */
    u32 egl_surface_count;          /* EGL Surface Count */
    egl_context_t egl_contexts[QT_MAX_WINDOWS]; /* EGL Contexts */
    u32 egl_context_count;          /* EGL Context Count */
    u32 last_error;                 /* Last Error */
} qt_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int qt_platform_init(void);
int qt_platform_shutdown(void);
bool qt_platform_is_initialized(void);

/* Platform management */
int qt_platform_register(u32 type, const char *name);
int qt_platform_unregister(u32 platform_id);
qt_platform_t *qt_platform_get(u32 platform_id);
qt_platform_t *qt_platform_get_active(void);
int qt_platform_set_active(u32 platform_id);
int qt_platform_list(u32 *ids, u32 *count);

/* LinuxFB plugin */
int linuxfb_init(void);
int linuxfb_shutdown(void);
int linuxfb_open(const char *fb_device);
int linuxfb_close(void);
int linuxfb_set_mode(u32 width, u32 height, u32 bpp);
int linuxfb_get_framebuffer(void **base, u32 *size);
int linuxfb_flush(void);
int linuxfb_wait_vsync(void);

/* EGLFS plugin */
int eglfs_init(void);
int eglfs_shutdown(void);
egl_display_t *eglfs_get_display(void);
int eglfs_terminate_display(egl_display_t *display);
egl_config_t *eglfs_choose_config(egl_display_t *display, u32 *attribs);
egl_surface_t *eglfs_create_surface(egl_display_t *display, egl_config_t *config, void *native_window);
egl_context_t *eglfs_create_context(egl_display_t *display, egl_config_t *config, egl_surface_t *surface);
int eglfs_make_current(egl_context_t *context);
int eglfs_swap_buffers(egl_surface_t *surface);
int eglfs_destroy_surface(egl_surface_t *surface);
int eglfs_destroy_context(egl_context_t *context);

/* Window management */
qt_window_t *qt_window_create(u32 width, u32 height, u32 format);
int qt_window_destroy(u32 window_id);
int qt_window_show(u32 window_id);
int qt_window_hide(u32 window_id);
int qt_window_resize(u32 window_id, u32 width, u32 height);
int qt_window_move(u32 window_id, s32 x, s32 y);
int qt_window_set_title(u32 window_id, const char *title);
int qt_window_set_fullscreen(u32 window_id, bool fullscreen);
int qt_window_get_buffer(u32 window_id, void **buffer, u32 *size);
int qt_window_flush(u32 window_id);

/* Input handling */
int qt_platform_process_events(void);
int qt_platform_poll_events(void);
int qt_platform_set_cursor(u32 x, u32 y);
int qt_platform_set_cursor_image(void *image, u32 width, u32 height);

/* EGL functions */
u32 egl_get_error(void);
egl_display_t *egl_get_display(void *native_display);
int egl_initialize(egl_display_t *display, u32 *major, u32 *minor);
int egl_terminate(egl_display_t *display);
const char *egl_query_string(egl_display_t *display, u32 name);
int egl_get_configs(egl_display_t *display, egl_config_t *configs, u32 config_size, u32 *num_config);
int egl_choose_config(egl_display_t *display, u32 *attribs, egl_config_t *configs, u32 config_size, u32 *num_config);
int egl_get_config_attrib(egl_display_t *display, egl_config_t *config, u32 attribute, u32 *value);
egl_surface_t *egl_create_window_surface(egl_display_t *display, egl_config_t *config, void *native_window, u32 *attribs);
egl_context_t *egl_create_context(egl_display_t *display, egl_config_t *config, egl_context_t *share_context, u32 *attribs);
int egl_make_current(egl_display_t *display, egl_surface_t *draw, egl_surface_t *read, egl_context_t *context);
int egl_swap_buffers(egl_display_t *display, egl_surface_t *surface);
int egl_destroy_surface(egl_display_t *display, egl_surface_t *surface);
int egl_destroy_context(egl_display_t *display, egl_context_t *context);

/* Utility functions */
const char *qt_platform_get_type_name(u32 type);
const char *qt_platform_get_error_name(u32 error);
u32 qt_platform_get_pixel_size(u32 format);

/* Global instance */
qt_driver_t *qt_platform_get_driver(void);

#endif /* _QT_PLATFORM_H */
