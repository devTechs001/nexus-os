/*
 * NEXUS OS - Desktop Dashboard Implementation
 * gui/dashboard/dashboard.c
 *
 * Main dashboard with system overview and widgets
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "dashboard/dashboard.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL DASHBOARD INSTANCE                         */
/*===========================================================================*/

static dashboard_t g_dashboard;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int create_dashboard_ui(dashboard_t *dash);
static int create_system_monitor_widget(dashboard_t *dash);
static int create_weather_widget(dashboard_t *dash);
static int create_calendar_widget(dashboard_t *dash);
static int create_notes_widget(dashboard_t *dash);
static int create_media_widget(dashboard_t *dash);

static void on_widget_clicked(widget_t *widget);
static void on_widget_collapse(widget_t *widget);

static void update_system_monitor(dashboard_t *dash);
static void update_weather(dashboard_t *dash);
static void update_calendar(dashboard_t *dash);

/*===========================================================================*/
/*                         DASHBOARD LIFECYCLE                               */
/*===========================================================================*/

/**
 * dashboard_init - Initialize desktop dashboard
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int dashboard_init(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    printk("[DASHBOARD] ========================================\n");
    printk("[DASHBOARD] NEXUS OS Desktop Dashboard v%s\n", DASHBOARD_VERSION);
    printk("[DASHBOARD] ========================================\n");
    printk("[DASHBOARD] Initializing dashboard...\n");
    
    /* Clear structure */
    memset(dash, 0, sizeof(dashboard_t));
    dash->initialized = true;
    dash->visible = false;
    
    /* Default settings */
    dash->columns = DASHBOARD_DEFAULT_COLUMNS;
    dash->grid_size = 50;
    dash->snap_to_grid = true;
    dash->show_grid = false;
    
    /* Create UI */
    if (create_dashboard_ui(dash) != 0) {
        printk("[DASHBOARD] Failed to create UI\n");
        return -1;
    }
    
    /* Create default widgets */
    create_system_monitor_widget(dash);
    create_weather_widget(dash);
    create_calendar_widget(dash);
    create_notes_widget(dash);
    create_media_widget(dash);
    
    printk("[DASHBOARD] Dashboard initialized with %d widgets\n", dash->widget_count);
    printk("[DASHBOARD] ========================================\n");
    
    return 0;
}

/**
 * dashboard_show - Show dashboard
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_show(dashboard_t *dash)
{
    if (!dash || !dash->initialized) {
        return -EINVAL;
    }
    
    if (dash->dashboard_window) {
        window_show(dash->dashboard_window);
        dash->visible = true;
    }
    
    return 0;
}

/**
 * dashboard_hide - Hide dashboard
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_hide(dashboard_t *dash)
{
    if (!dash || !dash->initialized) {
        return -EINVAL;
    }
    
    if (dash->dashboard_window) {
        window_hide(dash->dashboard_window);
        dash->visible = false;
    }
    
    return 0;
}

/**
 * dashboard_toggle - Toggle dashboard visibility
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_toggle(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    if (dash->visible) {
        return dashboard_hide(dash);
    } else {
        return dashboard_show(dash);
    }
}

/**
 * dashboard_shutdown - Shutdown dashboard
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_shutdown(dashboard_t *dash)
{
    if (!dash || !dash->initialized) {
        return -EINVAL;
    }
    
    /* Free all widgets */
    widget_item_t *widget = dash->widgets;
    while (widget) {
        widget_item_t *next = widget->next;
        kfree(widget);
        widget = next;
    }
    
    if (dash->dashboard_window) {
        window_hide(dash->dashboard_window);
    }
    
    dash->initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_dashboard_ui - Create dashboard UI
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_dashboard_ui(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    /* Create dashboard window */
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "Dashboard");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_RESIZABLE;
    props.bounds.x = 100;
    props.bounds.y = 100;
    props.bounds.width = 1200;
    props.bounds.height = 800;
    props.background = color_from_rgba(30, 30, 45, 255);
    
    dash->dashboard_window = window_create(&props);
    if (!dash->dashboard_window) {
        return -1;
    }
    
    /* Create dashboard widget */
    dash->dashboard_widget = panel_create(NULL, 0, 0, 1200, 800);
    if (!dash->dashboard_widget) {
        return -1;
    }
    
    widget_set_colors(dash->dashboard_widget,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(30, 30, 45, 255),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_system_monitor_widget - Create system monitor widget
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
static int create_system_monitor_widget(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_add_widget(dash, WIDGET_TYPE_SYSTEM, "System Monitor");
    if (!widget) {
        return -1;
    }
    
    /* Set position and size */
    widget->x = 10;
    widget->y = 10;
    widget->width = 380;
    widget->height = 250;
    
    return 0;
}

/**
 * create_weather_widget - Create weather widget
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
static int create_weather_widget(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_add_widget(dash, WIDGET_TYPE_WEATHER, "Weather");
    if (!widget) {
        return -1;
    }
    
    widget->x = 400;
    widget->y = 10;
    widget->width = 380;
    widget->height = 250;
    
    return 0;
}

/**
 * create_calendar_widget - Create calendar widget
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
static int create_calendar_widget(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_add_widget(dash, WIDGET_TYPE_CALENDAR, "Calendar");
    if (!widget) {
        return -1;
    }
    
    widget->x = 790;
    widget->y = 10;
    widget->width = 380;
    widget->height = 250;
    
    return 0;
}

/**
 * create_notes_widget - Create notes widget
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
static int create_notes_widget(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_add_widget(dash, WIDGET_TYPE_NOTES, "Notes");
    if (!widget) {
        return -1;
    }
    
    widget->x = 10;
    widget->y = 270;
    widget->width = 380;
    widget->height = 250;
    
    return 0;
}

/**
 * create_media_widget - Create media widget
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
static int create_media_widget(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_add_widget(dash, WIDGET_TYPE_MEDIA, "Media Player");
    if (!widget) {
        return -1;
    }
    
    widget->x = 400;
    widget->y = 270;
    widget->width = 380;
    widget->height = 250;
    
    return 0;
}

/*===========================================================================*/
/*                         WIDGET MANAGEMENT                                 */
/*===========================================================================*/

/**
 * dashboard_add_widget - Add widget to dashboard
 * @dash: Pointer to dashboard structure
 * @type: Widget type
 * @title: Widget title
 *
 * Returns: Pointer to added widget, or NULL on failure
 */
widget_item_t *dashboard_add_widget(dashboard_t *dash, u32 type, const char *title)
{
    if (!dash || !title) {
        return NULL;
    }
    
    widget_item_t *widget = kzalloc(sizeof(widget_item_t));
    if (!widget) {
        return NULL;
    }
    
    widget->widget_id = dash->widget_count + 1;
    widget->type = type;
    strncpy(widget->title, title, sizeof(widget->title) - 1);
    widget->visible = true;
    widget->collapsed = false;
    widget->draggable = true;
    widget->resizable = true;
    
    /* Add to list */
    widget->next = dash->widgets;
    dash->widgets = widget;
    dash->widget_count++;
    
    if (dash->on_widget_added) {
        dash->on_widget_added(widget);
    }
    
    return widget;
}

/**
 * dashboard_remove_widget - Remove widget from dashboard
 * @dash: Pointer to dashboard structure
 * @widget_id: Widget ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int dashboard_remove_widget(dashboard_t *dash, u32 widget_id)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t **prev = &dash->widgets;
    widget_item_t *curr = dash->widgets;
    
    while (curr) {
        if (curr->widget_id == widget_id) {
            *prev = curr->next;
            kfree(curr);
            dash->widget_count--;
            
            if (dash->on_widget_removed) {
                dash->on_widget_removed(curr);
            }
            
            return 0;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    return -ENOENT;
}

/**
 * dashboard_move_widget - Move widget
 * @dash: Pointer to dashboard structure
 * @widget_id: Widget ID
 * @x: New X position
 * @y: New Y position
 *
 * Returns: 0 on success
 */
int dashboard_move_widget(dashboard_t *dash, u32 widget_id, s32 x, s32 y)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_get_widget(dash, widget_id);
    if (widget) {
        widget->x = x;
        widget->y = y;
        
        if (dash->snap_to_grid) {
            widget->x = (x / dash->grid_size) * dash->grid_size;
            widget->y = (y / dash->grid_size) * dash->grid_size;
        }
        
        if (dash->on_widget_moved) {
            dash->on_widget_moved(widget);
        }
    }
    
    return 0;
}

/**
 * dashboard_resize_widget - Resize widget
 * @dash: Pointer to dashboard structure
 * @widget_id: Widget ID
 * @width: New width
 * @height: New height
 *
 * Returns: 0 on success
 */
int dashboard_resize_widget(dashboard_t *dash, u32 widget_id, u32 width, u32 height)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_get_widget(dash, widget_id);
    if (widget) {
        widget->width = width;
        widget->height = height;
        
        if (dash->on_widget_resized) {
            dash->on_widget_resized(widget);
        }
    }
    
    return 0;
}

/**
 * dashboard_collapse_widget - Collapse widget
 * @dash: Pointer to dashboard structure
 * @widget_id: Widget ID
 *
 * Returns: 0 on success
 */
int dashboard_collapse_widget(dashboard_t *dash, u32 widget_id)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_get_widget(dash, widget_id);
    if (widget) {
        widget->collapsed = true;
        
        if (widget->on_collapse) {
            widget->on_collapse(widget);
        }
    }
    
    return 0;
}

/**
 * dashboard_expand_widget - Expand widget
 * @dash: Pointer to dashboard structure
 * @widget_id: Widget ID
 *
 * Returns: 0 on success
 */
int dashboard_expand_widget(dashboard_t *dash, u32 widget_id)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dashboard_get_widget(dash, widget_id);
    if (widget) {
        widget->collapsed = false;
    }
    
    return 0;
}

/**
 * dashboard_get_widget - Get widget by ID
 * @dash: Pointer to dashboard structure
 * @widget_id: Widget ID
 *
 * Returns: Widget pointer, or NULL if not found
 */
widget_item_t *dashboard_get_widget(dashboard_t *dash, u32 widget_id)
{
    if (!dash) {
        return NULL;
    }
    
    widget_item_t *widget = dash->widgets;
    while (widget) {
        if (widget->widget_id == widget_id) {
            return widget;
        }
        widget = widget->next;
    }
    
    return NULL;
}

/*===========================================================================*/
/*                         SYSTEM MONITOR                                    */
/*===========================================================================*/

/**
 * dashboard_update_system_info - Update system information
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_update_system_info(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    /* In real implementation, would get actual system stats */
    dash->system.cpu_usage = 25;
    dash->system.ram_usage = 60;
    dash->system.ram_total = 16;
    dash->system.ram_used = 9;
    dash->system.disk_usage = 45;
    dash->system.disk_total = 512;
    dash->system.disk_used = 230;
    dash->system.network_upload = 128;
    dash->system.network_download = 1024;
    dash->system.cpu_temp = 55;
    dash->system.gpu_temp = 60;
    dash->system.fan_speed = 1200;
    
    update_system_monitor(dash);
    
    return 0;
}

/**
 * dashboard_set_system_data - Set system data
 * @dash: Pointer to dashboard structure
 * @data: System data
 *
 * Returns: 0 on success
 */
int dashboard_set_system_data(dashboard_t *dash, system_monitor_data_t *data)
{
    if (!dash || !data) {
        return -EINVAL;
    }
    
    memcpy(&dash->system, data, sizeof(system_monitor_data_t));
    update_system_monitor(dash);
    
    return 0;
}

/**
 * update_system_monitor - Update system monitor display
 * @dash: Pointer to dashboard structure
 */
static void update_system_monitor(dashboard_t *dash)
{
    if (!dash) return;
    
    /* In real implementation, would update widget display */
    printk("[DASHBOARD] CPU: %d%%, RAM: %d%%, Disk: %d%%\n",
           dash->system.cpu_usage, dash->system.ram_usage, dash->system.disk_usage);
}

/*===========================================================================*/
/*                         WEATHER                                           */
/*===========================================================================*/

/**
 * dashboard_update_weather - Update weather
 * @dash: Pointer to dashboard structure
 * @location: Location
 *
 * Returns: 0 on success
 */
int dashboard_update_weather(dashboard_t *dash, const char *location)
{
    if (!dash || !location) {
        return -EINVAL;
    }
    
    /* In real implementation, would fetch weather data */
    strncpy(dash->weather.location, location, sizeof(dash->weather.location) - 1);
    dash->weather.temperature = 22;
    strcpy(dash->weather.condition, "Sunny");
    
    update_weather(dash);
    
    return 0;
}

/**
 * dashboard_set_weather_data - Set weather data
 * @dash: Pointer to dashboard structure
 * @data: Weather data
 *
 * Returns: 0 on success
 */
int dashboard_set_weather_data(dashboard_t *dash, weather_data_t *data)
{
    if (!dash || !data) {
        return -EINVAL;
    }
    
    memcpy(&dash->weather, data, sizeof(weather_data_t));
    update_weather(dash);
    
    return 0;
}

/**
 * update_weather - Update weather display
 * @dash: Pointer to dashboard structure
 */
static void update_weather(dashboard_t *dash)
{
    if (!dash) return;
    
    printk("[DASHBOARD] Weather: %s, %d°C, %s\n",
           dash->weather.location, dash->weather.temperature, dash->weather.condition);
}

/*===========================================================================*/
/*                         CALENDAR                                          */
/*===========================================================================*/

/**
 * dashboard_update_calendar - Update calendar
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_update_calendar(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    /* Get current date */
    u64 now = get_time_ms();
    dash->calendar.day = ((u32)(now / 86400000) % 30) + 1;
    dash->calendar.month = ((u32)(now / 2592000000) % 12) + 1;
    dash->calendar.year = 2026;
    dash->calendar.weekday = (u32)(now / 86400000) % 7;
    
    update_calendar(dash);
    
    return 0;
}

/**
 * dashboard_set_calendar_data - Set calendar data
 * @dash: Pointer to dashboard structure
 * @data: Calendar data
 *
 * Returns: 0 on success
 */
int dashboard_set_calendar_data(dashboard_t *dash, calendar_data_t *data)
{
    if (!dash || !data) {
        return -EINVAL;
    }
    
    memcpy(&dash->calendar, data, sizeof(calendar_data_t));
    update_calendar(dash);
    
    return 0;
}

/**
 * dashboard_add_event - Add calendar event
 * @dash: Pointer to dashboard structure
 * @event: Event description
 *
 * Returns: 0 on success
 */
int dashboard_add_event(dashboard_t *dash, const char *event)
{
    if (!dash || !event) {
        return -EINVAL;
    }
    
    if (dash->calendar.events_count < 8) {
        strncpy(dash->calendar.events[dash->calendar.events_count],
                event, sizeof(dash->calendar.events[0]) - 1);
        dash->calendar.events_count++;
    }
    
    return 0;
}

/**
 * update_calendar - Update calendar display
 * @dash: Pointer to dashboard structure
 */
static void update_calendar(dashboard_t *dash)
{
    if (!dash) return;
    
    printk("[DASHBOARD] Calendar: %d/%d/%d\n",
           dash->calendar.day, dash->calendar.month, dash->calendar.year);
}

/*===========================================================================*/
/*                         NOTES                                             */
/*===========================================================================*/

/**
 * dashboard_save_notes - Save notes
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_save_notes(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    dash->notes.is_saved = true;
    dash->notes.last_modified = get_time_ms();
    
    printk("[DASHBOARD] Notes saved\n");
    
    return 0;
}

/**
 * dashboard_load_notes - Load notes
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_load_notes(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    /* In real implementation, would load from file */
    dash->notes.is_saved = true;
    
    return 0;
}

/**
 * dashboard_set_notes_content - Set notes content
 * @dash: Pointer to dashboard structure
 * @content: Notes content
 *
 * Returns: 0 on success
 */
int dashboard_set_notes_content(dashboard_t *dash, const char *content)
{
    if (!dash || !content) {
        return -EINVAL;
    }
    
    strncpy(dash->notes.content, content, sizeof(dash->notes.content) - 1);
    dash->notes.is_saved = false;
    
    return 0;
}

/*===========================================================================*/
/*                         MEDIA                                             */
/*===========================================================================*/

/**
 * dashboard_update_media - Update media
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_update_media(dashboard_t *dash)
{
    (void)dash;
    return 0;
}

/**
 * dashboard_set_media_data - Set media data
 * @dash: Pointer to dashboard structure
 * @data: Media data
 *
 * Returns: 0 on success
 */
int dashboard_set_media_data(dashboard_t *dash, media_data_t *data)
{
    if (!dash || !data) {
        return -EINVAL;
    }
    
    memcpy(&dash->media, data, sizeof(media_data_t));
    
    return 0;
}

/**
 * dashboard_media_play - Media play
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_media_play(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    dash->media.is_playing = true;
    dash->media.is_paused = false;
    
    return 0;
}

/**
 * dashboard_media_pause - Media pause
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_media_pause(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    dash->media.is_playing = false;
    dash->media.is_paused = true;
    
    return 0;
}

/**
 * dashboard_media_stop - Media stop
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_media_stop(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    dash->media.is_playing = false;
    dash->media.is_paused = false;
    
    return 0;
}

/**
 * dashboard_media_next - Media next track
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_media_next(dashboard_t *dash)
{
    (void)dash;
    return 0;
}

/**
 * dashboard_media_previous - Media previous track
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_media_previous(dashboard_t *dash)
{
    (void)dash;
    return 0;
}

/*===========================================================================*/
/*                         LAYOUT                                            */
/*===========================================================================*/

/**
 * dashboard_set_columns - Set column count
 * @dash: Pointer to dashboard structure
 * @columns: Column count
 *
 * Returns: 0 on success
 */
int dashboard_set_columns(dashboard_t *dash, u32 columns)
{
    if (!dash) {
        return -EINVAL;
    }
    
    dash->columns = columns;
    dashboard_arrange_widgets(dash);
    
    return 0;
}

/**
 * dashboard_set_grid_size - Set grid size
 * @dash: Pointer to dashboard structure
 * @size: Grid size
 *
 * Returns: 0 on success
 */
int dashboard_set_grid_size(dashboard_t *dash, u32 size)
{
    if (!dash) {
        return -EINVAL;
    }
    
    dash->grid_size = size;
    
    return 0;
}

/**
 * dashboard_snap_widgets - Snap widgets to grid
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_snap_widgets(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    widget_item_t *widget = dash->widgets;
    while (widget) {
        widget->x = (widget->x / dash->grid_size) * dash->grid_size;
        widget->y = (widget->y / dash->grid_size) * dash->grid_size;
        widget = widget->next;
    }
    
    return 0;
}

/**
 * dashboard_arrange_widgets - Arrange widgets
 * @dash: Pointer to dashboard structure
 *
 * Returns: 0 on success
 */
int dashboard_arrange_widgets(dashboard_t *dash)
{
    if (!dash) {
        return -EINVAL;
    }
    
    /* In real implementation, would auto-arrange widgets */
    
    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

static void on_widget_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    widget_item_t *item = (widget_item_t *)widget->user_data;
    dashboard_t *dash = &g_dashboard;
    
    if (dash->on_widget_added) {
        dash->on_widget_added(item);
    }
}

static void on_widget_collapse(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    widget_item_t *item = (widget_item_t *)widget->user_data;
    
    if (item->on_collapse) {
        item->on_collapse(item);
    }
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * dashboard_get_widget_type_name - Get widget type name
 * @type: Widget type
 *
 * Returns: Type name string
 */
const char *dashboard_get_widget_type_name(u32 type)
{
    switch (type) {
        case WIDGET_TYPE_SYSTEM:    return "System";
        case WIDGET_TYPE_WEATHER:   return "Weather";
        case WIDGET_TYPE_CALENDAR:  return "Calendar";
        case WIDGET_TYPE_NOTES:     return "Notes";
        case WIDGET_TYPE_CLOCK:     return "Clock";
        case WIDGET_TYPE_MEDIA:     return "Media";
        case WIDGET_TYPE_SHORTCUTS: return "Shortcuts";
        default:                    return "Unknown";
    }
}

/**
 * dashboard_get_instance - Get global dashboard instance
 *
 * Returns: Pointer to global instance
 */
dashboard_t *dashboard_get_instance(void)
{
    return &g_dashboard;
}
