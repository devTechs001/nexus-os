/*
 * NEXUS OS - Status Bar & System Tray
 * gui/status-bar/status-bar.h
 *
 * Modern status bar with system tray, clock, and quick settings
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _STATUS_BAR_H
#define _STATUS_BAR_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         STATUS BAR CONFIGURATION                          */
/*===========================================================================*/

#define STATUS_BAR_VERSION          "1.0.0"
#define STATUS_BAR_HEIGHT           32
#define STATUS_BAR_MAX_ITEMS        32
#define STATUS_BAR_MAX_NOTIFICATIONS 16

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Status Bar Item
 */
typedef struct status_item {
    u32 item_id;                    /* Item ID */
    char name[64];                  /* Item Name */
    char icon_path[256];            /* Icon Path */
    char tooltip[256];              /* Tooltip */
    u32 type;                       /* Item Type */
    bool visible;                   /* Is Visible */
    bool enabled;                   /* Is Enabled */
    bool has_menu;                  /* Has Context Menu */
    char value_text[64];            /* Value Text (for indicators) */
    void *data;                     /* Item Data */
    void (*on_click)(struct status_item *);
    void (*on_right_click)(struct status_item *, s32, s32);
    void (*on_update)(struct status_item *);
    struct status_item *next;       /* Next Item */
} status_item_t;

/**
 * Network Status
 */
typedef struct {
    bool connected;                 /* Is Connected */
    u32 type;                       /* Connection Type */
    char ssid[64];                  /* WiFi SSID */
    s32 signal_strength;            /* Signal Strength */
    u32 upload_speed;               /* Upload Speed */
    u32 download_speed;             /* Download Speed */
} network_status_t;

/**
 * Battery Status
 */
typedef struct {
    bool present;                   /* Battery Present */
    bool charging;                  /* Is Charging */
    u32 percentage;                 /* Battery Percentage */
    u32 time_remaining;             /* Time Remaining (minutes) */
    u32 time_to_full;               /* Time to Full (minutes) */
} battery_status_t;

/**
 * Volume Status
 */
typedef struct {
    u32 level;                      /* Volume Level (0-100) */
    bool muted;                     /* Is Muted */
    u32 output_device;              /* Output Device */
    u32 input_device;               /* Input Device */
} volume_status_t;

/**
 * Brightness Status
 */
typedef struct {
    u32 level;                      /* Brightness Level (0-100) */
    bool auto_brightness;           /* Auto Brightness */
} brightness_status_t;

/**
 * Status Bar State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *bar_window;           /* Bar Window */
    widget_t *bar_widget;           /* Bar Widget */
    
    /* Items */
    status_item_t *items;           /* Status Items */
    u32 item_count;                 /* Item Count */
    status_item_t *left_items;      /* Left Side Items */
    status_item_t *center_items;    /* Center Items */
    status_item_t *right_items;     /* Right Side Items */
    
    /* Status */
    network_status_t network;       /* Network Status */
    battery_status_t battery;       /* Battery Status */
    volume_status_t volume;         /* Volume Status */
    brightness_status_t brightness; /* Brightness Status */
    
    /* Clock */
    u32 hour;                       /* Hour */
    u32 minute;                     /* Minute */
    u32 second;                     /* Second */
    u32 day;                        /* Day */
    u32 month;                      /* Month */
    u32 year;                       /* Year */
    bool show_seconds;              /* Show Seconds */
    bool show_date;                 /* Show Date */
    bool use_24hour;                /* 24-hour Format */
    
    /* Quick Settings */
    bool quick_settings_visible;    /* Quick Settings Visible */
    widget_t *quick_settings_panel; /* Quick Settings Panel */
    bool wifi_enabled;              /* WiFi Enabled */
    bool bluetooth_enabled;         /* Bluetooth Enabled */
    bool airplane_mode;             /* Airplane Mode */
    bool dnd_enabled;               /* Do Not Disturb */
    bool night_light;               /* Night Light */
    
    /* Callbacks */
    void (*on_item_added)(status_item_t *);
    void (*on_item_removed)(status_item_t *);
    void (*on_settings_changed)(void);
    
    /* User Data */
    void *user_data;
    
} status_bar_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Status bar lifecycle */
int status_bar_init(status_bar_t *bar);
int status_bar_show(status_bar_t *bar);
int status_bar_hide(status_bar_t *bar);
int status_bar_toggle(status_bar_t *bar);
int status_bar_shutdown(status_bar_t *bar);

/* Item management */
status_item_t *status_bar_add_item(status_bar_t *bar, status_item_t *item);
int status_bar_remove_item(status_bar_t *bar, u32 item_id);
int status_bar_show_item(status_bar_t *bar, u32 item_id);
int status_bar_hide_item(status_bar_t *bar, u32 item_id);
status_item_t *status_bar_get_item(status_bar_t *bar, u32 item_id);

/* Status updates */
int status_bar_update_network(status_bar_t *bar, network_status_t *status);
int status_bar_update_battery(status_bar_t *bar, battery_status_t *status);
int status_bar_update_volume(status_bar_t *bar, volume_status_t *status);
int status_bar_update_brightness(status_bar_t *bar, brightness_status_t *status);
int status_bar_update_time(status_bar_t *bar);

/* Quick settings */
int status_bar_toggle_quick_settings(status_bar_t *bar);
int status_bar_show_quick_settings(status_bar_t *bar);
int status_bar_hide_quick_settings(status_bar_t *bar);
int status_bar_toggle_wifi(status_bar_t *bar);
int status_bar_toggle_bluetooth(status_bar_t *bar);
int status_bar_toggle_airplane_mode(status_bar_t *bar);
int status_bar_toggle_dnd(status_bar_t *bar);
int status_bar_toggle_night_light(status_bar_t *bar);
int status_bar_set_volume(status_bar_t *bar, u32 level);
int status_bar_set_brightness(status_bar_t *bar, u32 level);

/* System actions */
int status_bar_lock(status_bar_t *bar);
int status_bar_logout(status_bar_t *bar);
int status_bar_shutdown_system(status_bar_t *bar);
int status_bar_restart_system(status_bar_t *bar);
int status_bar_sleep(status_bar_t *bar);
int status_bar_hibernate(status_bar_t *bar);

/* Utility functions */
status_bar_t *status_bar_get_instance(void);

#endif /* _STATUS_BAR_H */
