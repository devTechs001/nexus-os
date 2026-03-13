/*
 * NEXUS OS - GUI Framework
 * Copyright (c) 2026 NEXUS Development Team
 *
 * gui.h - GUI Framework Header
 *
 * This header defines the core GUI framework interfaces for NEXUS OS.
 * It provides support for window management, compositing, rendering,
 * and widget systems.
 */

#ifndef _NEXUS_GUI_H
#define _NEXUS_GUI_H

#include "../kernel/include/types.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         GUI FRAMEWORK VERSION                             */
/*===========================================================================*/

#define GUI_VERSION_MAJOR       1
#define GUI_VERSION_MINOR       0
#define GUI_VERSION_PATCH       0
#define GUI_VERSION_STRING      "1.0.0"
#define GUI_CODENAME            "VisualCore"

/*===========================================================================*/
/*                         FRAMEWORK CONFIGURATION                           */
/*===========================================================================*/

/* Window configuration */
#define MAX_WINDOWS             256
#define MAX_LAYERS              64
#define MAX_DISPLAYS            8

/* Color configuration */
#define COLOR_DEPTH_8           8
#define COLOR_DEPTH_16          16
#define COLOR_DEPTH_24          24
#define COLOR_DEPTH_32          32

/* Rendering */
#define MAX_DIRTY_RECTS         64
#define RENDER_BATCH_SIZE       128

/* Animation */
#define ANIMATION_DURATION_DEFAULT  300  /* ms */
#define MAX_ANIMATIONS          128

/* Input */
#define MAX_TOUCH_POINTS        10
#define MAX_INPUT_DEVICES       16

/* Window flags */
#define WINDOW_FLAG_VISIBLE         0x0001
#define WINDOW_FLAG_FOCUSED         0x0002
#define WINDOW_FLAG_MODAL           0x0004
#define WINDOW_FLAG_TRANSPARENT     0x0008
#define WINDOW_FLAG_BORDERLESS      0x0010
#define WINDOW_FLAG_RESIZABLE       0x0020
#define WINDOW_FLAG_FULLSCREEN      0x0040
#define WINDOW_FLAG_MAXIMIZED       0x0080
#define WINDOW_FLAG_MINIMIZED       0x0100
#define WINDOW_FLAG_TOPMOST         0x0200
#define WINDOW_FLAG_CHILD           0x0400
#define WINDOW_FLAG_POPUP           0x0800

/* Window types */
#define WINDOW_TYPE_NORMAL          0
#define WINDOW_TYPE_DIALOG          1
#define WINDOW_TYPE_TOOLTIP         2
#define WINDOW_TYPE_MENU            3
#define WINDOW_TYPE_POPUP           4
#define WINDOW_TYPE_NOTIFICATION    5
#define WINDOW_TYPE_SYSTEM          6

/* Cursor types */
#define CURSOR_ARROW                0
#define CURSOR_IBEAM                1
#define CURSOR_WAIT                 2
#define CURSOR_CROSS                3
#define CURSOR_HAND                 4
#define CURSOR_SIZE_ALL             5
#define CURSOR_SIZE_NS              6
#define CURSOR_SIZE_WE              7
#define CURSOR_SIZE_NESW            8
#define CURSOR_SIZE_NWSE            9
#define CURSOR_NO                   10

/* Event types */
#define EVENT_NONE                  0
#define EVENT_WINDOW_CREATE         1
#define EVENT_WINDOW_DESTROY        2
#define EVENT_WINDOW_SHOW           3
#define EVENT_WINDOW_HIDE           4
#define EVENT_WINDOW_MOVE           5
#define EVENT_WINDOW_RESIZE         6
#define EVENT_WINDOW_FOCUS          7
#define EVENT_WINDOW_BLUR           8
#define EVENT_MOUSE_MOVE            9
#define EVENT_MOUSE_DOWN            10
#define EVENT_MOUSE_UP              11
#define EVENT_MOUSE_DBLCLICK        12
#define EVENT_MOUSE_WHEEL           13
#define EVENT_MOUSE_ENTER           14
#define EVENT_MOUSE_LEAVE           15
#define EVENT_KEY_DOWN              16
#define EVENT_KEY_UP                17
#define EVENT_KEY_CHAR              18
#define EVENT_TOUCH_BEGIN           19
#define EVENT_TOUCH_MOVE            20
#define EVENT_TOUCH_END             21
#define EVENT_TOUCH_CANCEL          22
#define EVENT_PAINT                 23
#define EVENT_ANIMATION             24

/* Mouse buttons */
#define MOUSE_BUTTON_LEFT           0x01
#define MOUSE_BUTTON_RIGHT          0x02
#define MOUSE_BUTTON_MIDDLE         0x04
#define MOUSE_BUTTON_X1             0x08
#define MOUSE_BUTTON_X2             0x10

/* Key modifiers */
#define MODIFIER_SHIFT              0x01
#define MODIFIER_CONTROL            0x02
#define MODIFIER_ALT                0x04
#define MODIFIER_SUPER              0x08
#define MODIFIER_CAPS_LOCK          0x10
#define MODIFIER_NUM_LOCK           0x20

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Color representation
 */
typedef struct color {
    union {
        struct {
            u8 r, g, b, a;      /* Red, Green, Blue, Alpha */
        };
        u32 rgba;               /* Packed RGBA */
    };
} color_t;

/* Common colors */
#define COLOR_BLACK       {0, 0, 0, 255}
#define COLOR_WHITE       {255, 255, 255, 255}
#define COLOR_RED         {255, 0, 0, 255}
#define COLOR_GREEN       {0, 255, 0, 255}
#define COLOR_BLUE        {0, 0, 255, 255}
#define COLOR_TRANSPARENT {0, 0, 0, 0}

/**
 * Rectangle
 */
typedef struct rect {
    s32 x, y;                   /* Top-left corner */
    s32 width, height;          /* Dimensions */
} rect_t;

/**
 * Point
 */
typedef struct point {
    s32 x, y;
} point_t;

/**
 * Size
 */
typedef struct size {
    s32 width, height;
} gui_size_t;

/**
 * Matrix 4x4 (for transformations)
 */
typedef struct matrix4 {
    float m[16];
} matrix4_t;

/**
 * Window Handle
 */
typedef struct window window_t;

/**
 * Window Properties
 */
typedef struct window_props {
    char title[256];            /* Window title */
    u32 type;                   /* Window type */
    u32 flags;                  /* Window flags */
    rect_t bounds;              /* Window position and size */
    rect_t client_rect;         /* Client area */
    color_t background;         /* Background color */
    u32 opacity;                /* Opacity (0-255) */
    window_t *parent;           /* Parent window */
    window_t *owner;            /* Owner window */
    void *user_data;            /* User data */
} window_props_t;

/**
 * Window Structure
 */
struct window {
    u32 window_id;              /* Unique window ID */
    window_props_t props;       /* Window properties */
    
    /* State */
    bool visible;               /* Window visibility */
    bool focused;               /* Has focus */
    bool enabled;               /* Window enabled */
    bool dirty;                 /* Needs repaint */
    
    /* Z-order */
    u32 z_order;                /* Z-order position */
    u32 layer_id;               /* Layer ID */
    
    /* Children */
    window_t *children;         /* Child windows */
    window_t *next_sibling;     /* Next sibling */
    window_t *prev_sibling;     /* Previous sibling */
    
    /* Rendering */
    void *surface;              /* Drawing surface */
    void *back_buffer;          /* Back buffer */
    rect_t dirty_rects[MAX_DIRTY_RECTS];
    u32 num_dirty_rects;
    
    /* Callbacks */
    void (*on_create)(window_t *);
    void (*on_destroy)(window_t *);
    void (*on_paint)(window_t *, void *);
    void (*on_resize)(window_t *, s32, s32);
    void (*on_move)(window_t *, s32, s32);
    void (*on_focus)(window_t *);
    void (*on_blur)(window_t *);
    void (*on_mouse_move)(window_t *, s32, s32, u32);
    void (*on_mouse_down)(window_t *, s32, s32, u32);
    void (*on_mouse_up)(window_t *, s32, s32, u32);
    void (*on_key_down)(window_t *, u32, u32);
    void (*on_key_up)(window_t *, u32, u32);
    
    /* Reference counting */
    atomic_t refcount;
};

/**
 * Input Event
 */
typedef struct input_event {
    u32 type;                   /* Event type */
    u32 timestamp;              /* Event timestamp */
    u32 window_id;              /* Target window */
    
    union {
        struct {
            s32 x, y;           /* Mouse position */
            s32 dx, dy;         /* Mouse delta */
            u32 buttons;        /* Button state */
            s32 wheel;          /* Wheel delta */
        } mouse;
        
        struct {
            u32 key_code;       /* Virtual key code */
            u32 scan_code;      /* Hardware scan code */
            u32 unicode;        /* Unicode character */
            u32 modifiers;      /* Modifier keys */
        } keyboard;
        
        struct {
            u32 touch_id;       /* Touch point ID */
            s32 x, y;           /* Touch position */
            s32 pressure;       /* Touch pressure */
            float size;         /* Touch size */
        } touch;
    };
} input_event_t;

/**
 * Display Information
 */
typedef struct display_info {
    u32 display_id;             /* Display identifier */
    char name[64];              /* Display name */
    bool is_primary;            /* Primary display */
    bool is_active;             /* Display is active */
    
    /* Resolution */
    u32 width;                  /* Width in pixels */
    u32 height;                 /* Height in pixels */
    u32 refresh_rate;           /* Refresh rate (Hz) */
    u32 color_depth;            /* Color depth (bits) */
    
    /* Physical properties */
    u32 dpi_x;                  /* Horizontal DPI */
    u32 dpi_y;                  /* Vertical DPI */
    float scale_factor;         /* UI scale factor */
    
    /* Position */
    s32 x_offset;               /* X offset in virtual desktop */
    s32 y_offset;               /* Y offset in virtual desktop */
    
    /* Rotation */
    u32 rotation;               /* Rotation (0, 90, 180, 270) */
} display_info_t;

/**
 * Cursor Information
 */
typedef struct cursor_info {
    u32 type;                   /* Cursor type */
    s32 x, y;                   /* Cursor position */
    bool visible;               /* Cursor visibility */
    void *bitmap;               /* Custom cursor bitmap */
    u32 hotspot_x;              /* Hotspot X */
    u32 hotspot_y;              /* Hotspot Y */
} cursor_info_t;

/**
 * Animation
 */
typedef struct animation {
    u32 animation_id;           /* Animation ID */
    char name[64];              /* Animation name */
    u32 target_window_id;       /* Target window */
    
    /* Animation properties */
    u32 duration_ms;            /* Duration in milliseconds */
    u32 elapsed_ms;             /* Elapsed time */
    bool running;               /* Animation running */
    bool looping;               /* Loop animation */
    
    /* Interpolation */
    u32 interpolation;          /* Interpolation type */
    float (*easing)(float);     /* Easing function */
    
    /* Values */
    float start_value;          /* Start value */
    float end_value;            /* End value */
    float current_value;        /* Current value */
    
    /* Callbacks */
    void (*on_update)(struct animation *, float);
    void (*on_complete)(struct animation *);
    
    /* Links */
    struct animation *next;
} animation_t;

/**
 * Graphics Context
 */
typedef struct graphics_context {
    void *surface;              /* Drawing surface */
    rect_t clip_rect;           /* Clip rectangle */
    color_t foreground;         /* Foreground color */
    color_t background;         /* Background color */
    u32 line_width;             /* Line width */
    u32 font_id;                /* Current font */
    u32 font_size;              /* Font size */
    matrix4_t transform;        /* Transform matrix */
    u32 antialias;              /* Antialiasing enabled */
    u32 alpha;                  /* Global alpha */
} graphics_context_t;

/*===========================================================================*/
/*                         GUI FRAMEWORK STATE                               */
/*===========================================================================*/

typedef struct gui_framework {
    bool initialized;           /* Framework initialized */
    u32 version;                /* Framework version */
    
    /* Display */
    display_info_t displays[MAX_DISPLAYS];
    u32 num_displays;
    u32 primary_display;
    
    /* Windows */
    window_t *windows;          /* Window list */
    u32 num_windows;
    u32 next_window_id;
    window_t *focused_window;
    window_t *capture_window;
    window_t *hover_window;
    
    /* Compositor */
    void *compositor;           /* Compositor instance */
    bool compositor_active;
    
    /* Renderer */
    void *renderer;             /* Renderer instance */
    u32 renderer_type;          /* Renderer type */
    
    /* Input */
    cursor_info_t cursor;
    input_event_t input_queue[256];
    u32 input_head;
    u32 input_tail;
    
    /* Animations */
    animation_t *animations;
    u32 num_animations;
    
    /* Theme */
    char theme_name[64];        /* Current theme */
    color_t theme_colors[32];   /* Theme colors */
    
    /* Performance */
    u64 frame_count;
    u64 last_frame_time;
    u32 fps;
    
    /* Synchronization */
    spinlock_t lock;
} gui_framework_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Framework initialization */
int gui_framework_init(void);
void gui_framework_shutdown(void);
bool gui_framework_is_initialized(void);

/* Display management */
int display_get_count(void);
int display_get_info(u32 display_id, display_info_t *info);
int display_get_primary(display_info_t *info);
int display_set_primary(u32 display_id);
int display_get_virtual_rect(rect_t *rect);

/* Window management */
window_t *window_create(window_props_t *props);
void window_destroy(window_t *window);
int window_show(window_t *window);
int window_hide(window_t *window);
int window_close(window_t *window);
int window_set_title(window_t *window, const char *title);
int window_set_bounds(window_t *window, s32 x, s32 y, s32 width, s32 height);
int window_set_position(window_t *window, s32 x, s32 y);
int window_set_size(window_t *window, s32 width, s32 height);
int window_get_bounds(window_t *window, rect_t *rect);
int window_focus(window_t *window);
int window_blur(window_t *window);
window_t *window_find_by_id(u32 window_id);
window_t *window_find_at(s32 x, s32 y);

/* Input handling */
int input_get_cursor(cursor_info_t *cursor);
int input_set_cursor_type(u32 type);
int input_set_cursor_position(s32 x, s32 y);
int input_show_cursor(bool show);
int input_get_event(input_event_t *event);
int input_poll_event(input_event_t *event, u32 timeout_ms);
int input_set_capture(window_t *window);
int input_release_capture(void);

/* Rendering */
graphics_context_t *graphics_create_context(void *surface);
void graphics_destroy_context(graphics_context_t *ctx);
int graphics_clear(graphics_context_t *ctx, color_t color);
int graphics_draw_pixel(graphics_context_t *ctx, s32 x, s32 y, color_t color);
int graphics_draw_line(graphics_context_t *ctx, s32 x1, s32 y1, s32 x2, s32 y2);
int graphics_draw_rect(graphics_context_t *ctx, rect_t *rect);
int graphics_fill_rect(graphics_context_t *ctx, rect_t *rect, color_t color);
int graphics_draw_text(graphics_context_t *ctx, s32 x, s32 y, const char *text);
int graphics_blit(graphics_context_t *ctx, void *src, rect_t *src_rect, s32 dst_x, s32 dst_y);
int graphics_present(void *surface, rect_t *dirty_rects, u32 num_rects);

/* Animation */
animation_t *animation_create(const char *name, u32 target_window_id);
void animation_destroy(animation_t *anim);
int animation_start(animation_t *anim);
int animation_stop(animation_t *anim);
int animation_set_duration(animation_t *anim, u32 duration_ms);
int animation_set_values(animation_t *anim, float start, float end);
int animation_set_easing(animation_t *anim, u32 interpolation);

/* Compositor */
int compositor_init(void);
void compositor_shutdown(void);
int compositor_add_window(window_t *window);
int compositor_remove_window(window_t *window);
int compositor_update_window(window_t *window);
int compositor_composite(void *output_surface);

/* Utility functions */
const char *gui_get_version(void);
const char *gui_get_codename(void);
color_t color_from_rgba(u8 r, u8 g, u8 b, u8 a);
color_t color_from_argb(u32 argb);
u32 color_to_argb(color_t color);
bool rect_contains(rect_t *rect, s32 x, s32 y);
bool rect_intersects(rect_t *a, rect_t *b);
rect_t rect_union(rect_t *a, rect_t *b);
rect_t rect_intersection(rect_t *a, rect_t *b);

#endif /* _NEXUS_GUI_H */
