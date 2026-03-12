/*
 * NEXUS OS - Modern Desktop Environment
 * gui/desktop/desktop.h
 * 
 * Complete desktop environment with panels, menus, taskbar, and system tray
 */

#ifndef _NEXUS_DESKTOP_H
#define _NEXUS_DESKTOP_H

#include "../gui.h"

/*===========================================================================*/
/*                         DESKTOP CONSTANTS                                 */
/*===========================================================================*/

#define DESKTOP_MAX_ICONS           256
#define DESKTOP_MAX_WINDOWS         64
#define DESKTOP_MAX_PANELS          8
#define DESKTOP_MAX_WORKSPACES      10
#define DESKTOP_TASKBAR_HEIGHT      48
#define DESKTOP_PANEL_HEIGHT        40
#define DESKTOP_ICON_SIZE           64
#define DESKTOP_ICON_SPACING        16

/* Desktop Colors (Modern Dark Theme) */
#define DESKTOP_COLOR_BG            0x1A1A2E
#define DESKTOP_COLOR_PANEL         0x16213E
#define DESKTOP_COLOR_TASKBAR       0x0F3460
#define DESKTOP_COLOR_ACCENT        0xE94560
#define DESKTOP_COLOR_TEXT          0xEEEEEE
#define DESKTOP_COLOR_TEXT_DIM      0x888888
#define DESKTOP_COLOR_HOVER         0x1F4287
#define DESKTOP_COLOR_SELECTED      0xE94560

/* Desktop Layout */
#define DESKTOP_LAYOUT_ICON_GRID    0
#define DESKTOP_LAYOUT_ICON_FREE    1

/* Panel Position */
#define PANEL_POSITION_TOP          0
#define PANEL_POSITION_BOTTOM       1
#define PANEL_POSITION_LEFT         2
#define PANEL_POSITION_RIGHT        3

/*===========================================================================*/
/*                         DESKTOP STRUCTURES                                */
/*===========================================================================*/

/**
 * desktop_icon_t - Desktop icon
 */
typedef struct desktop_icon {
    char name[64];
    char path[256];
    char icon_path[256];
    int icon_id;
    int x, y;
    int width, height;
    bool selected;
    bool hidden;
    void (*on_click)(struct desktop_icon *);
    void (*on_double_click)(struct desktop_icon *);
    void (*on_right_click)(struct desktop_icon *);
    void *data;
} desktop_icon_t;

/**
 * taskbar_item_t - Taskbar item
 */
typedef struct taskbar_item {
    u32 window_id;
    char title[128];
    char app_name[64];
    u8 icon[64];
    bool active;
    bool minimized;
    bool urgent;
    u64 last_active;
} taskbar_item_t;

/**
 * system_tray_item_t - System tray item
 */
typedef struct system_tray_item {
    char name[64];
    char tooltip[128];
    u8 icon[64];
    bool visible;
    bool has_menu;
    void (*on_click)(struct system_tray_item *);
    void (*on_right_click)(struct system_tray_item *);
    void *menu;
    void *data;
} system_tray_item_t;

/**
 * desktop_panel_t - Desktop panel (taskbar, menu bar, etc.)
 */
typedef struct desktop_panel {
    char name[32];
    int position;  /* TOP, BOTTOM, LEFT, RIGHT */
    int x, y, width, height;
    bool visible;
    bool autohide;
    u32 color;
    
    /* Panel items */
    taskbar_item_t *taskbar_items;
    int taskbar_count;
    
    system_tray_item_t *tray_items;
    int tray_count;
    
    /* Clock */
    bool show_clock;
    char time_format[32];
    char date_format[32];
    
    /* Start menu button */
    bool show_start_button;
    
    /* Workspace switcher */
    bool show_workspaces;
    int current_workspace;
    int workspace_count;
} desktop_panel_t;

/**
 * start_menu_t - Start menu
 */
typedef struct start_menu {
    bool visible;
    int x, y, width, height;
    
    /* Search */
    char search_text[128];
    bool search_focused;
    
    /* Categories */
    char **categories;
    int category_count;
    int selected_category;
    
    /* Applications */
    void **apps;
    int app_count;
    
    /* User info */
    char username[64];
    u8 user_avatar[128];
    
    /* Power options */
    bool show_power;
    bool show_sleep;
    bool show_restart;
    bool show_logout;
} start_menu_t;

/**
 * desktop_t - Desktop environment
 */
typedef struct desktop {
    /* Desktop info */
    char name[64];
    int workspace;
    int workspace_count;
    
    /* Screen */
    int screen_width;
    int screen_height;
    int screen_depth;
    
    /* Background */
    char wallpaper_path[256];
    u32 wallpaper_mode;  /* STRETCH, FIT, FILL, TILE, CENTER */
    u32 background_color;
    
    /* Icons */
    desktop_icon_t *icons;
    int icon_count;
    int icon_layout;
    int icon_grid_size;
    
    /* Panels */
    desktop_panel_t *panels;
    int panel_count;
    
    /* Start menu */
    start_menu_t start_menu;
    
    /* Context menu */
    void *context_menu;
    bool context_menu_visible;
    
    /* Selection */
    desktop_icon_t *selected_icon;
    desktop_icon_t *hovered_icon;
    
    /* Drag and drop */
    bool dragging;
    desktop_icon_t *dragged_icon;
    int drag_start_x;
    int drag_start_y;
    
    /* Theme */
    char theme_name[64];
    u32 accent_color;
    
    /* Settings */
    bool show_hidden_files;
    bool show_file_extensions;
    bool animations_enabled;
    bool transparency_enabled;
} desktop_t;

/*===========================================================================*/
/*                         DESKTOP API                                       */
/*===========================================================================*/

/* Desktop initialization */
int desktop_init(desktop_t *desktop);
int desktop_shutdown(desktop_t *desktop);
void desktop_render(desktop_t *desktop);

/* Icon management */
int desktop_icon_add(desktop_t *desktop, desktop_icon_t *icon);
int desktop_icon_remove(desktop_t *desktop, int index);
int desktop_icon_create(desktop_t *desktop, const char *name, const char *path, const char *icon_path, int x, int y);
desktop_icon_t *desktop_icon_get(desktop_t *desktop, int index);
int desktop_icon_arrange(desktop_t *desktop);

/* Panel management */
int desktop_panel_create(desktop_t *desktop, int position);
int desktop_panel_destroy(desktop_t *desktop, int index);
desktop_panel_t *desktop_panel_get(desktop_t *desktop, int index);
int desktop_panel_set_autohide(desktop_t *desktop, int index, bool autohide);

/* Taskbar management */
int desktop_taskbar_add_item(desktop_t *desktop, u32 window_id, const char *title, const char *app_name);
int desktop_taskbar_remove_item(desktop_t *desktop, u32 window_id);
int desktop_taskbar_update_item(desktop_t *desktop, u32 window_id, const char *title, bool active, bool minimized);
int desktop_taskbar_set_active(desktop_t *desktop, u32 window_id);

/* System tray management */
int desktop_tray_add_item(desktop_t *desktop, system_tray_item_t *item);
int desktop_tray_remove_item(desktop_t *desktop, const char *name);
system_tray_item_t *desktop_tray_get_item(desktop_t *desktop, const char *name);

/* Start menu */
int desktop_start_menu_toggle(desktop_t *desktop);
int desktop_start_menu_show(desktop_t *desktop);
int desktop_start_menu_hide(desktop_t *desktop);
int desktop_start_menu_add_app(desktop_t *desktop, void *app);

/* Workspace management */
int desktop_workspace_create(desktop_t *desktop);
int desktop_workspace_switch(desktop_t *desktop, int workspace);
int desktop_workspace_get_current(desktop_t *desktop);

/* Theme management */
int desktop_theme_load(desktop_t *desktop, const char *theme_name);
int desktop_theme_save(desktop_t *desktop);
int desktop_set_accent_color(desktop_t *desktop, u32 color);

/* Event handling */
int desktop_handle_mouse_move(desktop_t *desktop, int x, int y, u32 buttons);
int desktop_handle_mouse_click(desktop_t *desktop, int x, int y, int button);
int desktop_handle_mouse_double_click(desktop_t *desktop, int x, int y, int button);
int desktop_handle_key_press(desktop_t *desktop, u32 key, u32 modifiers);

/* Wallpaper */
int desktop_set_wallpaper(desktop_t *desktop, const char *path, u32 mode);
int desktop_set_background_color(desktop_t *desktop, u32 color);

/* Utilities */
int desktop_show_context_menu(desktop_t *desktop, int x, int y);
int desktop_hide_context_menu(desktop_t *desktop);
int desktop_show_notification(desktop_t *desktop, const char *title, const char *message, int timeout_ms);

/* Settings */
int desktop_load_settings(desktop_t *desktop);
int desktop_save_settings(desktop_t *desktop);
int desktop_reset_settings(desktop_t *desktop);

#endif /* _NEXUS_DESKTOP_H */
