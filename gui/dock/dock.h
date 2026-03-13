/*
 * NEXUS OS - Application Dock
 * gui/dock/dock.h
 *
 * Modern application dock with pinned apps and running indicators
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _DOCK_H
#define _DOCK_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         DOCK CONFIGURATION                                */
/*===========================================================================*/

#define DOCK_VERSION                "1.0.0"
#define DOCK_MAX_ITEMS              32
#define DOCK_ITEM_SIZE              48
#define DOCK_ITEM_SIZE_LARGE        64
#define DOCK_HEIGHT                 60
#define DOCK_WIDTH_AUTO             0

/*===========================================================================*/
/*                         DOCK POSITIONS                                    */
/*===========================================================================*/

#define DOCK_POSITION_BOTTOM         0
#define DOCK_POSITION_LEFT           1
#define DOCK_POSITION_RIGHT          2
#define DOCK_POSITION_TOP            3

/*===========================================================================*/
/*                         DOCK ITEM TYPES                                   */
/*===========================================================================*/

#define DOCK_ITEM_APP                0
#define DOCK_ITEM_SEPARATOR          1
#define DOCK_ITEM_LAUNCHER           2
#define DOCK_ITEM_TRAY               3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Dock Item
 */
typedef struct dock_item {
    u32 item_id;                    /* Item ID */
    u32 type;                       /* Item Type */
    char name[128];                 /* Item Name */
    char icon_path[512];            /* Icon Path */
    char app_path[512];             /* Application Path */
    char tooltip[256];              /* Tooltip */
    bool is_pinned;                 /* Is Pinned */
    bool is_running;                /* Is Running */
    bool is_active;                 /* Is Active Window */
    bool is_urgent;                 /* Has Urgent Notification */
    u32 window_count;               /* Window Count */
    u32 launch_order;               /* Launch Order */
    void *app_data;                 /* Application Data */
    struct dock_item *next;         /* Next Item */
} dock_item_t;

/**
 * Dock Settings
 */
typedef struct {
    u32 position;                   /* Dock Position */
    u32 item_size;                  /* Item Size */
    bool autohide;                  /* Auto Hide */
    bool magnification;             /* Magnification Effect */
    float magnification_factor;     /* Magnification Factor */
    u32 opacity;                    /* Opacity (0-100) */
    bool blur_background;           /* Blur Background */
    color_t background_color;       /* Background Color */
    color_t highlight_color;        /* Highlight Color */
    u32 animation_duration;         /* Animation Duration (ms) */
    bool show_labels;               /* Show Labels */
    bool show_running_indicator;    /* Show Running Indicator */
    bool show_all_windows;          /* Show All Windows */
} dock_settings_t;

/**
 * Dock State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *dock_window;          /* Dock Window */
    widget_t *dock_widget;          /* Dock Widget */
    
    /* Items */
    dock_item_t *items;             /* Dock Items */
    u32 item_count;                 /* Item Count */
    dock_item_t *hover_item;        /* Hover Item */
    dock_item_t *active_item;       /* Active Item */
    
    /* Settings */
    dock_settings_t settings;       /* Dock Settings */
    
    /* Animation */
    bool is_animating;              /* Is Animating */
    float animation_progress;       /* Animation Progress */
    u64 animation_start;            /* Animation Start Time */
    
    /* Callbacks */
    void (*on_item_clicked)(dock_item_t *);
    void (*on_item_right_clicked)(dock_item_t *, s32, s32);
    void (*on_item_dragged)(dock_item_t *, s32, s32);
    void (*on_settings_changed)(dock_settings_t *);
    
    /* User Data */
    void *user_data;
    
} dock_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Dock lifecycle */
int dock_init(dock_t *dock);
int dock_show(dock_t *dock);
int dock_hide(dock_t *dock);
int dock_toggle(dock_t *dock);
int dock_shutdown(dock_t *dock);

/* Item management */
dock_item_t *dock_add_item(dock_t *dock, dock_item_t *item);
int dock_remove_item(dock_t *dock, u32 item_id);
int dock_move_item(dock_t *dock, u32 item_id, u32 new_index);
int dock_pin_item(dock_t *dock, u32 item_id);
int dock_unpin_item(dock_t *dock, u32 item_id);
dock_item_t *dock_get_item(dock_t *dock, u32 item_id);
dock_item_t *dock_get_item_by_app(dock_t *dock, const char *app_path);

/* App status */
int dock_set_app_running(dock_t *dock, const char *app_path, bool running);
int dock_set_app_active(dock_t *dock, const char *app_path, bool active);
int dock_set_app_urgent(dock_t *dock, const char *app_path, bool urgent);
int dock_set_app_window_count(dock_t *dock, const char *app_path, u32 count);

/* Settings */
int dock_set_position(dock_t *dock, u32 position);
int dock_set_autohide(dock_t *dock, bool autohide);
int dock_set_magnification(dock_t *dock, bool enabled, float factor);
int dock_set_opacity(dock_t *dock, u32 opacity);
int dock_apply_settings(dock_t *dock, dock_settings_t *settings);

/* Context menu */
int dock_show_context_menu(dock_t *dock, dock_item_t *item, s32 x, s32 y);
int dock_add_context_action(dock_item_t *item, const char *name, void (*callback)(dock_item_t *));

/* Utility functions */
const char *dock_get_position_name(u32 position);
dock_t *dock_get_instance(void);

#endif /* _DOCK_H */
