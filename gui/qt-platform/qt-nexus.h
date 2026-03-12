/*
 * NEXUS OS - Qt Platform Plugin
 * gui/qt-platform/qt-nexus.h
 *
 * Qt platform abstraction plugin for NEXUS OS
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _QT_NEXUS_H
#define _QT_NEXUS_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         QT PLATFORM CONFIGURATION                         */
/*===========================================================================*/

#define QT_NEXUS_VERSION            "1.0.0"
#define QT_NEXUS_PLUGIN_NAME        "nexus"
#define QT_NEXUS_INTEGRATION_NAME   "NexusOS"

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Qt Nexus Platform Settings
 */
typedef struct {
    bool use_wayland;               /* Use Wayland */
    bool use_x11;                   /* Use X11 */
    bool use_framebuffer;           /* Use Framebuffer */
    bool use_eglfs;                 /* Use EGL FullScreen */
    bool use_linuxfb;               /* Use Linux Framebuffer */
    bool enable_hardware_cursor;    /* Enable Hardware Cursor */
    bool enable_vsync;              /* Enable VSync */
    bool enable_double_buffer;      /* Enable Double Buffering */
    bool enable_alpha_channel;      /* Enable Alpha Channel */
    u32 cursor_size;                /* Cursor Size */
    u32 dpi;                        /* DPI */
    u32 font_dpi;                   /* Font DPI */
    const char *font_family;        /* Font Family */
    const char *style_name;         /* Style Name */
    const char *platform_theme;     /* Platform Theme */
    const char *icon_theme;         /* Icon Theme */
} qt_nexus_settings_t;

/**
 * Qt Nexus Window
 */
typedef struct {
    u32 window_id;                  /* Window ID */
    u32 surface_id;                 /* Surface ID */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool is_visible;                /* Is Visible */
    bool is_active;                 /* Is Active */
    bool is_fullscreen;             /* Is Fullscreen */
    bool is_maximized;              /* Is Maximized */
    bool has_alpha;                 /* Has Alpha */
    u32 format;                     /* Surface Format */
    void *native_handle;            /* Native Handle */
} qt_nexus_window_t;

/**
 * Qt Nexus Input Context
 */
typedef struct {
    bool has_focus;                 /* Has Focus */
    u32 cursor_position;            /* Cursor Position */
    u32 anchor_position;            /* Anchor Position */
    char preedit_text[256];         /* Preedit Text */
    u32 preedit_cursor;             /* Preedit Cursor */
    bool is_composing;              /* Is Composing */
    const char *input_method;       /* Input Method */
} qt_nexus_input_context_t;

/**
 * Qt Nexus Platform State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    qt_nexus_settings_t settings;   /* Settings */
    qt_nexus_window_t *windows;     /* Windows */
    u32 window_count;               /* Window Count */
    qt_nexus_input_context_t input_ctx; /* Input Context */
    void *egl_display;              /* EGL Display */
    void *egl_context;              /* EGL Context */
    void *egl_surface;              /* EGL Surface */
    void *gl_context;               /* GL Context */
    u32 screen_width;               /* Screen Width */
    u32 screen_height;              /* Screen Height */
    u32 screen_depth;               /* Screen Depth */
    u32 screen_refresh;             /* Screen Refresh Rate */
    bool has_opengl;                /* Has OpenGL */
    bool has_opengles;              /* Has OpenGL ES */
    bool has_vulkan;                /* Has Vulkan */
    const char *gl_vendor;          /* GL Vendor */
    const char *gl_renderer;        /* GL Renderer */
    const char *gl_version;         /* GL Version */
    const char *glsl_version;       /* GLSL Version */
    void (*on_window_created)(qt_nexus_window_t *);
    void (*on_window_destroyed)(qt_nexus_window_t *);
    void (*on_input_event)(void *event);
    void *user_data;                /* User Data */
} qt_nexus_platform_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Platform initialization */
int qt_nexus_init(void);
int qt_nexus_shutdown(void);
bool qt_nexus_is_initialized(void);

/* Settings */
int qt_nexus_load_settings(qt_nexus_settings_t *settings);
int qt_nexus_save_settings(qt_nexus_settings_t *settings);
int qt_nexus_apply_settings(qt_nexus_settings_t *settings);

/* Window management */
qt_nexus_window_t *qt_nexus_create_window(u32 width, u32 height, u32 format);
int qt_nexus_destroy_window(qt_nexus_window_t *window);
int qt_nexus_show_window(qt_nexus_window_t *window);
int qt_nexus_hide_window(qt_nexus_window_t *window);
int qt_nexus_resize_window(qt_nexus_window_t *window, u32 width, u32 height);
int qt_nexus_move_window(qt_nexus_window_t *window, s32 x, s32 y);
int qt_nexus_set_window_title(qt_nexus_window_t *window, const char *title);
int qt_nexus_set_window_fullscreen(qt_nexus_window_t *window, bool fullscreen);
int qt_nexus_swap_buffers(qt_nexus_window_t *window);

/* Input handling */
qt_nexus_input_context_t *qt_nexus_get_input_context(void);
int qt_nexus_set_input_focus(qt_nexus_window_t *window);
int qt_nexus_commit_preedit(void);
int qt_nexus_reset_input_context(void);

/* OpenGL/EGL */
int qt_nexus_create_gl_context(void);
int qt_nexus_destroy_gl_context(void);
int qt_nexus_make_current(qt_nexus_window_t *window);
int qt_nexus_swap_interval(int interval);
void *qt_nexus_get_proc_address(const char *name);

/* Screen information */
int qt_nexus_get_screen_info(u32 *width, u32 *height, u32 *depth);
int qt_nexus_get_dpi(u32 *dpi);
int qt_nexus_get_refresh_rate(void);

/* Theme integration */
int qt_nexus_load_system_theme(void);
int qt_nexus_load_icon_theme(const char *theme_name);
const char *qt_nexus_get_system_font(void);
const char *qt_nexus_get_style_name(void);

/* Clipboard */
int qt_nexus_set_clipboard_text(const char *text);
char *qt_nexus_get_clipboard_text(void);
bool qt_nexus_has_clipboard_text(void);

/* Drag and drop */
int qt_nexus_start_drag(void *data, const char *mime_type);
int qt_nexus_accept_drop(const char *mime_type);
void *qt_nexus_get_drop_data(void);

/* Utility functions */
const char *qt_nexus_get_version(void);
const char *qt_nexus_get_plugin_name(void);
qt_nexus_platform_t *qt_nexus_get_platform(void);

#endif /* _QT_NEXUS_H */
