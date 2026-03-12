/*
 * NEXUS OS - Advanced Window Manager
 * gui/window-manager/window-manager.h
 *
 * Modern compositing window manager with advanced features
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _WINDOW_MANAGER_H
#define _WINDOW_MANAGER_H

#include "../gui.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         WINDOW MANAGER CONFIGURATION                      */
/*===========================================================================*/

#define WM_VERSION                  "1.0.0"
#define WM_MAX_WINDOWS              256
#define WM_MAX_WORKSPACES           10
#define WM_MAX_MONITORS             8
#define WM_ANIMATION_DURATION       300
#define WM_SNAP_THRESHOLD           10
#define WM_MIN_WINDOW_WIDTH         100
#define WM_MIN_WINDOW_HEIGHT        80

/*===========================================================================*/
/*                         WINDOW STATES                                     */
/*===========================================================================*/

#define WINDOW_STATE_NORMAL         0
#define WINDOW_STATE_MAXIMIZED      1
#define WINDOW_STATE_MINIMIZED      2
#define WINDOW_STATE_FULLSCREEN     3
#define WINDOW_STATE_SNAPPED_LEFT   4
#define WINDOW_STATE_SNAPPED_RIGHT  5
#define WINDOW_STATE_SNAPPED_TOP    6
#define WINDOW_STATE_SNAPPED_BOTTOM 7
#define WINDOW_STATE_TILED          8

/*===========================================================================*/
/*                         WINDOW DECORATIONS                                */
/*===========================================================================*/

#define DECOR_TITLE_BAR             (1 << 0)
#define DECOR_BORDER                (1 << 1)
#define DECOR_RESIZE_HANDLE         (1 << 2)
#define DECOR_CLOSE_BUTTON          (1 << 3)
#define DECOR_MAXIMIZE_BUTTON       (1 << 4)
#define DECOR_MINIMIZE_BUTTON       (1 << 5)
#define DECOR_SHADOW                (1 << 6)
#define DECOR_ALL                   0xFFFFFFFF

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Window Geometry
 */
typedef struct {
    s32 x, y;                     /* Position */
    s32 width, height;            /* Size */
    s32 min_width, min_height;    /* Minimum Size */
    s32 max_width, max_height;    /* Maximum Size */
} window_geometry_t;

/**
 * Window Decoration Colors
 */
typedef struct {
    color_t title_bg;             /* Title Bar Background */
    color_t title_fg;             /* Title Bar Foreground */
    color_t border;               /* Border Color */
    color_t shadow;               /* Shadow Color */
    color_t active;               /* Active Window Accent */
    color_t inactive;             /* Inactive Window */
} window_decor_colors_t;

/**
 * Window Animation
 */
typedef struct {
    u32 type;                     /* Animation Type */
    u32 duration;                 /* Duration (ms) */
    u64 start_time;               /* Start Time */
    u64 elapsed;                  /* Elapsed Time */
    bool running;                 /* Is Running */
    window_geometry_t from;       /* Start Geometry */
    window_geometry_t to;         /* End Geometry */
    float progress;               /* Progress (0-1) */
    float (*easing)(float);       /* Easing Function */
} window_animation_t;

/**
 * Window Drop Shadow
 */
typedef struct {
    bool enabled;                 /* Is Enabled */
    u32 size;                     /* Shadow Size */
    u32 opacity;                  /* Shadow Opacity */
    s32 offset_x;                 /* Shadow Offset X */
    s32 offset_y;                 /* Shadow Offset Y */
    color_t color;                /* Shadow Color */
} window_shadow_t;

/**
 * Window Structure (Extended)
 */
typedef struct wm_window {
    u32 window_id;                /* Unique Window ID */
    char title[256];              /* Window Title */
    u32 workspace;                /* Workspace Number */
    
    /* Geometry */
    window_geometry_t geometry;   /* Current Geometry */
    window_geometry_t restored;   /* Restored Geometry (for maximize) */
    
    /* State */
    u32 state;                    /* Window State */
    u32 flags;                    /* Window Flags */
    bool visible;                 /* Is Visible */
    bool focused;                 /* Has Focus */
    bool active;                  /* Is Active */
    bool dragging;                /* Is Being Dragged */
    bool resizing;                /* Is Being Resized */
    bool moving;                  /* Is Moving */
    
    /* Decorations */
    u32 decorations;              /* Decoration Flags */
    window_decor_colors_t decor_colors; /* Decoration Colors */
    window_shadow_t shadow;       /* Drop Shadow */
    
    /* Z-Order */
    u32 z_order;                  /* Z-Order Position */
    u32 layer;                    /* Window Layer */
    
    /* Parent/Child */
    struct wm_window *parent;     /* Parent Window */
    struct wm_window *children;   /* Child Windows */
    struct wm_window *next;       /* Next Sibling */
    
    /* Surface/Buffer */
    void *surface;                /* Window Surface */
    void *back_buffer;            /* Back Buffer */
    rect_t dirty_rects[64];       /* Dirty Rectangles */
    u32 dirty_count;              /* Dirty Rectangle Count */
    
    /* Animation */
    window_animation_t animation; /* Current Animation */
    
    /* Snap/Tile */
    rect_t snap_rect;             /* Snap Rectangle */
    u32 snap_state;               /* Snap State */
    
    /* Callbacks */
    void (*on_create)(struct wm_window *);
    void (*on_destroy)(struct wm_window *);
    void (*on_focus)(struct wm_window *);
    void (*on_blur)(struct wm_window *);
    void (*on_move)(struct wm_window *, s32, s32);
    void (*on_resize)(struct wm_window *, s32, s32);
    void (*on_state_change)(struct wm_window *, u32);
    void (*on_paint)(struct wm_window *, void *);
    
    /* User Data */
    void *user_data;
    
    /* Reference Count */
    atomic_t refcount;
} wm_window_t;

/**
 * Monitor Information
 */
typedef struct {
    u32 monitor_id;               /* Monitor ID */
    char name[64];                /* Monitor Name */
    bool is_primary;              /* Is Primary */
    bool is_active;               /* Is Active */
    rect_t geometry;              /* Monitor Geometry */
    rect_t work_area;             /* Work Area (excludes panels) */
    u32 scale_factor;             /* Scale Factor */
    u32 rotation;                 /* Rotation */
    u32 refresh_rate;             /* Refresh Rate */
    char manufacturer[64];        /* Manufacturer */
    char model[64];               /* Model */
    char serial[64];              /* Serial Number */
} wm_monitor_t;

/**
 * Workspace Information
 */
typedef struct {
    u32 workspace_id;             /* Workspace ID */
    char name[64];                /* Workspace Name */
    bool is_active;               /* Is Active */
    u32 window_count;             /* Window Count */
    wm_window_t *windows;         /* Window List */
    color_t background;           /* Background Color */
    char wallpaper[256];          /* Wallpaper Path */
} wm_workspace_t;

/**
 * Snap Layout
 */
typedef struct {
    u32 layout_id;                /* Layout ID */
    char name[64];                /* Layout Name */
    u32 zone_count;               /* Zone Count */
    rect_t zones[8];              /* Zone Rectangles */
} snap_layout_t;

/**
 * Window Manager State
 */
typedef struct {
    bool initialized;             /* Is Initialized */
    bool running;                 /* Is Running */
    
    /* Windows */
    wm_window_t *windows;         /* Window List */
    u32 window_count;             /* Window Count */
    u32 next_window_id;           /* Next Window ID */
    wm_window_t *focused_window;  /* Focused Window */
    wm_window_t *active_window;   /* Active Window */
    wm_window_t *drag_window;     /* Dragged Window */
    wm_window_t *resize_window;   /* Resized Window */
    
    /* Monitors */
    wm_monitor_t monitors[WM_MAX_MONITORS];
    u32 monitor_count;
    u32 primary_monitor;
    
    /* Workspaces */
    wm_workspace_t workspaces[WM_MAX_WORKSPACES];
    u32 workspace_count;
    u32 current_workspace;
    
    /* Layout */
    rect_t virtual_desktop;       /* Virtual Desktop Geometry */
    snap_layout_t snap_layouts[8]; /* Snap Layouts */
    u32 snap_layout_count;
    
    /* Theme */
    window_decor_colors_t theme;  /* Current Theme */
    char theme_name[64];          /* Theme Name */
    u32 title_height;             /* Title Bar Height */
    u32 border_width;             /* Border Width */
    
    /* Behavior */
    bool focus_follows_mouse;     /* Focus Follows Mouse */
    bool raise_on_focus;          /* Raise Window on Focus */
    bool snap_enabled;            /* Snap Windows Enabled */
    bool animations_enabled;      /* Animations Enabled */
    bool shadows_enabled;         /* Shadows Enabled */
    u32 snap_threshold;           /* Snap Threshold (pixels) */
    
    /* Input */
    s32 mouse_x;                  /* Mouse X */
    s32 mouse_y;                  /* Mouse Y */
    u32 mouse_buttons;            /* Mouse Buttons */
    wm_window_t *hover_window;    /* Hover Window */
    wm_window_t *capture_window;  /* Capture Window */
    
    /* Performance */
    u64 frame_count;              /* Frame Count */
    u64 last_frame_time;          /* Last Frame Time */
    u32 fps;                      /* FPS */
    u32 render_time;              /* Render Time (μs) */
    
    /* Callbacks */
    int (*on_window_created)(wm_window_t *);
    int (*on_window_destroyed)(wm_window_t *);
    int (*on_workspace_changed)(u32);
    int (*on_focus_changed)(wm_window_t *);
    
    /* User Data */
    void *user_data;
    
} window_manager_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Manager lifecycle */
int wm_init(window_manager_t *wm);
int wm_run(window_manager_t *wm);
int wm_shutdown(window_manager_t *wm);
bool wm_is_running(window_manager_t *wm);

/* Window creation/destruction */
wm_window_t *wm_create_window(window_manager_t *wm, window_props_t *props);
int wm_destroy_window(window_manager_t *wm, wm_window_t *win);
int wm_close_window(window_manager_t *wm, wm_window_t *win);

/* Window operations */
int wm_show_window(window_manager_t *wm, wm_window_t *win);
int wm_hide_window(window_manager_t *wm, wm_window_t *win);
int wm_focus_window(window_manager_t *wm, wm_window_t *win);
int wm_raise_window(window_manager_t *wm, wm_window_t *win);
int wm_lower_window(window_manager_t *wm, wm_window_t *win);
int wm_move_window(window_manager_t *wm, wm_window_t *win, s32 x, s32 y);
int wm_resize_window(window_manager_t *wm, wm_window_t *win, s32 w, s32 h);
int wm_set_geometry(window_manager_t *wm, wm_window_t *win, s32 x, s32 y, s32 w, s32 h);

/* Window state */
int wm_maximize_window(window_manager_t *wm, wm_window_t *win);
int wm_minimize_window(window_manager_t *wm, wm_window_t *win);
int wm_restore_window(window_manager_t *wm, wm_window_t *win);
int wm_fullscreen_window(window_manager_t *wm, wm_window_t *win);
int wm_toggle_maximize(window_manager_t *wm, wm_window_t *win);

/* Snapping and tiling */
int wm_snap_window_left(window_manager_t *wm, wm_window_t *win);
int wm_snap_window_right(window_manager_t *wm, wm_window_t *win);
int wm_snap_window_top(window_manager_t *wm, wm_window_t *win);
int wm_snap_window_bottom(window_manager_t *wm, wm_window_t *win);
int wm_tile_windows(window_manager_t *wm);
int wm_cascade_windows(window_manager_t *wm);

/* Window search */
wm_window_t *wm_get_window_by_id(window_manager_t *wm, u32 id);
wm_window_t *wm_get_window_at(window_manager_t *wm, s32 x, s32 y);
wm_window_t *wm_get_focused_window(window_manager_t *wm);
int wm_list_windows(window_manager_t *wm, wm_window_t *wins, u32 *count);

/* Monitor management */
int wm_detect_monitors(window_manager_t *wm);
wm_monitor_t *wm_get_monitor(window_manager_t *wm, u32 id);
wm_monitor_t *wm_get_primary_monitor(window_manager_t *wm);
int wm_set_primary_monitor(window_manager_t *wm, u32 id);

/* Workspace management */
int wm_switch_workspace(window_manager_t *wm, u32 workspace);
int wm_move_window_to_workspace(window_manager_t *wm, wm_window_t *win, u32 workspace);
wm_workspace_t *wm_get_workspace(window_manager_t *wm, u32 id);
int wm_rename_workspace(window_manager_t *wm, u32 id, const char *name);

/* Theme */
int wm_set_theme(window_manager_t *wm, const char *theme_name);
int wm_load_theme(window_manager_t *wm, const char *path);
window_decor_colors_t *wm_get_theme(window_manager_t *wm);

/* Rendering */
int wm_composite(window_manager_t *wm);
int wm_render_window(window_manager_t *wm, wm_window_t *win);
int wm_update_dirty_regions(window_manager_t *wm);

/* Input handling */
int wm_handle_mouse_move(window_manager_t *wm, s32 x, s32 y, u32 buttons);
int wm_handle_mouse_button(window_manager_t *wm, u32 button, bool pressed);
int wm_handle_key(window_manager_t *wm, u32 key, bool pressed, u32 modifiers);

/* Utility functions */
const char *wm_get_state_name(u32 state);
const char *wm_get_version(void);
rect_t wm_get_work_area(window_manager_t *wm, wm_monitor_t *mon);
rect_t wm_get_snap_rect(window_manager_t *wm, u32 snap_state, wm_monitor_t *mon);

/* Global instance */
window_manager_t *wm_get_instance(void);

#endif /* _WINDOW_MANAGER_H */
