/*
 * NEXUS OS - Desktop Environment Implementation
 * gui/desktop/desktop.c
 * 
 * Modern, clean desktop with panels, menus, taskbar, and system tray
 */

#include "desktop.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         GLOBAL DESKTOP INSTANCE                           */
/*===========================================================================*/

static desktop_t g_desktop;

/*===========================================================================*/
/*                         DESKTOP INITIALIZATION                            */
/*===========================================================================*/

/**
 * desktop_init - Initialize desktop environment
 */
int desktop_init(desktop_t *desktop)
{
    if (!desktop) return -1;
    
    printk("[DESKTOP] Initializing desktop environment...\n");
    
    memset(desktop, 0, sizeof(desktop_t));
    
    /* Desktop info */
    strcpy(desktop->name, "NEXUS Desktop");
    desktop->workspace = 0;
    desktop->workspace_count = DESKTOP_MAX_WORKSPACES;
    
    /* Screen (will be updated by compositor) */
    desktop->screen_width = 1920;
    desktop->screen_height = 1080;
    desktop->screen_depth = 32;
    
    /* Background */
    desktop->background_color = DESKTOP_COLOR_BG;
    desktop->wallpaper_mode = 2;  /* FIT */
    
    /* Icons */
    desktop->icons = kmalloc(sizeof(desktop_icon_t) * DESKTOP_MAX_ICONS);
    desktop->icon_count = 0;
    desktop->icon_layout = DESKTOP_LAYOUT_ICON_GRID;
    desktop->icon_grid_size = 80;
    
    /* Panels */
    desktop->panels = kmalloc(sizeof(desktop_panel_t) * DESKTOP_MAX_PANELS);
    desktop->panel_count = 0;
    
    /* Create default taskbar (bottom panel) */
    desktop_panel_t *taskbar = &desktop->panels[desktop->panel_count++];
    strcpy(taskbar->name, "Taskbar");
    taskbar->position = PANEL_POSITION_BOTTOM;
    taskbar->x = 0;
    taskbar->y = desktop->screen_height - DESKTOP_TASKBAR_HEIGHT;
    taskbar->width = desktop->screen_width;
    taskbar->height = DESKTOP_TASKBAR_HEIGHT;
    taskbar->visible = true;
    taskbar->autohide = false;
    taskbar->color = DESKTOP_COLOR_TASKBAR;
    taskbar->show_clock = true;
    taskbar->show_start_button = true;
    taskbar->show_workspaces = true;
    taskbar->current_workspace = 0;
    taskbar->workspace_count = desktop->workspace_count;
    strcpy(taskbar->time_format, "%H:%M");
    strcpy(taskbar->date_format, "%Y-%m-%d");
    
    /* Create top panel (menu bar) */
    desktop_panel_t *menubar = &desktop->panels[desktop->panel_count++];
    strcpy(menubar->name, "Menu Bar");
    menubar->position = PANEL_POSITION_TOP;
    menubar->x = 0;
    menubar->y = 0;
    menubar->width = desktop->screen_width;
    menubar->height = DESKTOP_PANEL_HEIGHT;
    menubar->visible = true;
    menubar->autohide = false;
    menubar->color = DESKTOP_COLOR_PANEL;
    menubar->show_clock = true;
    menubar->show_start_button = false;
    menubar->show_workspaces = false;
    
    /* Start menu */
    desktop->start_menu.visible = false;
    desktop->start_menu.x = 10;
    desktop->start_menu.y = desktop->screen_height - DESKTOP_TASKBAR_HEIGHT - 500;
    desktop->start_menu.width = 400;
    desktop->start_menu.height = 500;
    desktop->start_menu.show_power = true;
    desktop->start_menu.show_sleep = true;
    desktop->start_menu.show_restart = true;
    desktop->start_menu.show_logout = true;
    strcpy(desktop->start_menu.username, "User");
    
    /* Theme */
    strcpy(desktop->theme_name, "NEXUS Dark");
    desktop->accent_color = DESKTOP_COLOR_ACCENT;
    
    /* Settings */
    desktop->show_hidden_files = false;
    desktop->show_file_extensions = true;
    desktop->animations_enabled = true;
    desktop->transparency_enabled = true;
    
    /* Add default desktop icons */
    desktop_icon_create(desktop, "File Explorer", "/apps/file-manager", "/icons/folder.svg", 20, 20);
    desktop_icon_create(desktop, "App Store", "/apps/app-store", "/icons/store.svg", 20, 120);
    desktop_icon_create(desktop, "Settings", "/apps/settings", "/icons/settings.svg", 20, 220);
    desktop_icon_create(desktop, "Terminal", "/apps/terminal", "/icons/terminal.svg", 20, 320);
    desktop_icon_create(desktop, "Browser", "/apps/browser", "/icons/browser.svg", 20, 420);
    
    /* Load settings */
    desktop_load_settings(desktop);
    
    printk("[DESKTOP] Initialized with %d panels, %d icons\n", 
           desktop->panel_count, desktop->icon_count);
    printk("[DESKTOP] Theme: %s, Accent: #%06X\n", 
           desktop->theme_name, desktop->accent_color);
    
    return 0;
}

/**
 * desktop_shutdown - Shutdown desktop environment
 */
int desktop_shutdown(desktop_t *desktop)
{
    if (!desktop) return -1;
    
    printk("[DESKTOP] Shutting down...\n");
    
    desktop_save_settings(desktop);
    
    if (desktop->icons) {
        kfree(desktop->icons);
        desktop->icons = NULL;
    }
    
    if (desktop->panels) {
        kfree(desktop->panels);
        desktop->panels = NULL;
    }
    
    return 0;
}

/**
 * desktop_render - Render desktop
 */
void desktop_render(desktop_t *desktop)
{
    if (!desktop) return;
    
    /* Render background */
    /* compositor_fill(desktop->background_color); */
    
    /* Render wallpaper if set */
    if (desktop->wallpaper_path[0]) {
        /* compositor_draw_wallpaper(desktop->wallpaper_path, desktop->wallpaper_mode); */
    }
    
    /* Render icons */
    for (int i = 0; i < desktop->icon_count; i++) {
        desktop_icon_t *icon = &desktop->icons[i];
        if (icon->hidden) continue;
        
        /* Draw icon background if selected */
        if (icon->selected) {
            /* compositor_draw_rect(icon->x - 4, icon->y - 4, 
                                 icon->width + 8, icon->height + 8,
                                 DESKTOP_COLOR_HOVER); */
        }
        
        /* Draw icon */
        /* compositor_draw_icon(icon->icon_path, icon->x, icon->y, 
                             icon->width, icon->height); */
        
        /* Draw label */
        /* compositor_draw_text(icon->name, icon->x, icon->y + icon->height + 4,
                             DESKTOP_COLOR_TEXT); */
    }
    
    /* Render panels */
    for (int i = 0; i < desktop->panel_count; i++) {
        desktop_panel_t *panel = &desktop->panels[i];
        if (!panel->visible) continue;
        
        /* Draw panel background */
        /* compositor_draw_rect(panel->x, panel->y, panel->width, panel->height,
                             panel->color); */
        
        /* Draw start button */
        if (panel->show_start_button) {
            /* compositor_draw_button(10, panel->y + 4, 40, panel->height - 8,
                                   "NEXUS", DESKTOP_COLOR_ACCENT); */
        }
        
        /* Draw taskbar items */
        for (int j = 0; j < panel->taskbar_count; j++) {
            taskbar_item_t *item = &panel->taskbar_items[j];
            
            u32 color = item->active ? DESKTOP_COLOR_HOVER : panel->color;
            /* compositor_draw_button(60 + j * 200, panel->y + 4, 190, 
                                   panel->height - 8, item->title, color); */
        }
        
        /* Draw system tray */
        int tray_x = panel->width - 200;
        for (int j = 0; j < panel->tray_count; j++) {
            system_tray_item_t *item = &panel->tray_items[j];
            if (!item->visible) continue;
            
            /* compositor_draw_icon(item->icon, tray_x, panel->y + 10, 24, 24); */
            tray_x += 30;
        }
        
        /* Draw clock */
        if (panel->show_clock) {
            /* char time_str[32]; */
            /* get_time_string(time_str, sizeof(time_str), panel->time_format); */
            /* compositor_draw_text(time_str, panel->width - 80, panel->y + 12,
                                 DESKTOP_COLOR_TEXT); */
        }
    }
    
    /* Render start menu if visible */
    if (desktop->start_menu.visible) {
        /* Start menu rendering */
    }
}

/*===========================================================================*/
/*                         ICON MANAGEMENT                                   */
/*===========================================================================*/

/**
 * desktop_icon_create - Create desktop icon
 */
int desktop_icon_create(desktop_t *desktop, const char *name, const char *path, 
                        const char *icon_path, int x, int y)
{
    if (!desktop || !name || desktop->icon_count >= DESKTOP_MAX_ICONS) {
        return -1;
    }
    
    desktop_icon_t *icon = &desktop->icons[desktop->icon_count++];
    memset(icon, 0, sizeof(desktop_icon_t));
    
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    strncpy(icon->path, path, sizeof(icon->path) - 1);
    strncpy(icon->icon_path, icon_path, sizeof(icon->icon_path) - 1);
    icon->x = x;
    icon->y = y;
    icon->width = DESKTOP_ICON_SIZE;
    icon->height = DESKTOP_ICON_SIZE;
    icon->selected = false;
    icon->hidden = false;
    
    /* Default handlers */
    icon->on_click = NULL;
    icon->on_double_click = NULL;
    icon->on_right_click = NULL;
    
    printk("[DESKTOP] Created icon: %s at (%d, %d)\n", name, x, y);
    return desktop->icon_count - 1;
}

/**
 * desktop_icon_add - Add existing icon to desktop
 */
int desktop_icon_add(desktop_t *desktop, desktop_icon_t *icon)
{
    if (!desktop || !icon || desktop->icon_count >= DESKTOP_MAX_ICONS) {
        return -1;
    }
    
    memcpy(&desktop->icons[desktop->icon_count++], icon, sizeof(desktop_icon_t));
    return desktop->icon_count - 1;
}

/**
 * desktop_icon_remove - Remove icon from desktop
 */
int desktop_icon_remove(desktop_t *desktop, int index)
{
    if (!desktop || index < 0 || index >= desktop->icon_count) {
        return -1;
    }
    
    /* Shift remaining icons */
    for (int i = index; i < desktop->icon_count - 1; i++) {
        memcpy(&desktop->icons[i], &desktop->icons[i + 1], sizeof(desktop_icon_t));
    }
    
    desktop->icon_count--;
    return 0;
}

/**
 * desktop_icon_arrange - Auto-arrange icons in grid
 */
int desktop_icon_arrange(desktop_t *desktop)
{
    if (!desktop) return -1;
    
    int grid_size = desktop->icon_grid_size;
    int cols = (desktop->screen_width - 40) / grid_size;
    int row = 0, col = 0;
    
    for (int i = 0; i < desktop->icon_count; i++) {
        desktop_icon_t *icon = &desktop->icons[i];
        if (icon->hidden) continue;
        
        icon->x = 20 + col * grid_size;
        icon->y = 20 + row * grid_size;
        
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }
    
    printk("[DESKTOP] Arranged %d icons in grid\n", desktop->icon_count);
    return 0;
}

/*===========================================================================*/
/*                         PANEL MANAGEMENT                                  */
/*===========================================================================*/

/**
 * desktop_panel_create - Create desktop panel
 */
int desktop_panel_create(desktop_t *desktop, int position)
{
    if (!desktop || desktop->panel_count >= DESKTOP_MAX_PANELS) {
        return -1;
    }
    
    desktop_panel_t *panel = &desktop->panels[desktop->panel_count++];
    memset(panel, 0, sizeof(desktop_panel_t));
    
    panel->position = position;
    panel->visible = true;
    panel->color = DESKTOP_COLOR_PANEL;
    panel->show_clock = true;
    
    /* Set default position and size */
    switch (position) {
    case PANEL_POSITION_TOP:
        panel->x = 0;
        panel->y = 0;
        panel->width = desktop->screen_width;
        panel->height = DESKTOP_PANEL_HEIGHT;
        break;
    case PANEL_POSITION_BOTTOM:
        panel->x = 0;
        panel->y = desktop->screen_height - DESKTOP_PANEL_HEIGHT;
        panel->width = desktop->screen_width;
        panel->height = DESKTOP_PANEL_HEIGHT;
        break;
    case PANEL_POSITION_LEFT:
        panel->x = 0;
        panel->y = 0;
        panel->width = DESKTOP_PANEL_HEIGHT;
        panel->height = desktop->screen_height;
        break;
    case PANEL_POSITION_RIGHT:
        panel->x = desktop->screen_width - DESKTOP_PANEL_HEIGHT;
        panel->y = 0;
        panel->width = DESKTOP_PANEL_HEIGHT;
        panel->height = desktop->screen_height;
        break;
    }
    
    /* Allocate taskbar and tray items */
    panel->taskbar_items = kmalloc(sizeof(taskbar_item_t) * DESKTOP_MAX_WINDOWS);
    panel->taskbar_count = 0;
    panel->tray_items = kmalloc(sizeof(system_tray_item_t) * 32);
    panel->tray_count = 0;
    
    printk("[DESKTOP] Created panel at position %d\n", position);
    return desktop->panel_count - 1;
}

/**
 * desktop_taskbar_add_item - Add item to taskbar
 */
int desktop_taskbar_add_item(desktop_t *desktop, u32 window_id, const char *title, 
                             const char *app_name)
{
    if (!desktop || desktop->panel_count == 0) return -1;
    
    /* Find taskbar panel (bottom panel) */
    desktop_panel_t *taskbar = NULL;
    for (int i = 0; i < desktop->panel_count; i++) {
        if (desktop->panels[i].position == PANEL_POSITION_BOTTOM) {
            taskbar = &desktop->panels[i];
            break;
        }
    }
    
    if (!taskbar || taskbar->taskbar_count >= DESKTOP_MAX_WINDOWS) {
        return -1;
    }
    
    taskbar_item_t *item = &taskbar->taskbar_items[taskbar->taskbar_count++];
    memset(item, 0, sizeof(taskbar_item_t));
    item->window_id = window_id;
    strncpy(item->title, title, sizeof(item->title) - 1);
    strncpy(item->app_name, app_name, sizeof(item->app_name) - 1);
    item->active = true;
    item->minimized = false;
    item->last_active = get_time_ms();
    
    return taskbar->taskbar_count - 1;
}

/**
 * desktop_tray_add_item - Add item to system tray
 */
int desktop_tray_add_item(desktop_t *desktop, system_tray_item_t *item)
{
    if (!desktop || !item || desktop->panel_count == 0) return -1;
    
    /* Find taskbar panel */
    desktop_panel_t *taskbar = NULL;
    for (int i = 0; i < desktop->panel_count; i++) {
        if (desktop->panels[i].position == PANEL_POSITION_BOTTOM) {
            taskbar = &desktop->panels[i];
            break;
        }
    }
    
    if (!taskbar || taskbar->tray_count >= 32) {
        return -1;
    }
    
    memcpy(&taskbar->tray_items[taskbar->tray_count++], item, sizeof(system_tray_item_t));
    return taskbar->tray_count - 1;
}

/*===========================================================================*/
/*                         START MENU                                        */
/*===========================================================================*/

/**
 * desktop_start_menu_toggle - Toggle start menu visibility
 */
int desktop_start_menu_toggle(desktop_t *desktop)
{
    if (!desktop) return -1;
    
    desktop->start_menu.visible = !desktop->start_menu.visible;
    
    if (desktop->start_menu.visible) {
        printk("[DESKTOP] Start menu opened\n");
    } else {
        printk("[DESKTOP] Start menu closed\n");
    }
    
    return 0;
}

/**
 * desktop_start_menu_show - Show start menu
 */
int desktop_start_menu_show(desktop_t *desktop)
{
    if (!desktop) return -1;
    desktop->start_menu.visible = true;
    return 0;
}

/**
 * desktop_start_menu_hide - Hide start menu
 */
int desktop_start_menu_hide(desktop_t *desktop)
{
    if (!desktop) return -1;
    desktop->start_menu.visible = false;
    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLING                                    */
/*===========================================================================*/

/**
 * desktop_handle_mouse_click - Handle mouse click on desktop
 */
int desktop_handle_mouse_click(desktop_t *desktop, int x, int y, int button)
{
    if (!desktop) return -1;
    
    /* Check if clicked on start button */
    if (button == 1) {  /* Left click */
        /* Check start button */
        if (y >= desktop->screen_height - DESKTOP_TASKBAR_HEIGHT && 
            x >= 10 && x <= 60) {
            desktop_start_menu_toggle(desktop);
            return 0;
        }
        
        /* Check desktop icons */
        for (int i = 0; i < desktop->icon_count; i++) {
            desktop_icon_t *icon = &desktop->icons[i];
            if (icon->hidden) continue;
            
            if (x >= icon->x && x <= icon->x + icon->width &&
                y >= icon->y && y <= icon->y + icon->height) {
                
                /* Deselect all other icons */
                for (int j = 0; j < desktop->icon_count; j++) {
                    desktop->icons[j].selected = (j == i);
                }
                
                desktop->selected_icon = icon;
                
                if (icon->on_click) {
                    icon->on_click(icon);
                }
                
                return 0;
            }
        }
        
        /* Clicked on empty desktop - deselect all */
        for (int i = 0; i < desktop->icon_count; i++) {
            desktop->icons[i].selected = false;
        }
        desktop->selected_icon = NULL;
        
        /* Hide start menu */
        if (desktop->start_menu.visible) {
            desktop_start_menu_hide(desktop);
        }
    }
    
    return 0;
}

/**
 * desktop_handle_mouse_double_click - Handle double click
 */
int desktop_handle_mouse_double_click(desktop_t *desktop, int x, int y, int button)
{
    if (!desktop || button != 1) return -1;
    
    /* Check desktop icons */
    for (int i = 0; i < desktop->icon_count; i++) {
        desktop_icon_t *icon = &desktop->icons[i];
        if (icon->hidden) continue;
        
        if (x >= icon->x && x <= icon->x + icon->width &&
            y >= icon->y && y <= icon->y + icon->height) {
            
            if (icon->on_double_click) {
                icon->on_double_click(icon);
            }
            
            return 0;
        }
    }
    
    return 0;
}

/*===========================================================================*/
/*                         SETTINGS                                          */
/*===========================================================================*/

/**
 * desktop_load_settings - Load desktop settings
 */
int desktop_load_settings(desktop_t *desktop)
{
    if (!desktop) return -1;
    
    printk("[DESKTOP] Loading settings...\n");
    
    /* TODO: Load from registry/config file */
    /* For now, use defaults */
    
    return 0;
}

/**
 * desktop_save_settings - Save desktop settings
 */
int desktop_save_settings(desktop_t *desktop)
{
    if (!desktop) return -1;
    
    printk("[DESKTOP] Saving settings...\n");
    
    /* TODO: Save to registry/config file */
    
    return 0;
}

/**
 * desktop_reset_settings - Reset desktop settings to defaults
 */
int desktop_reset_settings(desktop_t *desktop)
{
    if (!desktop) return -1;
    
    printk("[DESKTOP] Resetting settings to defaults...\n");
    
    desktop_shutdown(desktop);
    desktop_init(desktop);
    
    return 0;
}

/*===========================================================================*/
/*                         NOTIFICATIONS                                     */
/*===========================================================================*/

/**
 * desktop_show_notification - Show desktop notification
 */
int desktop_show_notification(desktop_t *desktop, const char *title, 
                              const char *message, int timeout_ms)
{
    if (!desktop || !title || !message) return -1;
    
    printk("[NOTIFY] %s: %s\n", title, message);
    
    /* TODO: Show notification popup in bottom-right corner */
    
    return 0;
}

/*===========================================================================*/
/*                         GLOBAL DESKTOP ACCESS                             */
/*===========================================================================*/

/**
 * desktop_get - Get global desktop instance
 */
desktop_t *desktop_get(void)
{
    return &g_desktop;
}

/**
 * desktop_init_global - Initialize global desktop instance
 */
int desktop_init_global(void)
{
    return desktop_init(&g_desktop);
}
