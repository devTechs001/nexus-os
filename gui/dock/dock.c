/*
 * NEXUS OS - Application Dock Implementation
 * gui/dock/dock.c
 *
 * Modern application dock with pinned apps and running indicators
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "dock.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL DOCK INSTANCE                              */
/*===========================================================================*/

static dock_t g_dock;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int create_dock_ui(dock_t *dock);
static int render_dock_items(dock_t *dock);
static int update_dock_position(dock_t *dock);

static void on_item_clicked(widget_t *widget);
static void on_item_right_clicked(widget_t *widget, s32 x, s32 y);
static void on_item_hover(widget_t *widget, bool hover);

static dock_item_t *create_default_items(void);

/*===========================================================================*/
/*                         DOCK LIFECYCLE                                    */
/*===========================================================================*/

/**
 * dock_init - Initialize application dock
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int dock_init(dock_t *dock)
{
    if (!dock) {
        return -EINVAL;
    }
    
    printk("[DOCK] ========================================\n");
    printk("[DOCK] NEXUS OS Application Dock v%s\n", DOCK_VERSION);
    printk("[DOCK] ========================================\n");
    printk("[DOCK] Initializing dock...\n");
    
    /* Clear structure */
    memset(dock, 0, sizeof(dock_t));
    dock->initialized = true;
    dock->visible = true;
    
    /* Default settings */
    dock->settings.position = DOCK_POSITION_BOTTOM;
    dock->settings.item_size = DOCK_ITEM_SIZE;
    dock->settings.autohide = false;
    dock->settings.magnification = true;
    dock->settings.magnification_factor = 1.5f;
    dock->settings.opacity = 90;
    dock->settings.blur_background = true;
    dock->settings.background_color = color_from_rgba(30, 30, 45, 200);
    dock->settings.highlight_color = color_from_rgba(0, 120, 215, 255);
    dock->settings.animation_duration = 200;
    dock->settings.show_labels = false;
    dock->settings.show_running_indicator = true;
    dock->settings.show_all_windows = true;
    
    /* Create UI */
    if (create_dock_ui(dock) != 0) {
        printk("[DOCK] Failed to create UI\n");
        return -1;
    }
    
    /* Create default items */
    dock->items = create_default_items();
    dock->item_count = 0;
    
    /* Count items */
    dock_item_t *item = dock->items;
    while (item) {
        dock->item_count++;
        item = item->next;
    }
    
    /* Render items */
    render_dock_items(dock);
    
    printk("[DOCK] Dock initialized with %d items\n", dock->item_count);
    printk("[DOCK] ========================================\n");
    
    return 0;
}

/**
 * dock_show - Show dock
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success
 */
int dock_show(dock_t *dock)
{
    if (!dock || !dock->initialized) {
        return -EINVAL;
    }
    
    if (dock->dock_window) {
        window_show(dock->dock_window);
        dock->visible = true;
    }
    
    return 0;
}

/**
 * dock_hide - Hide dock
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success
 */
int dock_hide(dock_t *dock)
{
    if (!dock || !dock->initialized) {
        return -EINVAL;
    }
    
    if (dock->dock_window) {
        window_hide(dock->dock_window);
        dock->visible = false;
    }
    
    return 0;
}

/**
 * dock_toggle - Toggle dock visibility
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success
 */
int dock_toggle(dock_t *dock)
{
    if (!dock) {
        return -EINVAL;
    }
    
    if (dock->visible) {
        return dock_hide(dock);
    } else {
        return dock_show(dock);
    }
}

/**
 * dock_shutdown - Shutdown dock
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success
 */
int dock_shutdown(dock_t *dock)
{
    if (!dock || !dock->initialized) {
        return -EINVAL;
    }
    
    /* Free items */
    dock_item_t *item = dock->items;
    while (item) {
        dock_item_t *next = item->next;
        kfree(item);
        item = next;
    }
    
    if (dock->dock_window) {
        window_hide(dock->dock_window);
    }
    
    dock->initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_dock_ui - Create dock UI
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_dock_ui(dock_t *dock)
{
    if (!dock) {
        return -EINVAL;
    }
    
    /* Calculate dock dimensions */
    u32 dock_width = 1920;  /* Full width for bottom position */
    u32 dock_height = DOCK_HEIGHT;
    u32 dock_x = 0;
    u32 dock_y = 1080 - dock_height;
    
    /* Create dock window */
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "Dock");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_BORDERLESS;
    props.bounds.x = dock_x;
    props.bounds.y = dock_y;
    props.bounds.width = dock_width;
    props.bounds.height = dock_height;
    props.background = dock->settings.background_color;
    
    dock->dock_window = window_create(&props);
    if (!dock->dock_window) {
        return -1;
    }
    
    /* Create dock widget */
    dock->dock_widget = panel_create(NULL, 0, 0, dock_width, dock_height);
    if (!dock->dock_widget) {
        return -1;
    }
    
    widget_set_colors(dock->dock_widget,
                      color_from_rgba(255, 255, 255, 255),
                      dock->settings.background_color,
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * render_dock_items - Render dock items
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success
 */
static int render_dock_items(dock_t *dock)
{
    if (!dock || !dock->dock_widget) {
        return -EINVAL;
    }
    
    /* Clear existing item widgets */
    /* In real implementation, would remove old widgets */
    
    /* Render items */
    u32 item_count = 0;
    dock_item_t *item = dock->items;
    s32 x = 10;
    s32 y = (DOCK_HEIGHT - dock->settings.item_size) / 2;
    
    while (item) {
        /* Create item widget */
        widget_t *item_widget = panel_create(dock->dock_widget, x, y, 
                                              dock->settings.item_size, 
                                              dock->settings.item_size);
        if (item_widget) {
            widget_set_colors(item_widget,
                              color_from_rgba(255, 255, 255, 255),
                              color_from_rgba(0, 0, 0, 0),
                              color_from_rgba(0, 0, 0, 0));
            
            item_widget->user_data = item;
            item_widget->on_click = on_item_clicked;
            item_widget->on_mouse_enter = on_item_hover;
            item_widget->on_mouse_leave = on_item_hover;
        }
        
        x += dock->settings.item_size + 10;
        item_count++;
        item = item->next;
    }
    
    return 0;
}

/**
 * update_dock_position - Update dock position
 * @dock: Pointer to dock structure
 *
 * Returns: 0 on success
 */
static int update_dock_position(dock_t *dock)
{
    if (!dock || !dock->dock_window) {
        return -EINVAL;
    }
    
    u32 dock_x = 0, dock_y = 0;
    u32 dock_width = 0, dock_height = DOCK_HEIGHT;
    
    switch (dock->settings.position) {
        case DOCK_POSITION_BOTTOM:
            dock_x = 0;
            dock_y = 1080 - dock_height;
            dock_width = 1920;
            break;
        case DOCK_POSITION_LEFT:
            dock_x = 0;
            dock_y = 0;
            dock_width = dock_height;
            dock_height = 1080;
            break;
        case DOCK_POSITION_RIGHT:
            dock_x = 1920 - dock_height;
            dock_y = 0;
            dock_width = dock_height;
            dock_height = 1080;
            break;
        case DOCK_POSITION_TOP:
            dock_x = 0;
            dock_y = 0;
            dock_width = 1920;
            break;
    }
    
    window_set_bounds(dock->dock_window, dock_x, dock_y, dock_width, dock_height);
    
    return 0;
}

/*===========================================================================*/
/*                         ITEM MANAGEMENT                                   */
/*===========================================================================*/

/**
 * create_default_items - Create default dock items
 *
 * Returns: Pointer to first item
 */
static dock_item_t *create_default_items(void)
{
    /* Create default dock items */
    dock_item_t *items = NULL;
    dock_item_t *last = NULL;
    
    /* File Explorer */
    dock_item_t *explorer = kzalloc(sizeof(dock_item_t));
    if (explorer) {
        explorer->item_id = 1;
        strcpy(explorer->name, "File Explorer");
        strcpy(explorer->icon_path, "/usr/share/icons/file-explorer.png");
        strcpy(explorer->app_path, "/usr/bin/file-explorer");
        strcpy(explorer->tooltip, "File Explorer");
        explorer->is_pinned = true;
        explorer->type = DOCK_ITEM_APP;
        
        if (!items) {
            items = explorer;
            last = explorer;
        } else {
            last->next = explorer;
            last = explorer;
        }
    }
    
    /* Terminal */
    dock_item_t *terminal = kzalloc(sizeof(dock_item_t));
    if (terminal) {
        terminal->item_id = 2;
        strcpy(terminal->name, "Terminal");
        strcpy(terminal->icon_path, "/usr/share/icons/terminal.png");
        strcpy(terminal->app_path, "/usr/bin/terminal");
        strcpy(terminal->tooltip, "Terminal");
        terminal->is_pinned = true;
        terminal->type = DOCK_ITEM_APP;
        
        last->next = terminal;
        last = terminal;
    }
    
    /* App Store */
    dock_item_t *appstore = kzalloc(sizeof(dock_item_t));
    if (appstore) {
        appstore->item_id = 3;
        strcpy(appstore->name, "App Store");
        strcpy(appstore->icon_path, "/usr/share/icons/app-store.png");
        strcpy(appstore->app_path, "/usr/bin/app-store");
        strcpy(appstore->tooltip, "App Store");
        appstore->is_pinned = true;
        appstore->type = DOCK_ITEM_APP;
        
        last->next = appstore;
        last = appstore;
    }
    
    /* Settings */
    dock_item_t *settings = kzalloc(sizeof(dock_item_t));
    if (settings) {
        settings->item_id = 4;
        strcpy(settings->name, "Settings");
        strcpy(settings->icon_path, "/usr/share/icons/settings.png");
        strcpy(settings->app_path, "/usr/bin/settings");
        strcpy(settings->tooltip, "System Settings");
        settings->is_pinned = true;
        settings->type = DOCK_ITEM_APP;
        
        last->next = settings;
        last = settings;
    }
    
    /* Separator */
    dock_item_t *separator = kzalloc(sizeof(dock_item_t));
    if (separator) {
        separator->item_id = 5;
        separator->type = DOCK_ITEM_SEPARATOR;
        
        last->next = separator;
        last = separator;
    }
    
    /* Storage Manager */
    dock_item_t *storage = kzalloc(sizeof(dock_item_t));
    if (storage) {
        storage->item_id = 6;
        strcpy(storage->name, "Storage");
        strcpy(storage->icon_path, "/usr/share/icons/storage.png");
        strcpy(storage->app_path, "/usr/bin/storage-manager");
        strcpy(storage->tooltip, "Storage Manager");
        storage->is_pinned = true;
        storage->type = DOCK_ITEM_APP;
        
        last->next = storage;
        last = storage;
    }
    
    return items;
}

/**
 * dock_add_item - Add item to dock
 * @dock: Pointer to dock structure
 * @item: Item to add
 *
 * Returns: Pointer to added item, or NULL on failure
 */
dock_item_t *dock_add_item(dock_t *dock, dock_item_t *item)
{
    if (!dock || !item) {
        return NULL;
    }
    
    /* Add to end of list */
    if (!dock->items) {
        dock->items = item;
    } else {
        dock_item_t *last = dock->items;
        while (last->next) {
            last = last->next;
        }
        last->next = item;
    }
    
    dock->item_count++;
    
    /* Re-render */
    render_dock_items(dock);
    
    return item;
}

/**
 * dock_remove_item - Remove item from dock
 * @dock: Pointer to dock structure
 * @item_id: Item ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int dock_remove_item(dock_t *dock, u32 item_id)
{
    if (!dock) {
        return -EINVAL;
    }
    
    dock_item_t **prev = &dock->items;
    dock_item_t *curr = dock->items;
    
    while (curr) {
        if (curr->item_id == item_id) {
            *prev = curr->next;
            kfree(curr);
            dock->item_count--;
            
            /* Re-render */
            render_dock_items(dock);
            
            return 0;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    return -ENOENT;
}

/**
 * dock_get_item - Get item by ID
 * @dock: Pointer to dock structure
 * @item_id: Item ID
 *
 * Returns: Item pointer, or NULL if not found
 */
dock_item_t *dock_get_item(dock_t *dock, u32 item_id)
{
    if (!dock) {
        return NULL;
    }
    
    dock_item_t *item = dock->items;
    while (item) {
        if (item->item_id == item_id) {
            return item;
        }
        item = item->next;
    }
    
    return NULL;
}

/**
 * dock_pin_item - Pin item to dock
 * @dock: Pointer to dock structure
 * @item_id: Item ID
 *
 * Returns: 0 on success
 */
int dock_pin_item(dock_t *dock, u32 item_id)
{
    dock_item_t *item = dock_get_item(dock, item_id);
    if (item) {
        item->is_pinned = true;
    }
    return 0;
}

/**
 * dock_unpin_item - Unpin item from dock
 * @dock: Pointer to dock structure
 * @item_id: Item ID
 *
 * Returns: 0 on success
 */
int dock_unpin_item(dock_t *dock, u32 item_id)
{
    dock_item_t *item = dock_get_item(dock, item_id);
    if (item && !item->is_running) {
        dock_remove_item(dock, item_id);
    } else if (item) {
        item->is_pinned = false;
    }
    return 0;
}

/*===========================================================================*/
/*                         APP STATUS                                        */
/*===========================================================================*/

/**
 * dock_set_app_running - Set app running status
 * @dock: Pointer to dock structure
 * @app_path: Application path
 * @running: Running status
 *
 * Returns: 0 on success
 */
int dock_set_app_running(dock_t *dock, const char *app_path, bool running)
{
    if (!dock || !app_path) {
        return -EINVAL;
    }
    
    dock_item_t *item = dock->items;
    while (item) {
        if (strcmp(item->app_path, app_path) == 0) {
            item->is_running = running;
            
            /* Show/hide running indicator */
            /* In real implementation, would update indicator */
            
            break;
        }
        item = item->next;
    }
    
    return 0;
}

/**
 * dock_set_app_active - Set app active status
 * @dock: Pointer to dock structure
 * @app_path: Application path
 * @active: Active status
 *
 * Returns: 0 on success
 */
int dock_set_app_active(dock_t *dock, const char *app_path, bool active)
{
    if (!dock || !app_path) {
        return -EINVAL;
    }
    
    dock_item_t *item = dock->items;
    while (item) {
        if (strcmp(item->app_path, app_path) == 0) {
            item->is_active = active;
            break;
        }
        item = item->next;
    }
    
    return 0;
}

/**
 * dock_set_app_urgent - Set app urgent status
 * @dock: Pointer to dock structure
 * @app_path: Application path
 * @urgent: Urgent status
 *
 * Returns: 0 on success
 */
int dock_set_app_urgent(dock_t *dock, const char *app_path, bool urgent)
{
    if (!dock || !app_path) {
        return -EINVAL;
    }
    
    dock_item_t *item = dock->items;
    while (item) {
        if (strcmp(item->app_path, app_path) == 0) {
            item->is_urgent = urgent;
            break;
        }
        item = item->next;
    }
    
    return 0;
}

/**
 * dock_set_app_window_count - Set app window count
 * @dock: Pointer to dock structure
 * @app_path: Application path
 * @count: Window count
 *
 * Returns: 0 on success
 */
int dock_set_app_window_count(dock_t *dock, const char *app_path, u32 count)
{
    if (!dock || !app_path) {
        return -EINVAL;
    }
    
    dock_item_t *item = dock->items;
    while (item) {
        if (strcmp(item->app_path, app_path) == 0) {
            item->window_count = count;
            break;
        }
        item = item->next;
    }
    
    return 0;
}

/*===========================================================================*/
/*                         SETTINGS                                          */
/*===========================================================================*/

/**
 * dock_set_position - Set dock position
 * @dock: Pointer to dock structure
 * @position: Position
 *
 * Returns: 0 on success
 */
int dock_set_position(dock_t *dock, u32 position)
{
    if (!dock) {
        return -EINVAL;
    }
    
    dock->settings.position = position;
    update_dock_position(dock);
    
    return 0;
}

/**
 * dock_set_autohide - Set autohide
 * @dock: Pointer to dock structure
 * @autohide: Autohide setting
 *
 * Returns: 0 on success
 */
int dock_set_autohide(dock_t *dock, bool autohide)
{
    if (!dock) {
        return -EINVAL;
    }
    
    dock->settings.autohide = autohide;
    
    return 0;
}

/**
 * dock_set_magnification - Set magnification
 * @dock: Pointer to dock structure
 * @enabled: Enabled
 * @factor: Magnification factor
 *
 * Returns: 0 on success
 */
int dock_set_magnification(dock_t *dock, bool enabled, float factor)
{
    if (!dock) {
        return -EINVAL;
    }
    
    dock->settings.magnification = enabled;
    dock->settings.magnification_factor = factor;
    
    return 0;
}

/**
 * dock_set_opacity - Set dock opacity
 * @dock: Pointer to dock structure
 * @opacity: Opacity (0-100)
 *
 * Returns: 0 on success
 */
int dock_set_opacity(dock_t *dock, u32 opacity)
{
    if (!dock) {
        return -EINVAL;
    }
    
    if (opacity > 100) opacity = 100;
    dock->settings.opacity = opacity;
    
    return 0;
}

/**
 * dock_apply_settings - Apply dock settings
 * @dock: Pointer to dock structure
 * @settings: Settings to apply
 *
 * Returns: 0 on success
 */
int dock_apply_settings(dock_t *dock, dock_settings_t *settings)
{
    if (!dock || !settings) {
        return -EINVAL;
    }
    
    memcpy(&dock->settings, settings, sizeof(dock_settings_t));
    
    /* Update UI */
    update_dock_position(dock);
    render_dock_items(dock);
    
    if (dock->on_settings_changed) {
        dock->on_settings_changed(&dock->settings);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

static void on_item_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    dock_item_t *item = (dock_item_t *)widget->user_data;
    dock_t *dock = &g_dock;
    
    printk("[DOCK] Item clicked: %s\n", item->name);
    
    /* Launch or focus app */
    /* In real implementation, would launch/focus application */
    
    if (dock->on_item_clicked) {
        dock->on_item_clicked(item);
    }
}

static void on_item_right_clicked(widget_t *widget, s32 x, s32 y)
{
    if (!widget || !widget->user_data) return;
    
    dock_item_t *item = (dock_item_t *)widget->user_data;
    dock_t *dock = &g_dock;
    
    /* Show context menu */
    if (dock->on_item_right_clicked) {
        dock->on_item_right_clicked(item, x, y);
    }
}

static void on_item_hover(widget_t *widget, bool hover)
{
    if (!widget || !widget->user_data) return;
    
    dock_t *dock = &g_dock;
    
    if (hover) {
        dock->hover_item = (dock_item_t *)widget->user_data;
        
        /* Apply magnification effect */
        if (dock->settings.magnification) {
            /* In real implementation, would animate item size */
        }
    } else {
        dock->hover_item = NULL;
    }
}

/**
 * dock_show_context_menu - Show context menu for item
 * @dock: Pointer to dock structure
 * @item: Dock item
 * @x: X position
 * @y: Y position
 *
 * Returns: 0 on success
 */
int dock_show_context_menu(dock_t *dock, dock_item_t *item, s32 x, s32 y)
{
    if (!dock || !item) {
        return -EINVAL;
    }
    
    printk("[DOCK] Context menu for: %s\n", item->name);
    
    /* In real implementation, would show context menu with options:
     * - Open
     * - Pin to dock / Unpin from dock
     * - Close all windows
     * - Properties
     */
    
    return 0;
}

/**
 * dock_add_context_action - Add context action to item
 * @item: Dock item
 * @name: Action name
 * @callback: Callback function
 *
 * Returns: 0 on success
 */
int dock_add_context_action(dock_item_t *item, const char *name, void (*callback)(dock_item_t *))
{
    (void)item; (void)name; (void)callback;
    
    /* In real implementation, would add action to item's context menu */
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * dock_get_position_name - Get position name
 * @position: Position
 *
 * Returns: Position name string
 */
const char *dock_get_position_name(u32 position)
{
    switch (position) {
        case DOCK_POSITION_BOTTOM:  return "Bottom";
        case DOCK_POSITION_LEFT:    return "Left";
        case DOCK_POSITION_RIGHT:   return "Right";
        case DOCK_POSITION_TOP:     return "Top";
        default:                    return "Unknown";
    }
}

/**
 * dock_get_instance - Get global dock instance
 *
 * Returns: Pointer to global instance
 */
dock_t *dock_get_instance(void)
{
    return &g_dock;
}
