/*
 * NEXUS OS - Window System Drivers
 * drivers/video/window.h
 *
 * Window system support for Wayland, X11 bridge, and compositor
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _WINDOW_H
#define _WINDOW_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         WINDOW SYSTEM CONFIGURATION                       */
/*===========================================================================*/

#define WINDOW_MAX_WINDOWS          1024
#define WINDOW_MAX_SURFACES         2048
#define WINDOW_MAX_REGIONS          512
#define WINDOW_MAX_OUTPUTS          16
#define WINDOW_MAX_SEATS            8

/*===========================================================================*/
/*                         WINDOW SYSTEM TYPES                               */
/*===========================================================================*/

#define WINDOW_SYSTEM_NONE          0
#define WINDOW_SYSTEM_WAYLAND       1
#define WINDOW_SYSTEM_X11           2
#define WINDOW_SYSTEM_XWAYLAND      3
#define WINDOW_SYSTEM_DIRECTFB      4
#define WINDOW_SYSTEM_FRAMEBUFFER   5

/*===========================================================================*/
/*                         WAYLAND CONSTANTS                                 */
/*===========================================================================*/

#define WL_DISPLAY_VERSION          1
#define WL_REGISTRY_VERSION         1
#define WL_COMPOSITOR_VERSION       4
#define WL_SHELL_VERSION            1
#define WL_SHM_VERSION              1
#define WL_OUTPUT_VERSION           3
#define WL_SEAT_VERSION             7
#define WL_SURFACE_VERSION          4

/*===========================================================================*/
/*                         X11 CONSTANTS                                     */
/*===========================================================================*/

#define X11_NONE                    0L
#define X11_INPUT_MASK              (1L<<0)
#define X11_OUTPUT_MASK             (1L<<1)
#define X11_PROPERTY_MASK           (1L<<2)
#define X11_STRUCTURE_MASK          (1L<<3)

/*===========================================================================*/
/*                         WINDOW TYPES                                      */
/*===========================================================================*/

#define WINDOW_TYPE_NORMAL          0
#define WINDOW_TYPE_DIALOG          1
#define WINDOW_TYPE_POPUP           2
#define WINDOW_TYPE_MENU            3
#define WINDOW_TYPE_TOOLTIP         4
#define WINDOW_TYPE_NOTIFICATION    5
#define WINDOW_TYPE_SPLASH          6
#define WINDOW_TYPE_DOCK            7
#define WINDOW_TYPE_DESKTOP         8
#define WINDOW_TYPE_FULLSCREEN      9
#define WINDOW_TYPE_MAXIMIZED       10
#define WINDOW_TYPE_MINIMIZED       11

/*===========================================================================*/
/*                         WINDOW STATES                                     */
/*===========================================================================*/

#define WINDOW_STATE_NORMAL         0x0000
#define WINDOW_STATE_ACTIVE         0x0001
#define WINDOW_STATE_FOCUSED        0x0002
#define WINDOW_STATE_HOVERED        0x0003
#define WINDOW_STATE_MAXIMIZED      0x0004
#define WINDOW_STATE_MINIMIZED      0x0008
#define WINDOW_STATE_FULLSCREEN     0x0010
#define WINDOW_STATE_RESIZING       0x0020
#define WINDOW_STATE_MOVING         0x0040
#define WINDOW_STATE_MODAL          0x0080
#define WINDOW_STATE_ABOVE          0x0100
#define WINDOW_STATE_BELOW          0x0200
#define WINDOW_STATE_URGENT         0x0400
#define WINDOW_STATE_HIDDEN         0x0800

/*===========================================================================*/
/*                         WINDOW EDGES                                      */
/*===========================================================================*/

#define WINDOW_EDGE_NONE            0
#define WINDOW_EDGE_TOP             1
#define WINDOW_EDGE_BOTTOM          2
#define WINDOW_EDGE_LEFT            4
#define WINDOW_EDGE_RIGHT           8
#define WINDOW_EDGE_TOP_LEFT        16
#define WINDOW_EDGE_TOP_RIGHT       32
#define WINDOW_EDGE_BOTTOM_LEFT     64
#define WINDOW_EDGE_BOTTOM_RIGHT    128

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Rectangle
 */
typedef struct {
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
} rect_t;

/**
 * Point
 */
typedef struct {
    s32 x;                          /* X Coordinate */
    s32 y;                          /* Y Coordinate */
} point_t;

/**
 * Size
 */
typedef struct {
    u32 width;                      /* Width */
    u32 height;                     /* Height */
} size_t;

/**
 * Color
 */
typedef struct {
    u8 r;                           /* Red */
    u8 g;                           /* Green */
    u8 b;                           /* Blue */
    u8 a;                           /* Alpha */
} color_t;

/**
 * Wayland Output
 */
typedef struct {
    u32 output_id;                  /* Output ID */
    char name[64];                  /* Output Name */
    char make[64];                  /* Make */
    char model[64];                 /* Model */
    char serial[64];                /* Serial */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    s32 mm_width;                   /* Physical Width (mm) */
    s32 mm_height;                  /* Physical Height (mm) */
    s32 subpixel;                   /* Subpixel Order */
    char *modes;                    /* Display Modes */
    u32 mode_count;                 /* Mode Count */
    u32 current_mode;               /* Current Mode */
    s32 scale;                      /* Scale Factor */
    bool is_enabled;                /* Is Enabled */
    bool is_connected;              /* Is Connected */
} wl_output_t;

/**
 * Wayland Seat
 */
typedef struct {
    u32 seat_id;                    /* Seat ID */
    char name[64];                  /* Seat Name */
    u32 capabilities;               /* Capabilities */
    bool has_pointer;               /* Has Pointer */
    bool has_keyboard;              /* Has Keyboard */
    bool has_touch;                 /* Has Touch */
    void *pointer_data;             /* Pointer Data */
    void *keyboard_data;            /* Keyboard Data */
    void *touch_data;               /* Touch Data */
} wl_seat_t;

/**
 * Wayland Surface
 */
typedef struct {
    u32 surface_id;                 /* Surface ID */
    void *buffer;                   /* Buffer */
    u32 buffer_width;               /* Buffer Width */
    u32 buffer_height;              /* Buffer Height */
    rect_t damage;                  /* Damage Region */
    rect_t input_region;            /* Input Region */
    rect_t opaque_region;           /* Opaque Region */
    s32 scale;                      /* Buffer Scale */
    bool is_attached;               /* Is Buffer Attached */
    bool is_visible;                /* Is Visible */
    void *driver_data;              /* Driver Private Data */
} wl_surface_t;

/**
 * Wayland Shell Surface
 */
typedef struct {
    u32 shell_surface_id;           /* Shell Surface ID */
    wl_surface_t *surface;          /* Surface */
    u32 transient_parent;           /* Transient Parent */
    s32 transient_x;                /* Transient X */
    s32 transient_y;                /* Transient Y */
    u32 transient_flags;            /* Transient Flags */
    u32 toplevel_parent;            /* Toplevel Parent */
    char title[256];                /* Title */
    char app_id[256];               /* Application ID */
    u32 state;                      /* State */
    u32 max_width;                  /* Max Width */
    u32 max_height;                 /* Max Height */
    u32 min_width;                  /* Min Width */
    u32 min_height;                 /* Min Height */
} wl_shell_surface_t;

/**
 * X11 Window
 */
typedef struct {
    u32 window_id;                  /* Window ID */
    u32 parent_id;                  /* Parent Window ID */
    u32 *children;                  /* Children */
    u32 child_count;                /* Child Count */
    rect_t geometry;                /* Geometry */
    rect_t border;                  /* Border */
    u32 border_width;               /* Border Width */
    u32 depth;                      /* Depth */
    u32 visual;                     /* Visual */
    u32 colormap;                   /* Colormap */
    u32 background;                 /* Background */
    u32 event_mask;                 /* Event Mask */
    u32 attributes;                 /* Attributes */
    char *wm_name;                  /* WM Name */
    char *wm_class;                 /* WM Class */
    bool is_mapped;                 /* Is Mapped */
    bool is_viewable;               /* Is Viewable */
    bool is_override_redirect;      /* Override Redirect */
    void *driver_data;              /* Driver Private Data */
} x11_window_t;

/**
 * X11 Display
 */
typedef struct {
    int display_id;                 /* Display ID */
    char display_name[64];          /* Display Name */
    u32 screen_count;               /* Screen Count */
    u32 default_screen;             /* Default Screen */
    u32 root_window;                /* Root Window */
    u32 default_depth;              /* Default Depth */
    u32 default_visual;             /* Default Visual */
    u32 default_colormap;           /* Default Colormap */
    u32 width_mm;                   /* Width (mm) */
    u32 height_mm;                  /* Height (mm) */
    u32 white_pixel;                /* White Pixel */
    u32 black_pixel;                  /* Black Pixel */
    bool is_open;                   /* Is Open */
    void *driver_data;              /* Driver Private Data */
} x11_display_t;

/**
 * Window
 */
typedef struct {
    u32 window_id;                  /* Window ID */
    u32 type;                       /* Window Type */
    u32 state;                      /* Window State */
    rect_t geometry;                /* Geometry */
    rect_t client_geometry;         /* Client Geometry */
    rect_t damaged;                 /* Damaged Region */
    u32 border_width;               /* Border Width */
    color_t border_color;           /* Border Color */
    color_t background_color;       /* Background Color */
    u32 opacity;                    /* Opacity (0-255) */
    u32 z_order;                    /* Z-Order */
    u32 parent_id;                  /* Parent Window */
    u32 *children;                  /* Children */
    u32 child_count;                /* Child Count */
    char title[256];                /* Title */
    char class[256];                /* Class */
    char app_id[256];               /* Application ID */
    void *buffer;                   /* Buffer */
    u32 buffer_width;               /* Buffer Width */
    u32 buffer_height;              /* Buffer Height */
    u32 buffer_format;              /* Buffer Format */
    bool is_mapped;                 /* Is Mapped */
    bool is_focused;                /* Is Focused */
    bool is_hovered;                /* Is Hovered */
    bool is_decorated;              /* Has Decorations */
    bool is_resizable;              /* Is Resizable */
    bool is_closable;               /* Is Closable */
    bool is_minimizable;            /* Is Minimizable */
    bool is_maximizable;            /* Is Maximizable */
    bool is_above;                  /* Is Above */
    bool is_below;                  /* Is Below */
    bool is_modal;                  /* Is Modal */
    bool is_fullscreen;             /* Is Fullscreen */
    bool is_maximized;              /* Is Maximized */
    bool is_minimized;              /* Is Minimized */
    bool is_transparent;            /* Is Transparent */
    bool is_shadowed;               /* Has Shadow */
    u32 workspace;                  /* Workspace */
    void *user_data;                /* User Data */
} window_t;

/**
 * Compositor
 */
typedef struct {
    u32 compositor_id;              /* Compositor ID */
    char name[64];                  /* Compositor Name */
    u32 version;                    /* Version */
    u32 capabilities;               /* Capabilities */
    rect_t output_geometry;         /* Output Geometry */
    u32 output_scale;               /* Output Scale */
    u32 output_transform;           /* Output Transform */
    u32 active_workspace;           /* Active Workspace */
    u32 workspace_count;            /* Workspace Count */
    window_t *windows;              /* Windows */
    u32 window_count;               /* Window Count */
    window_t *focused_window;       /* Focused Window */
    window_t *hovered_window;       /* Hovered Window */
    window_t *active_window;        /* Active Window */
    u32 cursor_x;                   /* Cursor X */
    u32 cursor_y;                   /* Cursor Y */
    void *cursor_buffer;            /* Cursor Buffer */
    u32 cursor_width;               /* Cursor Width */
    u32 cursor_height;              /* Cursor Height */
    bool cursor_visible;            /* Cursor Visible */
    bool vsync_enabled;             /* VSync Enabled */
    bool hw_accel;                  /* Hardware Acceleration */
    void *driver_data;              /* Driver Private Data */
} compositor_t;

/**
 * Window System Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    u32 active_system;              /* Active Window System */
    wl_output_t wl_outputs[WINDOW_MAX_OUTPUTS]; /* Wayland Outputs */
    u32 wl_output_count;            /* Wayland Output Count */
    wl_seat_t wl_seats[WINDOW_MAX_SEATS]; /* Wayland Seats */
    u32 wl_seat_count;              /* Wayland Seat Count */
    wl_surface_t wl_surfaces[WINDOW_MAX_SURFACES]; /* Wayland Surfaces */
    u32 wl_surface_count;           /* Wayland Surface Count */
    wl_shell_surface_t wl_shell_surfaces[WINDOW_MAX_WINDOWS]; /* Shell Surfaces */
    u32 wl_shell_surface_count;     /* Shell Surface Count */
    x11_display_t x11_displays[8]; /* X11 Displays */
    u32 x11_display_count;          /* X11 Display Count */
    x11_window_t x11_windows[WINDOW_MAX_WINDOWS]; /* X11 Windows */
    u32 x11_window_count;           /* X11 Window Count */
    window_t windows[WINDOW_MAX_WINDOWS]; /* Windows */
    u32 window_count;               /* Window Count */
    compositor_t compositor;        /* Compositor */
    u32 active_workspace;           /* Active Workspace */
    u32 workspace_count;            /* Workspace Count */
    void (*on_window_create)(window_t *);
    void (*on_window_destroy)(window_t *);
    void (*on_window_focus)(window_t *);
    void (*on_window_move)(window_t *, s32, s32);
    void (*on_window_resize)(window_t *, u32, u32);
} window_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int window_init(void);
int window_shutdown(void);
bool window_is_initialized(void);

/* Window system selection */
int window_set_active_system(u32 system);
u32 window_get_active_system(void);

/* Wayland functions */
int wayland_init(void);
int wayland_shutdown(void);
wl_output_t *wayland_create_output(void);
wl_seat_t *wayland_create_seat(const char *name);
wl_surface_t *wayland_create_surface(void);
int wayland_destroy_surface(u32 surface_id);
wl_shell_surface_t *wayland_create_shell_surface(wl_surface_t *surface);
int wayland_set_shell_surface_title(wl_shell_surface_t *shell, const char *title);
int wayland_set_shell_surface_fullscreen(wl_shell_surface_t *shell, wl_output_t *output);

/* X11 functions */
int x11_init(void);
int x11_shutdown(void);
x11_display_t *x11_open_display(const char *display_name);
int x11_close_display(x11_display_t *display);
x11_window_t *x11_create_window(x11_display_t *display, x11_window_t *parent, rect_t *geometry);
int x11_destroy_window(x11_display_t *display, x11_window_t *window);
int x11_map_window(x11_display_t *display, x11_window_t *window);
int x11_unmap_window(x11_display_t *display, x11_window_t *window);
int x11_move_window(x11_display_t *display, x11_window_t *window, s32 x, s32 y);
int x11_resize_window(x11_display_t *display, x11_window_t *window, u32 w, u32 h);
int x11_set_window_title(x11_display_t *display, x11_window_t *window, const char *title);

/* Window management */
window_t *window_create(u32 type, rect_t *geometry);
int window_destroy(u32 window_id);
int window_show(u32 window_id);
int window_hide(u32 window_id);
int window_focus(u32 window_id);
int window_unfocus(u32 window_id);
int window_move(u32 window_id, s32 x, s32 y);
int window_resize(u32 window_id, u32 w, u32 h);
int window_set_geometry(u32 window_id, rect_t *geometry);
int window_get_geometry(u32 window_id, rect_t *geometry);
int window_set_title(u32 window_id, const char *title);
int window_set_class(u32 window_id, const char *class);
int window_set_state(u32 window_id, u32 state);
int window_set_opacity(u32 window_id, u32 opacity);
int window_set_z_order(u32 window_id, u32 z);
int window_maximize(u32 window_id);
int window_minimize(u32 window_id);
int window_restore(u32 window_id);
int window_close(u32 window_id);
window_t *window_get(u32 window_id);
int window_list(u32 *ids, u32 *count);

/* Compositor functions */
int compositor_init(void);
int compositor_shutdown(void);
compositor_t *compositor_get(void);
int compositor_add_window(window_t *window);
int compositor_remove_window(u32 window_id);
int compositor_focus_window(u32 window_id);
int compositor_move_cursor(s32 x, s32 y);
int compositor_set_cursor_image(void *image, u32 w, u32 h);
int compositor_show_cursor(bool show);
int compositor_composite(void);
int compositor_set_vsync(bool enable);
int compositor_set_hw_accel(bool enable);

/* Workspace management */
int workspace_create(void);
int workspace_destroy(u32 workspace_id);
int workspace_switch(u32 workspace_id);
u32 workspace_get_active(void);
int workspace_add_window(u32 workspace_id, u32 window_id);
int workspace_remove_window(u32 workspace_id, u32 window_id);

/* Utility functions */
const char *window_get_system_name(u32 system);
const char *window_get_type_name(u32 type);
const char *window_get_state_name(u32 state);
bool window_contains_point(window_t *win, s32 x, s32 y);
bool window_intersects(window_t *a, window_t *b);

/* Global instance */
window_driver_t *window_get_driver(void);

#endif /* _WINDOW_H */
