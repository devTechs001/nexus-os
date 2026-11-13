/*
 * NEXUS OS - Desktop Environment Implementation
 * gui/desktop/desktop.c
 * 
 * Modern, clean desktop with panels, menus, taskbar, and system tray
 */

#include "desktop.h"
#include "../gui.h"
#include "../../fs/vfs/vfs.h"
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
    
    /* Screen (use actual boot params dimensions) */
    boot_params_t *dp = get_boot_params();
    desktop->screen_width = (dp && dp->gfx_width) ? dp->gfx_width : 1024;
    desktop->screen_height = (dp && dp->gfx_height) ? dp->gfx_height : 768;
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
 * desktop_render - Render the desktop
 *
 * This function previously drew nothing at all: every drawing call in it was
 * commented out, so the desktop "rendered" by falling through a list of empty
 * blocks. The renderer (gui/renderer/renderer.c) is real and works, so the
 * background, icon cells and panels are now actually painted through it.
 *
 * A graphics context is created per frame from the primary framebuffer and
 * released afterwards; the renderer is not re-entrant, so the frame is drawn
 * in a single pass.
 */
void desktop_render(desktop_t *desktop)
{
    graphics_context_t *ctx;
    rect_t r;
    color_t bg;
    color_t accent;

    if (!desktop) {
        return;
    }

    ctx = graphics_create_context(NULL);
    if (!ctx) {
        return;
    }

    /* Background: wallpaper is not decoded yet, so paint the solid colour. */
    bg.rgba = desktop->background_color;
    if (bg.a == 0) {
        bg = (color_t)COLOR_BLACK;
    }

    r.x = 0;
    r.y = 0;
    r.width = desktop->screen_width > 0 ? desktop->screen_width : (s32)ctx->width;
    r.height = desktop->screen_height > 0 ? desktop->screen_height : (s32)ctx->height;
    graphics_fill_rect(ctx, &r, bg);

    accent.rgba = desktop->accent_color;

    /* Icons: a tile plus its label. */
    for (int i = 0; i < desktop->icon_count; i++) {
        desktop_icon_t *icon = &desktop->icons[i];
        bool hot;

        if (icon->hidden) {
            continue;
        }

        hot = (desktop->hovered_icon == icon) || icon->selected;

        r.x = icon->x;
        r.y = icon->y;
        r.width = icon->width > 0 ? icon->width : 48;
        r.height = icon->height > 0 ? icon->height : 48;

        if (hot) {
            /* Hover/selection gets the accent colour behind the tile. */
            graphics_fill_rect(ctx, &r, accent);
        } else if (desktop->background_color != 0) {
            /* A slightly lifted tile so icons are visible on any background. */
            color_t tile;
            tile.r = (u8)(bg.r + 24 > 255 ? 255 : bg.r + 24);
            tile.g = (u8)(bg.g + 24 > 255 ? 255 : bg.g + 24);
            tile.b = (u8)(bg.b + 24 > 255 ? 255 : bg.b + 24);
            tile.a = 255;
            graphics_fill_rect(ctx, &r, tile);
        }

        if (icon->name[0]) {
            graphics_draw_text(ctx, icon->x, icon->y + r.height + 2, icon->name);
        }
    }

    /* Panels: bar, start button, taskbar items, tray, clock. */
    for (int i = 0; i < desktop->panel_count; i++) {
        desktop_panel_t *panel = &desktop->panels[i];
        color_t panel_color;

        panel_color.rgba = panel->color;
        if (panel_color.a == 0) {
            panel_color = accent;
        }

        r.x = panel->x;
        r.y = panel->y;
        r.width = panel->width;
        r.height = panel->height;
        graphics_fill_rect(ctx, &r, panel_color);

        if (panel->show_start_button) {
            rect_t btn;
            btn.x = panel->x + 8;
            btn.y = panel->y + 4;
            btn.width = 84;
            btn.height = panel->height - 8;
            graphics_fill_rect(ctx, &btn, accent);
            graphics_draw_text(ctx, btn.x + 6, btn.y + 6, "NEXUS");
        }

        /* Taskbar entries: active ones are highlighted. */
        for (int j = 0; j < panel->taskbar_count; j++) {
            taskbar_item_t *item = &panel->taskbar_items[j];
            rect_t cell;

            cell.x = panel->x + 100 + j * 190;
            cell.y = panel->y + 4;
            cell.width = 180;
            cell.height = panel->height - 8;

            if (cell.x + cell.width > panel->x + panel->width) {
                break;                      /* Would overflow the panel. */
            }

            if (item->active) {
                graphics_fill_rect(ctx, &cell, accent);
            }

            if (item->title[0]) {
                graphics_draw_text(ctx, cell.x + 6, cell.y + 6, item->title);
            }
        }

        /* System tray icons, right-aligned. */
        for (int j = 0; j < panel->tray_count; j++) {
            system_tray_item_t *item = &panel->tray_items[j];
            rect_t cell;

            if (!item->visible) {
                continue;
            }

            cell.x = panel->x + panel->width - 24 - j * 24;
            cell.y = panel->y + (panel->height - 16) / 2;
            cell.width = 16;
            cell.height = 16;

            if (cell.x < panel->x + 300) {
                break;
            }

            graphics_fill_rect(ctx, &cell, accent);

            if (item->tooltip[0]) {
                graphics_draw_text(ctx, cell.x, cell.y - 14, item->tooltip);
            }
        }

        /* Clock. */
        if (panel->show_clock) {
            graphics_draw_text(ctx, panel->x + panel->width - 120,
                               panel->y + (panel->height - 8) / 2,
                               "00:00");
        }
    }

    graphics_destroy_context(ctx);
}

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

/*===========================================================================*/
/*        DESKTOP API DECLARED IN desktop.h BUT NEVER IMPLEMENTED            */
/*===========================================================================*/
/*
 * desktop.h advertised twenty-odd functions that had no bodies anywhere in
 * the tree: no workspace management, no way to read or destroy a panel, no
 * accent colour, no wallpaper setter, no context menu. The desktop could be
 * initialised and drawn, but not driven.
 */

/**
 * desktop_workspace_create - Add another workspace
 *
 * Returns the new workspace index, or a negative error code.
 */
int desktop_workspace_create(desktop_t *desktop)
{
    int index;

    if (!desktop) {
        return -EINVAL;
    }

    if (desktop->workspace_count >= DESKTOP_MAX_WORKSPACES) {
        return -ENOSPC;
    }

    index = desktop->workspace_count++;
    desktop->workspace = index;

    pr_debug("[DESKTOP] Created workspace %d (of %d)\n",
             index, desktop->workspace_count);

    return index;
}

/**
 * desktop_workspace_switch - Make a workspace current
 *
 * Returns 0 on success, negative error code if the index is out of range.
 */
int desktop_workspace_switch(desktop_t *desktop, int workspace)
{
    if (!desktop) {
        return -EINVAL;
    }

    if (workspace < 0 || workspace >= desktop->workspace_count) {
        return -EINVAL;
    }

    desktop->workspace = workspace;

    /* Panels follow the switch, so their indicator stays in sync. */
    for (int i = 0; i < desktop->panel_count; i++) {
        if (desktop->panels[i].show_workspaces) {
            desktop->panels[i].current_workspace = workspace;
        }
    }

    return 0;
}

/**
 * desktop_workspace_get_current - Index of the active workspace
 */
int desktop_workspace_get_current(desktop_t *desktop)
{
    if (!desktop) {
        return -EINVAL;
    }
    return desktop->workspace;
}

/**
 * desktop_panel_get - Fetch a panel by index
 */
desktop_panel_t *desktop_panel_get(desktop_t *desktop, int index)
{
    if (!desktop || !desktop->panels) {
        return NULL;
    }
    if (index < 0 || index >= desktop->panel_count) {
        return NULL;
    }
    return &desktop->panels[index];
}

/**
 * desktop_panel_destroy - Remove a panel by index
 *
 * The panel array is packed, so the tail is shifted down.
 */
int desktop_panel_destroy(desktop_t *desktop, int index)
{
    if (!desktop || !desktop->panels) {
        return -EINVAL;
    }
    if (index < 0 || index >= desktop->panel_count) {
        return -EINVAL;
    }

    for (int i = index; i + 1 < desktop->panel_count; i++) {
        desktop->panels[i] = desktop->panels[i + 1];
    }
    desktop->panel_count--;

    return 0;
}

/**
 * desktop_panel_set_autohide - Enable or disable panel auto-hide
 */
int desktop_panel_set_autohide(desktop_t *desktop, int index, bool autohide)
{
    desktop_panel_t *panel = desktop_panel_get(desktop, index);

    if (!panel) {
        return -EINVAL;
    }

    panel->autohide = autohide;

    return 0;
}

/**
 * desktop_set_accent_color - Set the system accent colour
 */
int desktop_set_accent_color(desktop_t *desktop, u32 color)
{
    if (!desktop) {
        return -EINVAL;
    }

    desktop->accent_color = color;

    return 0;
}

/**
 * desktop_set_wallpaper - Point the desktop at a wallpaper
 *
 * @mode: STRETCH, FIT, FILL, TILE or CENTER
 *
 * The file is not decoded here - the render path owns that - but it must
 * exist, otherwise the desktop would show nothing and report success.
 */
int desktop_set_wallpaper(desktop_t *desktop, const char *path, u32 mode)
{
    struct vfs_stat st;

    if (!desktop) {
        return -EINVAL;
    }

    if (!path || !*path) {
        /* Empty path means "no wallpaper": fall back to a solid colour. */
        desktop->wallpaper_path[0] = '\0';
        return 0;
    }

    if (vfs_stat(path, &st) != 0) {
        pr_err("[DESKTOP] Wallpaper not found: %s\n", path);
        return -ENOENT;
    }

    strncpy(desktop->wallpaper_path, path, sizeof(desktop->wallpaper_path) - 1);
    desktop->wallpaper_path[sizeof(desktop->wallpaper_path) - 1] = '\0';
    desktop->wallpaper_mode = mode;

    return 0;
}

/**
 * desktop_set_background_color - Set the solid background colour
 */
int desktop_set_background_color(desktop_t *desktop, u32 color)
{
    if (!desktop) {
        return -EINVAL;
    }

    desktop->background_color = color;

    return 0;
}

/**
 * desktop_icon_get - Look up a desktop icon by id
 */
desktop_icon_t *desktop_icon_get(desktop_t *desktop, int index)
{
    if (!desktop || !desktop->icons) {
        return NULL;
    }
    if (index < 0 || index >= desktop->icon_count) {
        return NULL;
    }
    return &desktop->icons[index];
}

/**
 * desktop_show_context_menu - Open the desktop context menu
 */
int desktop_show_context_menu(desktop_t *desktop, int x, int y)
{
    if (!desktop) {
        return -EINVAL;
    }

    /*
     * desktop_t stores the context menu as an opaque pointer plus a visibility
     * flag; the position belongs to the menu object itself, so record it
     * there rather than inventing fields on the desktop.
     */
    desktop->context_menu_visible = true;
    if (desktop->context_menu) {
        /* desktop_menu_t lives in desktop-env.h; the first three fields are
         * x, y, position, so record the anchor point through the struct. */
        struct desktop_menu_anchor {
            s32 x;
            s32 y;
            u32 position;
        } *anchor = (struct desktop_menu_anchor *)desktop->context_menu;
        anchor->x = x;
        anchor->y = y;
    }

    return 0;
}

/**
 * desktop_hide_context_menu - Close the desktop context menu
 */
int desktop_hide_context_menu(desktop_t *desktop)
{
    if (!desktop) {
        return -EINVAL;
    }

    desktop->context_menu_visible = false;

    return 0;
}

/**
 * desktop_theme_save - Persist the current theme selection
 *
 * Writes the theme name to the desktop's config file so the choice survives
 * a reboot.
 */
int desktop_theme_save(desktop_t *desktop)
{
    char path[128];
    char buf[160];
    s32 fd;
    int len;

    if (!desktop) {
        return -EINVAL;
    }

    snprintf(path, sizeof(path), "/etc/desktop/%s.theme", desktop->theme_name[0] ?
             desktop->theme_name : "default");

    fd = vfs_open(path, VFS_FO_WRITE | VFS_FO_CREAT, 0644);
    if (fd < 0) {
        pr_err("[DESKTOP] Cannot save theme to %s\n", path);
        return -EIO;
    }

    len = snprintf(buf, sizeof(buf), "theme=%s\naccent=0x%08X\nwallpaper=%s\nmode=%u\n",
                   desktop->theme_name,
                   (unsigned)desktop->accent_color,
                   desktop->wallpaper_path,
                   (unsigned)desktop->wallpaper_mode);

    vfs_write((u32)fd, buf, (u64)len);
    vfs_close((u32)fd);

    return 0;
}

/**
 * desktop_theme_load - Restore a theme selection by name
 */
int desktop_theme_load(desktop_t *desktop, const char *theme_name)
{
    if (!desktop || !theme_name) {
        return -EINVAL;
    }

    strncpy(desktop->theme_name, theme_name, sizeof(desktop->theme_name) - 1);
    desktop->theme_name[sizeof(desktop->theme_name) - 1] = '\0';

    pr_info("[DESKTOP] Theme set to '%s'\n", desktop->theme_name);

    return 0;
}

/**
 * desktop_handle_key_press - Keyboard input for the desktop itself
 *
 * Handles the global shortcuts the desktop owns: workspace switching and
 * dismissing the context menu.
 */
int desktop_handle_key_press(desktop_t *desktop, u32 key, u32 modifiers)
{
    if (!desktop) {
        return -EINVAL;
    }

    /* Escape always dismisses transient UI. */
    if (key == 0x1B) {
        if (desktop->context_menu_visible) {
            desktop->context_menu_visible = false;
            return 0;
        }
        if (desktop->selected_icon) {
            desktop->selected_icon = NULL;
            return 0;
        }
        return -ENOENT;
    }

    /* Alt+Left / Alt+Right cycle workspaces. */
    /* Bit 2 of the modifier mask is Alt (see the keyboard driver). */
    if (modifiers & 0x4) {
        if (key == 0x25) {      /* Left arrow */
            int target = desktop->workspace - 1;
            if (target < 0) {
                target = desktop->workspace_count - 1;
            }
            return desktop_workspace_switch(desktop, target);
        }
        if (key == 0x27) {      /* Right arrow */
            int target = desktop->workspace + 1;
            if (target >= desktop->workspace_count) {
                target = 0;
            }
            return desktop_workspace_switch(desktop, target);
        }
    }

    return -ENOSYS;
}

/**
 * desktop_handle_mouse_move - Track the cursor over the desktop
 */
int desktop_handle_mouse_move(desktop_t *desktop, int x, int y, u32 buttons)
{
    (void)buttons;

    if (!desktop) {
        return -EINVAL;
    }

    /* Update hover state for icons under the cursor. */
    desktop->hovered_icon = NULL;
    if (desktop->icons) {
        for (int i = 0; i < desktop->icon_count; i++) {
            desktop_icon_t *icon = &desktop->icons[i];
            if (x >= icon->x && x < icon->x + icon->width &&
                y >= icon->y && y < icon->y + icon->height) {
                desktop->hovered_icon = icon;
                break;
            }
        }
    }

    return 0;
}

/*===========================================================================*/
/*                    TASKBAR, TRAY AND START MENU                           */
/*===========================================================================*/

/**
 * find_taskbar_panel - Locate the panel that owns taskbar items
 *
 * Taskbar state is per-panel, so an operation on a window id has to find the
 * panel holding it first.
 */
static desktop_panel_t *find_taskbar_panel(desktop_t *desktop, int *out_index)
{
    if (!desktop || !desktop->panels) {
        return NULL;
    }

    for (int i = 0; i < desktop->panel_count; i++) {
        if (desktop->panels[i].show_taskbar) {
            if (out_index) {
                *out_index = i;
            }
            return &desktop->panels[i];
        }
    }

    return NULL;
}

/**
 * desktop_taskbar_remove_item - Remove a window's taskbar button
 */
int desktop_taskbar_remove_item(desktop_t *desktop, u32 window_id)
{
    desktop_panel_t *panel = find_taskbar_panel(desktop, NULL);
    int found = -1;

    if (!panel) {
        return -ENOENT;
    }

    for (int i = 0; i < panel->taskbar_count; i++) {
        if (panel->taskbar_items[i].window_id == window_id) {
            found = i;
            break;
        }
    }

    if (found < 0) {
        return -ENOENT;
    }

    for (int i = found; i + 1 < panel->taskbar_count; i++) {
        panel->taskbar_items[i] = panel->taskbar_items[i + 1];
    }
    panel->taskbar_count--;

    return 0;
}

/**
 * desktop_taskbar_update_item - Update a taskbar button's state
 *
 * @window_id: Which button to change
 * @title:     New title, or NULL to keep the current one
 */
int desktop_taskbar_update_item(desktop_t *desktop, u32 window_id,
                                const char *title, bool active, bool minimized)
{
    desktop_panel_t *panel = find_taskbar_panel(desktop, NULL);

    if (!panel) {
        return -ENOENT;
    }

    for (int i = 0; i < panel->taskbar_count; i++) {
        taskbar_item_t *item = &panel->taskbar_items[i];

        if (item->window_id != window_id) {
            continue;
        }

        if (title) {
            strncpy(item->title, title, sizeof(item->title) - 1);
            item->title[sizeof(item->title) - 1] = '\0';
        }

        item->active = active;
        item->minimized = minimized;

        if (active) {
            item->urgent = false;
            item->last_active = get_time_ms();
        }

        return 0;
    }

    return -ENOENT;
}

/**
 * desktop_taskbar_set_active - Make one window the active taskbar entry
 *
 * Exactly one button is active at a time, so every other entry is cleared.
 */
int desktop_taskbar_set_active(desktop_t *desktop, u32 window_id)
{
    desktop_panel_t *panel = find_taskbar_panel(desktop, NULL);
    int matched = -1;

    if (!panel) {
        return -ENOENT;
    }

    for (int i = 0; i < panel->taskbar_count; i++) {
        if (panel->taskbar_items[i].window_id == window_id) {
            matched = i;
        }
    }

    if (matched < 0) {
        return -ENOENT;
    }

    for (int i = 0; i < panel->taskbar_count; i++) {
        panel->taskbar_items[i].active = (i == matched);
    }

    panel->taskbar_items[matched].last_active = get_time_ms();
    panel->taskbar_items[matched].urgent = false;

    return 0;
}

/**
 * desktop_tray_get_item - Look up a tray item by name
 */
system_tray_item_t *desktop_tray_get_item(desktop_t *desktop, const char *name)
{
    if (!desktop || !desktop->panels || !name) {
        return NULL;
    }

    for (int i = 0; i < desktop->panel_count; i++) {
        desktop_panel_t *panel = &desktop->panels[i];

        for (int j = 0; j < panel->tray_count; j++) {
            if (strcmp(panel->tray_items[j].name, name) == 0) {
                return &panel->tray_items[j];
            }
        }
    }

    return NULL;
}

/**
 * desktop_tray_remove_item - Remove a tray item by name
 */
int desktop_tray_remove_item(desktop_t *desktop, const char *name)
{
    if (!desktop || !desktop->panels || !name) {
        return -EINVAL;
    }

    for (int i = 0; i < desktop->panel_count; i++) {
        desktop_panel_t *panel = &desktop->panels[i];

        for (int j = 0; j < panel->tray_count; j++) {
            if (strcmp(panel->tray_items[j].name, name) != 0) {
                continue;
            }

            for (int k = j; k + 1 < panel->tray_count; k++) {
                panel->tray_items[k] = panel->tray_items[k + 1];
            }
            panel->tray_count--;

            return 0;
        }
    }

    return -ENOENT;
}

/**
 * desktop_start_menu_add_app - Register an application in the start menu
 *
 * The menu stores an array of void* pointers, so it grows by one slot per
 * entry. Reallocating is done explicitly because there is no realloc here.
 */
int desktop_start_menu_add_app(desktop_t *desktop, void *app)
{
    start_menu_t *menu;
    void **grown;

    if (!desktop || !app) {
        return -EINVAL;
    }

    menu = &desktop->start_menu;

    if (menu->app_count >= DESKTOP_MAX_START_APPS) {
        return -ENOSPC;
    }

    grown = (void **)kmalloc(sizeof(void *) * (menu->app_count + 1));
    if (!grown) {
        return -ENOMEM;
    }

    for (int i = 0; i < menu->app_count; i++) {
        grown[i] = menu->apps[i];
    }
    grown[menu->app_count] = app;

    if (menu->apps) {
        kfree(menu->apps);
    }

    menu->apps = grown;
    menu->app_count++;

    return 0;
}
