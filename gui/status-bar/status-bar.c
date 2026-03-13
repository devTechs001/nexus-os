/*
 * NEXUS OS - Status Bar & System Tray Implementation
 * gui/status-bar/status-bar.c
 *
 * Modern status bar with system tray, clock, and quick settings
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "status-bar.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL STATUS BAR INSTANCE                        */
/*===========================================================================*/

static status_bar_t g_status_bar;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int create_status_bar_ui(status_bar_t *bar);
static int create_left_items(status_bar_t *bar);
static int create_center_items(status_bar_t *bar);
static int create_right_items(status_bar_t *bar);
static int create_quick_settings(status_bar_t *bar);

static void update_clock_display(status_bar_t *bar);
static void update_network_display(status_bar_t *bar);
static void update_battery_display(status_bar_t *bar);
static void update_volume_display(status_bar_t *bar);

static void on_clock_clicked(widget_t *widget);
static void on_network_clicked(widget_t *widget);
static void on_volume_clicked(widget_t *widget);
static void on_wifi_toggle(widget_t *widget);
static void on_bluetooth_toggle(widget_t *widget);
static void on_airplane_toggle(widget_t *widget);

/*===========================================================================*/
/*                         STATUS BAR LIFECYCLE                              */
/*===========================================================================*/

/**
 * status_bar_init - Initialize status bar
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int status_bar_init(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    printk("[STATUS-BAR] ========================================\n");
    printk("[STATUS-BAR] NEXUS OS Status Bar v%s\n", STATUS_BAR_VERSION);
    printk("[STATUS-BAR] ========================================\n");
    printk("[STATUS-BAR] Initializing status bar...\n");
    
    /* Clear structure */
    memset(bar, 0, sizeof(status_bar_t));
    bar->initialized = true;
    bar->visible = true;
    
    /* Default settings */
    bar->show_seconds = false;
    bar->show_date = true;
    bar->use_24hour = false;
    bar->wifi_enabled = true;
    bar->bluetooth_enabled = false;
    bar->airplane_mode = false;
    bar->dnd_enabled = false;
    bar->night_light = false;
    
    /* Create UI */
    if (create_status_bar_ui(bar) != 0) {
        printk("[STATUS-BAR] Failed to create UI\n");
        return -1;
    }
    
    /* Create quick settings panel */
    create_quick_settings(bar);
    
    /* Initialize status */
    bar->network.connected = false;
    bar->network.signal_strength = 0;
    bar->battery.present = false;
    bar->battery.percentage = 100;
    bar->volume.level = 75;
    bar->volume.muted = false;
    bar->brightness.level = 80;
    
    /* Start clock timer */
    update_clock_display(bar);
    
    printk("[STATUS-BAR] Status bar initialized\n");
    printk("[STATUS-BAR] ========================================\n");
    
    return 0;
}

/**
 * status_bar_show - Show status bar
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_show(status_bar_t *bar)
{
    if (!bar || !bar->initialized) {
        return -EINVAL;
    }
    
    if (bar->bar_window) {
        window_show(bar->bar_window);
        bar->visible = true;
    }
    
    return 0;
}

/**
 * status_bar_hide - Hide status bar
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_hide(status_bar_t *bar)
{
    if (!bar || !bar->initialized) {
        return -EINVAL;
    }
    
    if (bar->bar_window) {
        window_hide(bar->bar_window);
        bar->visible = false;
    }
    
    return 0;
}

/**
 * status_bar_toggle - Toggle status bar visibility
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_toggle(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    if (bar->visible) {
        return status_bar_hide(bar);
    } else {
        return status_bar_show(bar);
    }
}

/**
 * status_bar_shutdown - Shutdown status bar
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_shutdown(status_bar_t *bar)
{
    if (!bar || !bar->initialized) {
        return -EINVAL;
    }
    
    if (bar->bar_window) {
        window_hide(bar->bar_window);
    }
    
    bar->initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_status_bar_ui - Create status bar UI
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_status_bar_ui(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    /* Create status bar window */
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "Status Bar");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_BORDERLESS;
    props.bounds.x = 0;
    props.bounds.y = 0;
    props.bounds.width = 1920;
    props.bounds.height = STATUS_BAR_HEIGHT;
    props.background = color_from_rgba(30, 30, 45, 255);
    
    bar->bar_window = window_create(&props);
    if (!bar->bar_window) {
        return -1;
    }
    
    /* Create status bar widget */
    bar->bar_widget = panel_create(NULL, 0, 0, 1920, STATUS_BAR_HEIGHT);
    if (!bar->bar_widget) {
        return -1;
    }
    
    widget_set_colors(bar->bar_widget,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(30, 30, 45, 255),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create sections */
    create_left_items(bar);
    create_center_items(bar);
    create_right_items(bar);
    
    return 0;
}

/**
 * create_left_items - Create left side items
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
static int create_left_items(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    /* Activities button */
    widget_t *activities = button_create(bar->bar_widget, "Activities", 10, 4, 80, 24);
    if (activities) {
        widget_set_colors(activities,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(50, 50, 70, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    return 0;
}

/**
 * create_center_items - Create center items
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
static int create_center_items(status_bar_t *bar)
{
    (void)bar;
    /* Center items (optional) */
    return 0;
}

/**
 * create_right_items - Create right side items
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
static int create_right_items(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    s32 x = 1920 - 300;
    
    /* Network indicator */
    widget_t *network = button_create(bar->bar_widget, "🌐", x, 4, 32, 24);
    if (network) {
        network->user_data = bar;
        network->on_click = on_network_clicked;
        widget_set_colors(network,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(50, 50, 70, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    x += 40;
    
    /* Volume indicator */
    widget_t *volume = button_create(bar->bar_widget, "🔊", x, 4, 32, 24);
    if (volume) {
        volume->user_data = bar;
        volume->on_click = on_volume_clicked;
        widget_set_colors(volume,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(50, 50, 70, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    x += 40;
    
    /* Battery indicator */
    widget_t *battery = button_create(bar->bar_widget, "🔋 100%", x, 4, 60, 24);
    if (battery) {
        widget_set_colors(battery,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(50, 50, 70, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    x += 70;
    
    /* Clock */
    widget_t *clock = button_create(bar->bar_widget, "12:00 PM", x, 4, 80, 24);
    if (clock) {
        clock->user_data = bar;
        clock->on_click = on_clock_clicked;
        widget_set_colors(clock,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(50, 50, 70, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    return 0;
}

/**
 * create_quick_settings - Create quick settings panel
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
static int create_quick_settings(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    /* Create quick settings panel */
    bar->quick_settings_panel = panel_create(bar->bar_window, 1800, 32, 120, 200);
    if (!bar->quick_settings_panel) {
        return -1;
    }
    
    widget_set_colors(bar->quick_settings_panel,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 55, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* Initially hidden */
    widget_hide(bar->quick_settings_panel);
    
    /* WiFi toggle */
    widget_t *wifi_toggle = button_create(bar->quick_settings_panel, "📶 WiFi", 10, 10, 100, 35);
    if (wifi_toggle) {
        wifi_toggle->user_data = bar;
        wifi_toggle->on_click = on_wifi_toggle;
        widget_set_colors(wifi_toggle,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(0, 120, 215, 255),
                          color_from_rgba(0, 100, 180, 255));
    }
    
    /* Bluetooth toggle */
    widget_t *bt_toggle = button_create(bar->quick_settings_panel, "Bluetooth", 10, 55, 100, 35);
    if (bt_toggle) {
        bt_toggle->user_data = bar;
        bt_toggle->on_click = on_bluetooth_toggle;
        widget_set_colors(bt_toggle,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(60, 60, 80, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    /* Airplane mode toggle */
    widget_t *airplane_toggle = button_create(bar->quick_settings_panel, "✈ Airplane", 10, 100, 100, 35);
    if (airplane_toggle) {
        airplane_toggle->user_data = bar;
        airplane_toggle->on_click = on_airplane_toggle;
        widget_set_colors(airplane_toggle,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(60, 60, 80, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    return 0;
}

/*===========================================================================*/
/*                         DISPLAY UPDATES                                   */
/*===========================================================================*/

/**
 * update_clock_display - Update clock display
 * @bar: Pointer to status bar structure
 */
static void update_clock_display(status_bar_t *bar)
{
    if (!bar) return;
    
    /* Get current time */
    u64 now = get_time_ms();
    u32 total_seconds = (u32)(now / 1000);
    
    bar->second = total_seconds % 60;
    bar->minute = (total_seconds / 60) % 60;
    bar->hour = (total_seconds / 3600) % 24;
    bar->day = ((total_seconds / 86400) % 30) + 1;
    bar->month = ((total_seconds / 2592000) % 12) + 1;
    bar->year = 2026;
    
    /* Format time string */
    char time_str[32];
    if (bar->use_24hour) {
        if (bar->show_seconds) {
            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                     bar->hour, bar->minute, bar->second);
        } else {
            snprintf(time_str, sizeof(time_str), "%02d:%02d",
                     bar->hour, bar->minute);
        }
    } else {
        const char *ampm = bar->hour >= 12 ? "PM" : "AM";
        u32 hour_12 = bar->hour % 12;
        if (hour_12 == 0) hour_12 = 12;
        snprintf(time_str, sizeof(time_str), "%d:%02d %s",
                 hour_12, bar->minute, ampm);
    }
    
    /* Update clock widget */
    /* In real implementation, would update widget text */
    
    printk("[STATUS-BAR] Time: %s\n", time_str);
}

/**
 * update_network_display - Update network display
 * @bar: Pointer to status bar structure
 */
static void update_network_display(status_bar_t *bar)
{
    if (!bar) return;
    
    /* Update network indicator based on status */
    if (bar->network.connected) {
        /* Show connected icon with signal strength */
    } else {
        /* Show disconnected icon */
    }
}

/**
 * update_battery_display - Update battery display
 * @bar: Pointer to status bar structure
 */
static void update_battery_display(status_bar_t *bar)
{
    if (!bar) return;
    
    /* Update battery indicator */
    if (bar->battery.present) {
        /* Show battery level */
    }
}

/**
 * update_volume_display - Update volume display
 * @bar: Pointer to status bar structure
 */
static void update_volume_display(status_bar_t *bar)
{
    if (!bar) return;
    
    /* Update volume indicator */
    if (bar->volume.muted) {
        /* Show muted icon */
    } else {
        /* Show volume level icon */
    }
}

/*===========================================================================*/
/*                         STATUS UPDATES                                    */
/*===========================================================================*/

/**
 * status_bar_update_network - Update network status
 * @bar: Pointer to status bar structure
 * @status: Network status
 *
 * Returns: 0 on success
 */
int status_bar_update_network(status_bar_t *bar, network_status_t *status)
{
    if (!bar || !status) {
        return -EINVAL;
    }
    
    memcpy(&bar->network, status, sizeof(network_status_t));
    update_network_display(bar);
    
    return 0;
}

/**
 * status_bar_update_battery - Update battery status
 * @bar: Pointer to status bar structure
 * @status: Battery status
 *
 * Returns: 0 on success
 */
int status_bar_update_battery(status_bar_t *bar, battery_status_t *status)
{
    if (!bar || !status) {
        return -EINVAL;
    }
    
    memcpy(&bar->battery, status, sizeof(battery_status_t));
    update_battery_display(bar);
    
    return 0;
}

/**
 * status_bar_update_volume - Update volume status
 * @bar: Pointer to status bar structure
 * @status: Volume status
 *
 * Returns: 0 on success
 */
int status_bar_update_volume(status_bar_t *bar, volume_status_t *status)
{
    if (!bar || !status) {
        return -EINVAL;
    }
    
    memcpy(&bar->volume, status, sizeof(volume_status_t));
    update_volume_display(bar);
    
    return 0;
}

/**
 * status_bar_update_brightness - Update brightness status
 * @bar: Pointer to status bar structure
 * @status: Brightness status
 *
 * Returns: 0 on success
 */
int status_bar_update_brightness(status_bar_t *bar, brightness_status_t *status)
{
    if (!bar || !status) {
        return -EINVAL;
    }
    
    memcpy(&bar->brightness, status, sizeof(brightness_status_t));
    
    return 0;
}

/**
 * status_bar_update_time - Update time
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_update_time(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    update_clock_display(bar);
    
    return 0;
}

/*===========================================================================*/
/*                         QUICK SETTINGS                                    */
/*===========================================================================*/

/**
 * status_bar_toggle_quick_settings - Toggle quick settings
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_toggle_quick_settings(status_bar_t *bar)
{
    if (!bar || !bar->quick_settings_panel) {
        return -EINVAL;
    }
    
    if (bar->quick_settings_visible) {
        widget_hide(bar->quick_settings_panel);
        bar->quick_settings_visible = false;
    } else {
        widget_show(bar->quick_settings_panel);
        bar->quick_settings_visible = true;
    }
    
    return 0;
}

/**
 * status_bar_show_quick_settings - Show quick settings
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_show_quick_settings(status_bar_t *bar)
{
    if (!bar || !bar->quick_settings_panel) {
        return -EINVAL;
    }
    
    widget_show(bar->quick_settings_panel);
    bar->quick_settings_visible = true;
    
    return 0;
}

/**
 * status_bar_hide_quick_settings - Hide quick settings
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_hide_quick_settings(status_bar_t *bar)
{
    if (!bar || !bar->quick_settings_panel) {
        return -EINVAL;
    }
    
    widget_hide(bar->quick_settings_panel);
    bar->quick_settings_visible = false;
    
    return 0;
}

/**
 * status_bar_toggle_wifi - Toggle WiFi
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_toggle_wifi(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    bar->wifi_enabled = !bar->wifi_enabled;
    
    printk("[STATUS-BAR] WiFi %s\n", bar->wifi_enabled ? "enabled" : "disabled");
    
    if (bar->on_settings_changed) {
        bar->on_settings_changed();
    }
    
    return 0;
}

/**
 * status_bar_toggle_bluetooth - Toggle Bluetooth
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_toggle_bluetooth(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    bar->bluetooth_enabled = !bar->bluetooth_enabled;
    
    printk("[STATUS-BAR] Bluetooth %s\n", bar->bluetooth_enabled ? "enabled" : "disabled");
    
    if (bar->on_settings_changed) {
        bar->on_settings_changed();
    }
    
    return 0;
}

/**
 * status_bar_toggle_airplane_mode - Toggle airplane mode
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_toggle_airplane_mode(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    bar->airplane_mode = !bar->airplane_mode;
    
    if (bar->airplane_mode) {
        bar->wifi_enabled = false;
        bar->bluetooth_enabled = false;
    }
    
    printk("[STATUS-BAR] Airplane mode %s\n", bar->airplane_mode ? "enabled" : "disabled");
    
    if (bar->on_settings_changed) {
        bar->on_settings_changed();
    }
    
    return 0;
}

/**
 * status_bar_toggle_dnd - Toggle Do Not Disturb
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_toggle_dnd(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    bar->dnd_enabled = !bar->dnd_enabled;
    
    printk("[STATUS-BAR] Do Not Disturb %s\n", bar->dnd_enabled ? "enabled" : "disabled");
    
    return 0;
}

/**
 * status_bar_toggle_night_light - Toggle night light
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_toggle_night_light(status_bar_t *bar)
{
    if (!bar) {
        return -EINVAL;
    }
    
    bar->night_light = !bar->night_light;
    
    printk("[STATUS-BAR] Night light %s\n", bar->night_light ? "enabled" : "disabled");
    
    return 0;
}

/**
 * status_bar_set_volume - Set volume level
 * @bar: Pointer to status bar structure
 * @level: Volume level (0-100)
 *
 * Returns: 0 on success
 */
int status_bar_set_volume(status_bar_t *bar, u32 level)
{
    if (!bar) {
        return -EINVAL;
    }
    
    if (level > 100) level = 100;
    bar->volume.level = level;
    bar->volume.muted = (level == 0);
    
    update_volume_display(bar);
    
    return 0;
}

/**
 * status_bar_set_brightness - Set brightness level
 * @bar: Pointer to status bar structure
 * @level: Brightness level (0-100)
 *
 * Returns: 0 on success
 */
int status_bar_set_brightness(status_bar_t *bar, u32 level)
{
    if (!bar) {
        return -EINVAL;
    }
    
    if (level > 100) level = 100;
    bar->brightness.level = level;
    
    return 0;
}

/*===========================================================================*/
/*                         SYSTEM ACTIONS                                    */
/*===========================================================================*/

/**
 * status_bar_lock - Lock system
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_lock(status_bar_t *bar)
{
    (void)bar;
    
    printk("[STATUS-BAR] Locking system\n");
    
    /* In real implementation, would lock session */
    
    return 0;
}

/**
 * status_bar_logout - Logout user
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_logout(status_bar_t *bar)
{
    (void)bar;
    
    printk("[STATUS-BAR] Logging out\n");
    
    /* In real implementation, would end session */
    
    return 0;
}

/**
 * status_bar_shutdown_system - Shutdown system
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_shutdown_system(status_bar_t *bar)
{
    (void)bar;
    
    printk("[STATUS-BAR] Shutting down system\n");
    
    /* In real implementation, would shutdown system */
    
    return 0;
}

/**
 * status_bar_restart_system - Restart system
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_restart_system(status_bar_t *bar)
{
    (void)bar;
    
    printk("[STATUS-BAR] Restarting system\n");
    
    /* In real implementation, would restart system */
    
    return 0;
}

/**
 * status_bar_sleep - Put system to sleep
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_sleep(status_bar_t *bar)
{
    (void)bar;
    
    printk("[STATUS-BAR] System sleeping\n");
    
    /* In real implementation, would put system to sleep */
    
    return 0;
}

/**
 * status_bar_hibernate - Hibernate system
 * @bar: Pointer to status bar structure
 *
 * Returns: 0 on success
 */
int status_bar_hibernate(status_bar_t *bar)
{
    (void)bar;
    
    printk("[STATUS-BAR] System hibernating\n");
    
    /* In real implementation, would hibernate system */
    
    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

static void on_clock_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    status_bar_t *bar = (status_bar_t *)widget->user_data;
    
    printk("[STATUS-BAR] Clock clicked\n");
    
    /* Toggle calendar/notification panel */
    status_bar_toggle_quick_settings(bar);
}

static void on_network_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    status_bar_t *bar = (status_bar_t *)widget->user_data;
    
    printk("[STATUS-BAR] Network clicked\n");
    
    /* Show network settings */
    status_bar_toggle_quick_settings(bar);
}

static void on_volume_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    status_bar_t *bar = (status_bar_t *)widget->user_data;
    
    printk("[STATUS-BAR] Volume clicked\n");
    
    /* Show volume slider */
    status_bar_toggle_quick_settings(bar);
}

static void on_wifi_toggle(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    status_bar_t *bar = (status_bar_t *)widget->user_data;
    status_bar_toggle_wifi(bar);
}

static void on_bluetooth_toggle(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    status_bar_t *bar = (status_bar_t *)widget->user_data;
    status_bar_toggle_bluetooth(bar);
}

static void on_airplane_toggle(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    status_bar_t *bar = (status_bar_t *)widget->user_data;
    status_bar_toggle_airplane_mode(bar);
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * status_bar_get_instance - Get global status bar instance
 *
 * Returns: Pointer to global instance
 */
status_bar_t *status_bar_get_instance(void)
{
    return &g_status_bar;
}
