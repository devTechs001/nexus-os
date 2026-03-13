/*
 * NEXUS OS - Desktop Dashboard
 * gui/dashboard/dashboard.h
 *
 * Main dashboard with system overview and widgets
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _DASHBOARD_H
#define _DASHBOARD_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         DASHBOARD CONFIGURATION                           */
/*===========================================================================*/

#define DASHBOARD_VERSION           "1.0.0"
#define DASHBOARD_MAX_WIDGETS       32
#define DASHBOARD_DEFAULT_COLUMNS   3

/*===========================================================================*/
/*                         WIDGET TYPES                                      */
/*===========================================================================*/

#define WIDGET_TYPE_SYSTEM          0
#define WIDGET_TYPE_WEATHER         1
#define WIDGET_TYPE_CALENDAR        2
#define WIDGET_TYPE_NOTES           3
#define WIDGET_TYPE_CLOCK           4
#define WIDGET_TYPE_MEDIA           5
#define WIDGET_TYPE_SHORTCUTS       6

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Widget Data
 */
typedef struct widget {
    u32 widget_id;                  /* Widget ID */
    u32 type;                       /* Widget Type */
    char title[128];                /* Widget Title */
    s32 x;                          /* X Position */
    s32 y;                          /* Y Position */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    bool visible;                   /* Is Visible */
    bool collapsed;                 /* Is Collapsed */
    bool draggable;                 /* Is Draggable */
    bool resizable;                 /* Is Resizable */
    void *data;                     /* Widget Data */
    void (*on_update)(struct widget *);
    void (*on_click)(struct widget *);
    void (*on_collapse)(struct widget *);
    struct widget *next;            /* Next Widget */
} widget_item_t;

/**
 * System Monitor Widget Data
 */
typedef struct {
    u32 cpu_usage;                  /* CPU Usage (%) */
    u32 ram_usage;                  /* RAM Usage (%) */
    u32 ram_total;                  /* RAM Total (GB) */
    u32 ram_used;                   /* RAM Used (GB) */
    u32 disk_usage;                 /* Disk Usage (%) */
    u32 disk_total;                 /* Disk Total (GB) */
    u32 disk_used;                  /* Disk Used (GB) */
    u32 network_upload;             /* Network Upload (KB/s) */
    u32 network_download;           /* Network Download (KB/s) */
    u32 cpu_temp;                   /* CPU Temperature */
    u32 gpu_temp;                   /* GPU Temperature */
    u32 fan_speed;                  /* Fan Speed (RPM) */
} system_monitor_data_t;

/**
 * Weather Widget Data
 */
typedef struct {
    s32 temperature;                /* Temperature */
    char condition[64];             /* Condition */
    char location[128];             /* Location */
    u32 humidity;                   /* Humidity (%) */
    u32 wind_speed;                 /* Wind Speed */
    char wind_direction[16];        /* Wind Direction */
    s32 feels_like;                 /* Feels Like */
    s32 high_temp;                  /* High Temp */
    s32 low_temp;                   /* Low Temp */
    char forecast[7][64];           /* 7-Day Forecast */
} weather_data_t;

/**
 * Calendar Widget Data
 */
typedef struct {
    u32 day;                        /* Current Day */
    u32 month;                      /* Current Month */
    u32 year;                       /* Current Year */
    u32 weekday;                    /* Current Weekday */
    char month_name[32];            /* Month Name */
    char weekday_name[32];          /* Weekday Name */
    u32 events_count;               /* Events Count */
    char events[8][256];            /* Events */
} calendar_data_t;

/**
 * Notes Widget Data
 */
typedef struct {
    char content[4096];             /* Note Content */
    u64 last_modified;              /* Last Modified */
    bool is_saved;                  /* Is Saved */
} notes_data_t;

/**
 * Media Widget Data
 */
typedef struct {
    char title[256];                /* Media Title */
    char artist[128];               /* Artist */
    char album[128];                /* Album */
    char cover_path[512];           /* Cover Art Path */
    u32 duration;                   /* Duration (seconds) */
    u32 position;                   /* Current Position */
    bool is_playing;                /* Is Playing */
    bool is_paused;                 /* Is Paused */
} media_data_t;

/**
 * Dashboard State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *dashboard_window;     /* Dashboard Window */
    widget_t *dashboard_widget;     /* Dashboard Widget */
    
    /* Widgets */
    widget_item_t *widgets;         /* Widgets */
    u32 widget_count;               /* Widget Count */
    u32 columns;                    /* Column Count */
    
    /* Widget Data */
    system_monitor_data_t system;   /* System Monitor Data */
    weather_data_t weather;         /* Weather Data */
    calendar_data_t calendar;       /* Calendar Data */
    notes_data_t notes;             /* Notes Data */
    media_data_t media;             /* Media Data */
    
    /* Layout */
    u32 grid_size;                  /* Grid Size */
    bool snap_to_grid;              /* Snap to Grid */
    bool show_grid;                 /* Show Grid */
    
    /* Callbacks */
    void (*on_widget_added)(widget_item_t *);
    void (*on_widget_removed)(widget_item_t *);
    void (*on_widget_moved)(widget_item_t *);
    void (*on_widget_resized)(widget_item_t *);
    
    /* User Data */
    void *user_data;
    
} dashboard_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Dashboard lifecycle */
int dashboard_init(dashboard_t *dash);
int dashboard_show(dashboard_t *dash);
int dashboard_hide(dashboard_t *dash);
int dashboard_toggle(dashboard_t *dash);
int dashboard_shutdown(dashboard_t *dash);

/* Widget management */
widget_item_t *dashboard_add_widget(dashboard_t *dash, u32 type, const char *title);
int dashboard_remove_widget(dashboard_t *dash, u32 widget_id);
int dashboard_move_widget(dashboard_t *dash, u32 widget_id, s32 x, s32 y);
int dashboard_resize_widget(dashboard_t *dash, u32 widget_id, u32 width, u32 height);
int dashboard_collapse_widget(dashboard_t *dash, u32 widget_id);
int dashboard_expand_widget(dashboard_t *dash, u32 widget_id);
widget_item_t *dashboard_get_widget(dashboard_t *dash, u32 widget_id);

/* System monitor */
int dashboard_update_system_info(dashboard_t *dash);
int dashboard_set_system_data(dashboard_t *dash, system_monitor_data_t *data);

/* Weather */
int dashboard_update_weather(dashboard_t *dash, const char *location);
int dashboard_set_weather_data(dashboard_t *dash, weather_data_t *data);

/* Calendar */
int dashboard_update_calendar(dashboard_t *dash);
int dashboard_set_calendar_data(dashboard_t *dash, calendar_data_t *data);
int dashboard_add_event(dashboard_t *dash, const char *event);

/* Notes */
int dashboard_save_notes(dashboard_t *dash);
int dashboard_load_notes(dashboard_t *dash);
int dashboard_set_notes_content(dashboard_t *dash, const char *content);

/* Media */
int dashboard_update_media(dashboard_t *dash);
int dashboard_set_media_data(dashboard_t *dash, media_data_t *data);
int dashboard_media_play(dashboard_t *dash);
int dashboard_media_pause(dashboard_t *dash);
int dashboard_media_stop(dashboard_t *dash);
int dashboard_media_next(dashboard_t *dash);
int dashboard_media_previous(dashboard_t *dash);

/* Layout */
int dashboard_set_columns(dashboard_t *dash, u32 columns);
int dashboard_set_grid_size(dashboard_t *dash, u32 size);
int dashboard_snap_widgets(dashboard_t *dash);
int dashboard_arrange_widgets(dashboard_t *dash);

/* Utility functions */
const char *dashboard_get_widget_type_name(u32 type);
dashboard_t *dashboard_get_instance(void);

#endif /* _DASHBOARD_H */
