/*
 * NEXUS OS - Desktop Environment
 * gui/desktop-environment/desktop-env.h
 *
 * Modern desktop environment with panels, backgrounds, menus, and more
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _DESKTOP_ENV_H
#define _DESKTOP_ENV_H

#include "../gui.h"
#include "../widgets/widgets.h"
#include "../window-manager/window-manager.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         DESKTOP ENVIRONMENT CONFIGURATION                 */
/*===========================================================================*/

#define DESKTOP_ENV_VERSION         "1.0.0"
#define DESKTOP_MAX_ICONS           128
#define DESKTOP_MAX_PANELS          8
#define DESKTOP_MAX_MENU_ITEMS      64
#define DESKTOP_MAX_WORKSPACES      10
#define DESKTOP_MAX_MONITOR         8
#define ICON_SIZE_SMALL             16
#define ICON_SIZE_MEDIUM            32
#define ICON_SIZE_LARGE             48
#define ICON_SIZE_XLARGE            64

/*===========================================================================*/
/*                         BACKGROUND TYPES                                  */
/*===========================================================================*/

#define BG_TYPE_SOLID_COLOR         0
#define BG_TYPE_GRADIENT_H          1
#define BG_TYPE_GRADIENT_V          2
#define BG_TYPE_GRADIENT_RADIAL     3
#define BG_TYPE_IMAGE               4
#define BG_TYPE_IMAGE_SLIDESHOW     5
#define BG_TYPE_PATTERN             6

/*===========================================================================*/
/*                         GRADIENT STYLES                                   */
/*===========================================================================*/

#define GRADIENT_LINEAR             0
#define GRADIENT_RADIAL             1
#define GRADIENT_CONIC              2
#define GRADIENT_DIAMOND            3

/*===========================================================================*/
/*                         PANEL POSITIONS                                   */
/*===========================================================================*/

#define PANEL_POSITION_TOP          0
#define PANEL_POSITION_BOTTOM       1
#define PANEL_POSITION_LEFT         2
#define PANEL_POSITION_RIGHT        3
#define PANEL_POSITION_FLOATING     4

/*===========================================================================*/
/*                         MENU TYPES                                        */
/*===========================================================================*/

#define MENU_TYPE_START             0
#define MENU_TYPE_CONTEXT           1
#define MENU_TYPE_DROPDOWN          2
#define MENU_TYPE_POPUP             3
#define MENU_TYPE_SYSTEM            4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Color Stop (for gradients)
 */
typedef struct {
    float position;                 /* Position (0.0-1.0) */
    color_t color;                  /* Color */
} color_stop_t;

/**
 * Gradient Definition
 */
typedef struct {
    u32 type;                       /* Gradient Type */
    u32 style;                      /* Gradient Style */
    color_stop_t stops[8];          /* Color Stops */
    u32 stop_count;                 /* Stop Count */
    s32 start_x;                    /* Start X (linear) */
    s32 start_y;                    /* Start Y (linear) */
    s32 end_x;                      /* End X (linear) */
    s32 end_y;                      /* End Y (linear) */
    s32 center_x;                   /* Center X (radial) */
    s32 center_y;                   /* Center Y (radial) */
    u32 radius;                     /* Radius (radial) */
} gradient_t;

/**
 * Background Configuration
 */
typedef struct {
    u32 type;                       /* Background Type */
    color_t solid_color;            /* Solid Color */
    gradient_t gradient;            /* Gradient Definition */
    char image_path[512];           /* Image Path */
    char slideshow_folder[512];     /* Slideshow Folder */
    u32 slideshow_interval;         /* Slideshow Interval (seconds) */
    char pattern_name[64];          /* Pattern Name */
    u32 fit_mode;                   /* Fit Mode (fill, fit, stretch, tile, center) */
    u32 primary_monitor;            /* Primary Monitor */
    bool span_monitors;             /* Span Monitors */
} desktop_background_t;

/**
 * Desktop Icon
 */
typedef struct {
    u32 icon_id;                    /* Icon ID */
    char name[128];                 /* Icon Name */
    char path[512];                 /* Item Path */
    char icon_path[512];            /* Icon Image Path */
    u32 icon_type;                  /* Icon Type (file, folder, app, device) */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool selected;                  /* Is Selected */
    bool hidden;                    /* Is Hidden */
    bool locked;                    /* Is Locked */
    char tooltip[256];              /* Tooltip */
    u32 workspace;                  /* Workspace */
    void (*on_click)(struct desktop_icon *);
    void (*on_double_click)(struct desktop_icon *);
    void (*on_right_click)(struct desktop_icon *);
    void *data;                     /* User Data */
} desktop_icon_t;

/**
 * Menu Item
 */
typedef struct menu_item {
    u32 item_id;                    /* Item ID */
    char label[128];                /* Item Label */
    char icon_path[256];            /* Icon Path */
    char shortcut[32];              /* Keyboard Shortcut */
    u32 item_type;                  /* Item Type (normal, separator, submenu) */
    bool enabled;                   /* Is Enabled */
    bool checked;                   /* Is Checked */
    struct menu_item *submenu;      /* Submenu */
    u32 submenu_count;              /* Submenu Count */
    void (*on_click)(struct menu_item *);
    void *data;                     /* User Data */
    struct menu_item *next;         /* Next Item */
} menu_item_t;

/**
 * Menu
 */
typedef struct {
    u32 menu_id;                    /* Menu ID */
    u32 menu_type;                  /* Menu Type */
    char title[64];                 /* Menu Title */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    menu_item_t *items;             /* Menu Items */
    u32 item_count;                 /* Item Count */
    bool visible;                   /* Is Visible */
    bool auto_hide;                 /* Auto Hide */
    u32 max_height;                 /* Max Height */
    color_t background;             /* Background Color */
    color_t foreground;             /* Foreground Color */
    color_t hover;                  /* Hover Color */
    u32 font_size;                  /* Font Size */
} desktop_menu_t;

/**
 * Search Bar
 */
typedef struct {
    u32 search_id;                  /* Search ID */
    char placeholder[64];           /* Placeholder Text */
    char query[512];                /* Current Query */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
    bool focused;                   /* Is Focused */
    bool show_suggestions;          /* Show Suggestions */
    u32 max_suggestions;            /* Max Suggestions */
    char suggestions[16][256];      /* Suggestions */
    u32 suggestion_count;           /* Suggestion Count */
    u32 selected_suggestion;        /* Selected Suggestion */
    void (*on_search)(struct search_bar *, const char *);
    void (*on_suggestion_click)(struct search_bar *, u32);
    void *data;                     /* User Data */
} search_bar_t;

/**
 * Panel Item (for taskbar, system tray, etc.)
 */
typedef struct {
    u32 item_id;                    /* Item ID */
    char name[64];                  /* Item Name */
    char icon_path[256];            /* Icon Path */
    char tooltip[256];              /* Tooltip */
    u32 item_type;                  /* Item Type */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
    bool enabled;                   /* Is Enabled */
    bool active;                    /* Is Active */
    bool urgent;                    /* Is Urgent */
    color_t background;             /* Background Color */
    color_t foreground;             /* Foreground Color */
    void (*on_click)(struct panel_item *);
    void (*on_right_click)(struct panel_item *);
    void *data;                     /* User Data */
} panel_item_t;

/**
 * Panel (Taskbar, Menu Bar, etc.)
 */
typedef struct {
    u32 panel_id;                   /* Panel ID */
    char name[64];                  /* Panel Name */
    u32 position;                   /* Panel Position */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 autohide_height;            /* Autohide Height */
    bool visible;                   /* Is Visible */
    bool autohide;                  /* Auto Hide */
    bool floating;                  /* Floating */
    color_t background;             /* Background Color */
    color_t foreground;             /* Foreground Color */
    panel_item_t *items;            /* Panel Items */
    u32 item_count;                 /* Item Count */
    u32 max_items;                  /* Max Items */
    u32 icon_size;                  /* Icon Size */
    u32 spacing;                    /* Item Spacing */
    u32 margin;                     /* Panel Margin */
    void *data;                     /* User Data */
} desktop_panel_t;

/**
 * System Tray Item
 */
typedef struct {
    u32 tray_id;                    /* Tray ID */
    char name[64];                  /* Item Name */
    char icon_path[256];            /* Icon Path */
    char tooltip[256];              /* Tooltip */
    bool visible;                   /* Is Visible */
    bool has_menu;                  /* Has Menu */
    bool has_notification;          /* Has Notification */
    u32 notification_count;         /* Notification Count */
    desktop_menu_t *menu;           /* Context Menu */
    void (*on_click)(struct system_tray_item *);
    void (*on_right_click)(struct system_tray_item *);
    void *data;                     /* User Data */
} system_tray_item_t;

/**
 * System Tray
 */
typedef struct {
    system_tray_item_t items[32];   /* Tray Items */
    u32 item_count;                 /* Item Count */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
} system_tray_t;

/**
 * Notification
 */
typedef struct {
    u32 notify_id;                  /* Notification ID */
    char title[128];                /* Title */
    char message[512];              /* Message */
    char icon_path[256];            /* Icon Path */
    u32 urgency;                    /* Urgency (low, normal, critical) */
    u64 timestamp;                  /* Timestamp */
    u32 timeout;                    /* Timeout (ms) */
    bool has_actions;               /* Has Actions */
    char actions[4][64];            /* Actions */
    void (*on_click)(struct notification *);
    void (*on_action)(struct notification *, u32);
    void *data;                     /* User Data */
} notification_t;

/**
 * Notification Area
 */
typedef struct {
    notification_t notifications[16]; /* Notifications */
    u32 notification_count;         /* Notification Count */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
    u32 position;                   /* Position */
} notification_area_t;

/**
 * Workspace Switcher
 */
typedef struct {
    u32 workspace_count;            /* Workspace Count */
    u32 current_workspace;          /* Current Workspace */
    char names[DESKTOP_MAX_WORKSPACES][64]; /* Workspace Names */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
    u32 layout;                     /* Layout (horizontal, vertical, grid) */
    u32 columns;                    /* Columns (grid layout) */
    color_t active_color;           /* Active Workspace Color */
    color_t inactive_color;         /* Inactive Workspace Color */
} workspace_switcher_t;

/**
 * Clock/Calendar Widget
 */
typedef struct {
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
    bool show_date;                 /* Show Date */
    bool show_seconds;              /* Show Seconds */
    bool show_calendar;             /* Show Calendar */
    bool is_24hour;                 /* 24-hour Format */
    char date_format[64];           /* Date Format */
    char time_format[64];           /* Time Format */
    color_t color;                  /* Text Color */
    u32 font_size;                  /* Font Size */
    void (*on_click)(struct clock_widget *);
} clock_widget_t;

/**
 * Quick Settings Panel
 */
typedef struct {
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
    bool wifi_enabled;              /* WiFi Enabled */
    bool bluetooth_enabled;         /* Bluetooth Enabled */
    bool airplane_mode;             /* Airplane Mode */
    bool dnd_enabled;               /* Do Not Disturb */
    bool night_light;               /* Night Light */
    u32 brightness;                 /* Brightness (0-100) */
    u32 volume;                     /* Volume (0-100) */
    char wifi_network[64];          /* Current WiFi Network */
    char bluetooth_device[64];      /* Connected Bluetooth */
} quick_settings_t;

/**
 * Desktop Environment State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool running;                   /* Is Running */
    
    /* Window Manager Integration */
    window_manager_t *wm;           /* Window Manager */
    compositor_t *compositor;       /* Compositor */
    
    /* Desktop */
    desktop_background_t background; /* Background */
    desktop_icon_t icons[DESKTOP_MAX_ICONS]; /* Icons */
    u32 icon_count;                 /* Icon Count */
    u32 icon_layout;                /* Icon Layout (grid, free, auto) */
    u32 icon_size;                  /* Icon Size */
    u32 grid_size;                  /* Grid Size */
    bool show_icons;                /* Show Icons */
    bool snap_to_grid;              /* Snap to Grid */
    
    /* Panels */
    desktop_panel_t panels[DESKTOP_MAX_PANELS]; /* Panels */
    u32 panel_count;                /* Panel Count */
    desktop_panel_t *taskbar;       /* Taskbar */
    desktop_panel_t *menubar;       /* Menu Bar */
    
    /* Menus */
    desktop_menu_t start_menu;      /* Start Menu */
    desktop_menu_t context_menu;    /* Context Menu */
    desktop_menu_t *active_menu;    /* Active Menu */
    
    /* Search */
    search_bar_t search_bar;        /* Search Bar */
    bool search_active;             /* Search Active */
    
    /* System Tray */
    system_tray_t system_tray;      /* System Tray */
    
    /* Notifications */
    notification_area_t notifications; /* Notification Area */
    
    /* Widgets */
    workspace_switcher_t workspace_switcher; /* Workspace Switcher */
    clock_widget_t clock;           /* Clock Widget */
    quick_settings_t quick_settings; /* Quick Settings */
    
    /* Theme */
    char theme_name[64];            /* Theme Name */
    color_t theme_colors[32];       /* Theme Colors */
    char font_name[64];             /* Font Name */
    u32 font_size;                  /* Font Size */
    
    /* Behavior */
    bool animations_enabled;        /* Animations Enabled */
    bool sounds_enabled;            /* Sounds Enabled */
    bool show_desktop_on_hover;     /* Show Desktop on Hover */
    u32 double_click_speed;         /* Double Click Speed */
    
    /* Input */
    s32 mouse_x;                    /* Mouse X */
    s32 mouse_y;                    /* Mouse Y */
    desktop_icon_t *hover_icon;     /* Hover Icon */
    desktop_icon_t *selected_icon;  /* Selected Icon */
    
    /* Callbacks */
    int (*on_icon_clicked)(desktop_icon_t *);
    int (*on_panel_item_clicked)(panel_item_t *);
    int (*on_menu_item_clicked)(menu_item_t *);
    int (*on_search)(search_bar_t *, const char *);
    int (*on_workspace_changed)(u32);
    
    /* User Data */
    void *user_data;
    
} desktop_environment_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Desktop lifecycle */
int desktop_env_init(desktop_environment_t *env);
int desktop_env_run(desktop_environment_t *env);
int desktop_env_shutdown(desktop_environment_t *env);
bool desktop_env_is_running(desktop_environment_t *env);

/* Background */
int desktop_set_background(desktop_environment_t *env, desktop_background_t *bg);
int desktop_set_solid_color(desktop_environment_t *env, color_t color);
int desktop_set_gradient(desktop_environment_t *env, gradient_t *gradient);
int desktop_set_image(desktop_environment_t *env, const char *path, u32 fit_mode);
int desktop_set_slideshow(desktop_environment_t *env, const char *folder, u32 interval);
desktop_background_t *desktop_get_background(desktop_environment_t *env);

/* Icons */
int desktop_add_icon(desktop_environment_t *env, desktop_icon_t *icon);
int desktop_remove_icon(desktop_environment_t *env, u32 icon_id);
int desktop_update_icon(desktop_environment_t *env, u32 icon_id, desktop_icon_t *icon);
desktop_icon_t *desktop_get_icon(desktop_environment_t *env, u32 icon_id);
int desktop_select_icon(desktop_environment_t *env, u32 icon_id);
int desktop_arrange_icons(desktop_environment_t *env, u32 layout);
int desktop_delete_icon(desktop_environment_t *env, u32 icon_id);

/* Panels */
int desktop_create_panel(desktop_environment_t *env, const char *name, u32 position, u32 height);
int desktop_remove_panel(desktop_environment_t *env, u32 panel_id);
int desktop_add_panel_item(desktop_environment_t *env, u32 panel_id, panel_item_t *item);
int desktop_remove_panel_item(desktop_environment_t *env, u32 panel_id, u32 item_id);
desktop_panel_t *desktop_get_panel(desktop_environment_t *env, u32 panel_id);
int desktop_set_panel_position(desktop_environment_t *env, u32 panel_id, u32 position);

/* Menus */
int desktop_show_start_menu(desktop_environment_t *env);
int desktop_hide_start_menu(desktop_environment_t *env);
int desktop_toggle_start_menu(desktop_environment_t *env);
int desktop_show_context_menu(desktop_environment_t *env, s32 x, s32 y);
int desktop_hide_context_menu(desktop_environment_t *env);
int desktop_add_menu_item(desktop_menu_t *menu, menu_item_t *item);
int desktop_remove_menu_item(desktop_menu_t *menu, u32 item_id);

/* Search */
int desktop_show_search(desktop_environment_t *env);
int desktop_hide_search(desktop_environment_t *env);
int desktop_toggle_search(desktop_environment_t *env);
int desktop_search_query(desktop_environment_t *env, const char *query);
int desktop_clear_search(desktop_environment_t *env);

/* System Tray */
int desktop_add_tray_item(desktop_environment_t *env, system_tray_item_t *item);
int desktop_remove_tray_item(desktop_environment_t *env, u32 tray_id);
int desktop_update_tray_item(desktop_environment_t *env, u32 tray_id, system_tray_item_t *item);

/* Notifications */
int desktop_show_notification(desktop_environment_t *env, notification_t *notify);
int desktop_dismiss_notification(desktop_environment_t *env, u32 notify_id);
int desktop_clear_notifications(desktop_environment_t *env);

/* Widgets */
int desktop_update_clock(desktop_environment_t *env);
int desktop_switch_workspace(desktop_environment_t *env, u32 workspace);
int desktop_show_quick_settings(desktop_environment_t *env);
int desktop_hide_quick_settings(desktop_environment_t *env);

/* Theme */
int desktop_set_theme(desktop_environment_t *env, const char *theme_name);
int desktop_load_theme(desktop_environment_t *env, const char *path);
int desktop_save_theme(desktop_environment_t *env, const char *path);

/* Rendering */
int desktop_render(desktop_environment_t *env);
int desktop_render_background(desktop_environment_t *env);
int desktop_render_icons(desktop_environment_t *env);
int desktop_render_panels(desktop_environment_t *env);

/* Input handling */
int desktop_handle_mouse(desktop_environment_t *env, s32 x, s32 y, u32 buttons);
int desktop_handle_key(desktop_environment_t *env, u32 key, u32 modifiers);

/* Utility functions */
const char *desktop_get_bg_type_name(u32 type);
const char *desktop_get_panel_position_name(u32 position);
gradient_t *gradient_create(u32 type, color_t color1, color_t color2);
color_t gradient_sample(gradient_t *gradient, float position);

/* Global instance */
desktop_environment_t *desktop_env_get_instance(void);

#endif /* _DESKTOP_ENV_H */
