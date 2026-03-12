/*
 * NEXUS OS - Desktop Environment Implementation
 * gui/desktop-environment/desktop-env.c
 *
 * Modern desktop environment with panels, backgrounds, menus, and more
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "desktop-env.h"
#include "../widgets/widgets.h"
#include "../window-manager/window-manager.h"
#include "../compositor/compositor.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL DESKTOP ENVIRONMENT INSTANCE               */
/*===========================================================================*/

static desktop_environment_t g_desktop_env;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* Background rendering */
static int render_solid_background(desktop_environment_t *env);
static int render_gradient_background(desktop_environment_t *env);
static int render_image_background(desktop_environment_t *env);
static color_t gradient_sample(gradient_t *gradient, float t);

/* Icon management */
static int arrange_icons_grid(desktop_environment_t *env);
static int snap_icon_to_grid(desktop_environment_t *env, desktop_icon_t *icon);

/* Panel operations */
static int create_default_panels(desktop_environment_t *env);
static int create_taskbar(desktop_environment_t *env);
static int create_menubar(desktop_environment_t *env);

/* Menu operations */
static int create_start_menu(desktop_environment_t *env);
static menu_item_t *create_menu_item(const char *label, const char *icon, const char *shortcut);

/* Search */
static int update_search_suggestions(desktop_environment_t *env);

/* Rendering */
static int render_panel(desktop_environment_t *env, desktop_panel_t *panel);
static int render_system_tray(desktop_environment_t *env);
static int render_notifications(desktop_environment_t *env);

/* Utilities */
static void init_default_theme(desktop_environment_t *env);
static void init_default_icons(desktop_environment_t *env);

/*===========================================================================*/
/*                         DESKTOP INITIALIZATION                            */
/*===========================================================================*/

/**
 * desktop_env_init - Initialize desktop environment
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_env_init(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    printk("[DESKTOP] ========================================\n");
    printk("[DESKTOP] NEXUS OS Desktop Environment v%s\n", DESKTOP_ENV_VERSION);
    printk("[DESKTOP] ========================================\n");
    printk("[DESKTOP] Initializing desktop environment...\n");
    
    /* Clear structure */
    memset(env, 0, sizeof(desktop_environment_t));
    env->initialized = true;
    env->icon_count = 0;
    env->panel_count = 0;
    
    /* Default settings */
    env->icon_layout = 0;  /* Grid layout */
    env->icon_size = ICON_SIZE_LARGE;
    env->grid_size = 80;
    env->show_icons = true;
    env->snap_to_grid = true;
    env->animations_enabled = true;
    env->sounds_enabled = true;
    env->double_click_speed = 500;  /* ms */
    
    /* Initialize default theme */
    init_default_theme(env);
    
    /* Initialize default background (gradient) */
    env->background.type = BG_TYPE_GRADIENT_V;
    env->background.gradient.type = GRADIENT_LINEAR;
    env->background.gradient.style = GRADIENT_LINEAR;
    env->background.gradient.stops[0].position = 0.0f;
    env->background.gradient.stops[0].color = color_from_rgba(26, 26, 46, 255);
    env->background.gradient.stops[1].position = 1.0f;
    env->background.gradient.stops[1].color = color_from_rgba(45, 45, 75, 255);
    env->background.gradient.stop_count = 2;
    
    /* Initialize default icons */
    init_default_icons(env);
    
    /* Create default panels */
    create_default_panels(env);
    
    /* Create start menu */
    create_start_menu(env);
    
    /* Initialize search bar */
    env->search_bar.x = 100;
    env->search_bar.y = 10;
    env->search_bar.width = 400;
    env->search_bar.height = 40;
    strcpy(env->search_bar.placeholder, "Search applications, files, settings...");
    env->search_bar.max_suggestions = 8;
    env->search_bar.on_search = NULL;
    
    /* Initialize system tray */
    env->system_tray.item_count = 0;
    env->system_tray.visible = true;
    
    /* Initialize notification area */
    env->notifications.notification_count = 0;
    env->notifications.position = PANEL_POSITION_TOP;
    env->notifications.visible = true;
    
    /* Initialize workspace switcher */
    env->workspace_switcher.workspace_count = WM_MAX_WORKSPACES;
    env->workspace_switcher.current_workspace = 0;
    env->workspace_switcher.visible = true;
    env->workspace_switcher.layout = 0;  /* Horizontal */
    env->workspace_switcher.active_color = color_from_rgba(0, 120, 215, 255);
    env->workspace_switcher.inactive_color = color_from_rgba(100, 100, 110, 255);
    
    /* Initialize clock widget */
    env->clock.visible = true;
    env->clock.show_date = true;
    env->clock.show_seconds = false;
    env->clock.is_24hour = false;
    env->clock.font_size = 12;
    
    /* Initialize quick settings */
    env->quick_settings.visible = false;
    env->quick_settings.wifi_enabled = true;
    env->quick_settings.bluetooth_enabled = false;
    env->quick_settings.airplane_mode = false;
    env->quick_settings.dnd_enabled = false;
    env->quick_settings.night_light = false;
    env->quick_settings.brightness = 100;
    env->quick_settings.volume = 75;
    
    printk("[DESKTOP] Desktop environment initialized\n");
    printk("[DESKTOP] %d default icons, %d panels\n", env->icon_count, env->panel_count);
    printk("[DESKTOP] ========================================\n");
    
    return 0;
}

/**
 * desktop_env_run - Start desktop environment
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_env_run(desktop_environment_t *env)
{
    if (!env || !env->initialized) {
        return -EINVAL;
    }
    
    printk("[DESKTOP] Starting desktop environment...\n");
    
    env->running = true;
    
    /* Render initial desktop */
    desktop_render(env);
    
    return 0;
}

/**
 * desktop_env_shutdown - Shutdown desktop environment
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_env_shutdown(desktop_environment_t *env)
{
    if (!env || !env->initialized) {
        return -EINVAL;
    }
    
    printk("[DESKTOP] Shutting down desktop environment...\n");
    
    env->running = false;
    env->initialized = false;
    
    /* Clean up resources */
    /* In real implementation, would free all allocated resources */
    
    printk("[DESKTOP] Desktop environment shutdown complete\n");
    
    return 0;
}

/**
 * desktop_env_is_running - Check if desktop is running
 * @env: Pointer to desktop environment structure
 *
 * Returns: true if running, false otherwise
 */
bool desktop_env_is_running(desktop_environment_t *env)
{
    return env && env->running;
}

/*===========================================================================*/
/*                         BACKGROUND MANAGEMENT                             */
/*===========================================================================*/

/**
 * desktop_set_background - Set desktop background
 * @env: Pointer to desktop environment structure
 * @bg: Background configuration
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_set_background(desktop_environment_t *env, desktop_background_t *bg)
{
    if (!env || !bg) {
        return -EINVAL;
    }
    
    memcpy(&env->background, bg, sizeof(desktop_background_t));
    
    /* Re-render background */
    desktop_render_background(env);
    
    return 0;
}

/**
 * desktop_set_solid_color - Set solid color background
 * @env: Pointer to desktop environment structure
 * @color: Background color
 *
 * Returns: 0 on success
 */
int desktop_set_solid_color(desktop_environment_t *env, color_t color)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->background.type = BG_TYPE_SOLID_COLOR;
    env->background.solid_color = color;
    
    desktop_render_background(env);
    
    return 0;
}

/**
 * desktop_set_gradient - Set gradient background
 * @env: Pointer to desktop environment structure
 * @gradient: Gradient definition
 *
 * Returns: 0 on success
 */
int desktop_set_gradient(desktop_environment_t *env, gradient_t *gradient)
{
    if (!env || !gradient) {
        return -EINVAL;
    }
    
    env->background.type = BG_TYPE_GRADIENT_H;
    if (gradient->style == GRADIENT_RADIAL) {
        env->background.type = BG_TYPE_GRADIENT_RADIAL;
    }
    
    memcpy(&env->background.gradient, gradient, sizeof(gradient_t));
    
    desktop_render_background(env);
    
    return 0;
}

/**
 * desktop_set_image - Set image background
 * @env: Pointer to desktop environment structure
 * @path: Image path
 * @fit_mode: Fit mode (0=fill, 1=fit, 2=stretch, 3=tile, 4=center)
 *
 * Returns: 0 on success
 */
int desktop_set_image(desktop_environment_t *env, const char *path, u32 fit_mode)
{
    if (!env || !path) {
        return -EINVAL;
    }
    
    env->background.type = BG_TYPE_IMAGE;
    strncpy(env->background.image_path, path, sizeof(env->background.image_path) - 1);
    env->background.fit_mode = fit_mode;
    
    desktop_render_background(env);
    
    return 0;
}

/**
 * desktop_set_slideshow - Set slideshow background
 * @env: Pointer to desktop environment structure
 * @folder: Folder containing images
 * @interval: Interval in seconds
 *
 * Returns: 0 on success
 */
int desktop_set_slideshow(desktop_environment_t *env, const char *folder, u32 interval)
{
    if (!env || !folder) {
        return -EINVAL;
    }
    
    env->background.type = BG_TYPE_IMAGE_SLIDESHOW;
    strncpy(env->background.slideshow_folder, folder, sizeof(env->background.slideshow_folder) - 1);
    env->background.slideshow_interval = interval;
    
    return 0;
}

/**
 * desktop_get_background - Get current background
 * @env: Pointer to desktop environment structure
 *
 * Returns: Background configuration pointer
 */
desktop_background_t *desktop_get_background(desktop_environment_t *env)
{
    return env ? &env->background : NULL;
}

/**
 * render_solid_background - Render solid color background
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int render_solid_background(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    /* In real implementation, would fill framebuffer with color */
    color_t color = env->background.solid_color;
    
    printk("[DESKTOP] Rendering solid background: #%02X%02X%02X\n", 
           color.r, color.g, color.b);
    
    return 0;
}

/**
 * render_gradient_background - Render gradient background
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int render_gradient_background(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    gradient_t *grad = &env->background.gradient;
    
    printk("[DESKTOP] Rendering gradient background (%d stops)\n", grad->stop_count);
    
    /* In real implementation, would render gradient to framebuffer */
    /* For each pixel, sample gradient at appropriate position */
    
    return 0;
}

/**
 * render_image_background - Render image background
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int render_image_background(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    printk("[DESKTOP] Rendering image background: %s\n", env->background.image_path);
    
    /* In real implementation, would load and render image */
    
    return 0;
}

/**
 * desktop_render_background - Render desktop background
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_render_background(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    switch (env->background.type) {
        case BG_TYPE_SOLID_COLOR:
            return render_solid_background(env);
        case BG_TYPE_GRADIENT_H:
        case BG_TYPE_GRADIENT_V:
        case BG_TYPE_GRADIENT_RADIAL:
            return render_gradient_background(env);
        case BG_TYPE_IMAGE:
            return render_image_background(env);
        case BG_TYPE_IMAGE_SLIDESHOW:
            /* Would cycle through images */
            return render_image_background(env);
        default:
            return render_solid_background(env);
    }
}

/**
 * gradient_sample - Sample gradient at position
 * @gradient: Gradient definition
 * @t: Position (0.0-1.0)
 *
 * Returns: Interpolated color
 */
static color_t gradient_sample(gradient_t *gradient, float t)
{
    color_t result = {0, 0, 0, 255};
    
    if (!gradient || gradient->stop_count == 0) {
        return result;
    }
    
    if (t <= 0.0f) {
        return gradient->stops[0].color;
    }
    if (t >= 1.0f) {
        return gradient->stops[gradient->stop_count - 1].color;
    }
    
    /* Find surrounding stops */
    for (u32 i = 0; i < gradient->stop_count - 1; i++) {
        if (t >= gradient->stops[i].position && t <= gradient->stops[i + 1].position) {
            float range = gradient->stops[i + 1].position - gradient->stops[i].position;
            float local_t = (t - gradient->stops[i].position) / range;
            
            /* Linear interpolation */
            result.r = (u8)(gradient->stops[i].color.r * (1.0f - local_t) + 
                           gradient->stops[i + 1].color.r * local_t);
            result.g = (u8)(gradient->stops[i].color.g * (1.0f - local_t) + 
                           gradient->stops[i + 1].color.g * local_t);
            result.b = (u8)(gradient->stops[i].color.b * (1.0f - local_t) + 
                           gradient->stops[i + 1].color.b * local_t);
            result.a = (u8)(gradient->stops[i].color.a * (1.0f - local_t) + 
                           gradient->stops[i + 1].color.a * local_t);
            break;
        }
    }
    
    return result;
}

/**
 * gradient_create - Create gradient
 * @type: Gradient type
 * @color1: Start color
 * @color2: End color
 *
 * Returns: Pointer to created gradient
 */
gradient_t *gradient_create(u32 type, color_t color1, color_t color2)
{
    static gradient_t gradient;
    
    memset(&gradient, 0, sizeof(gradient_t));
    gradient.type = type;
    gradient.style = (type == GRADIENT_RADIAL) ? GRADIENT_RADIAL : GRADIENT_LINEAR;
    gradient.stops[0].position = 0.0f;
    gradient.stops[0].color = color1;
    gradient.stops[1].position = 1.0f;
    gradient.stops[1].color = color2;
    gradient.stop_count = 2;
    
    return &gradient;
}

/*===========================================================================*/
/*                         ICON MANAGEMENT                                   */
/*===========================================================================*/

/**
 * desktop_add_icon - Add desktop icon
 * @env: Pointer to desktop environment structure
 * @icon: Icon to add
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_add_icon(desktop_environment_t *env, desktop_icon_t *icon)
{
    if (!env || !icon) {
        return -EINVAL;
    }
    
    if (env->icon_count >= DESKTOP_MAX_ICONS) {
        printk("[DESKTOP] Maximum icon count reached\n");
        return -ENOSPC;
    }
    
    /* Add icon */
    memcpy(&env->icons[env->icon_count], icon, sizeof(desktop_icon_t));
    env->icons[env->icon_count].icon_id = env->icon_count + 1;
    
    /* Snap to grid if enabled */
    if (env->snap_to_grid) {
        snap_icon_to_grid(env, &env->icons[env->icon_count]);
    }
    
    env->icon_count++;
    
    return 0;
}

/**
 * desktop_remove_icon - Remove desktop icon
 * @env: Pointer to desktop environment structure
 * @icon_id: Icon ID to remove
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_remove_icon(desktop_environment_t *env, u32 icon_id)
{
    if (!env || icon_id == 0) {
        return -EINVAL;
    }
    
    /* Find and remove icon */
    for (u32 i = 0; i < env->icon_count; i++) {
        if (env->icons[i].icon_id == icon_id) {
            /* Shift remaining icons */
            for (u32 j = i; j < env->icon_count - 1; j++) {
                env->icons[j] = env->icons[j + 1];
            }
            env->icon_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * desktop_update_icon - Update desktop icon
 * @env: Pointer to desktop environment structure
 * @icon_id: Icon ID to update
 * @icon: New icon data
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_update_icon(desktop_environment_t *env, u32 icon_id, desktop_icon_t *icon)
{
    if (!env || !icon) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < env->icon_count; i++) {
        if (env->icons[i].icon_id == icon_id) {
            memcpy(&env->icons[i], icon, sizeof(desktop_icon_t));
            env->icons[i].icon_id = icon_id;
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * desktop_get_icon - Get desktop icon
 * @env: Pointer to desktop environment structure
 * @icon_id: Icon ID
 *
 * Returns: Icon pointer, or NULL if not found
 */
desktop_icon_t *desktop_get_icon(desktop_environment_t *env, u32 icon_id)
{
    if (!env || icon_id == 0) {
        return NULL;
    }
    
    for (u32 i = 0; i < env->icon_count; i++) {
        if (env->icons[i].icon_id == icon_id) {
            return &env->icons[i];
        }
    }
    
    return NULL;
}

/**
 * desktop_select_icon - Select desktop icon
 * @env: Pointer to desktop environment structure
 * @icon_id: Icon ID to select
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_select_icon(desktop_environment_t *env, u32 icon_id)
{
    if (!env) {
        return -EINVAL;
    }
    
    /* Deselect all icons */
    for (u32 i = 0; i < env->icon_count; i++) {
        env->icons[i].selected = false;
    }
    
    /* Select specified icon */
    for (u32 i = 0; i < env->icon_count; i++) {
        if (env->icons[i].icon_id == icon_id) {
            env->icons[i].selected = true;
            env->selected_icon = &env->icons[i];
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * desktop_arrange_icons - Arrange desktop icons
 * @env: Pointer to desktop environment structure
 * @layout: Layout type (0=grid, 1=free, 2=auto)
 *
 * Returns: 0 on success
 */
int desktop_arrange_icons(desktop_environment_t *env, u32 layout)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->icon_layout = layout;
    
    switch (layout) {
        case 0:  /* Grid */
            return arrange_icons_grid(env);
        case 1:  /* Free */
            /* Keep current positions */
            break;
        case 2:  /* Auto */
            return arrange_icons_grid(env);
    }
    
    return 0;
}

/**
 * arrange_icons_grid - Arrange icons in grid
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int arrange_icons_grid(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    s32 x = 20;
    s32 y = 20;
    s32 row_height = env->grid_size + 10;
    s32 col_width = env->grid_size + 10;
    s32 max_cols = (1920 - 40) / col_width;
    
    for (u32 i = 0; i < env->icon_count; i++) {
        desktop_icon_t *icon = &env->icons[i];
        
        icon->x = x;
        icon->y = y;
        icon->width = env->icon_size;
        icon->height = env->icon_size + 20;  /* Extra space for label */
        
        x += col_width;
        
        /* Wrap to next row */
        if (x + env->icon_size > 1920 - 20) {
            x = 20;
            y += row_height;
        }
    }
    
    return 0;
}

/**
 * snap_icon_to_grid - Snap icon to grid
 * @env: Pointer to desktop environment structure
 * @icon: Icon to snap
 *
 * Returns: 0 on success
 */
static int snap_icon_to_grid(desktop_environment_t *env, desktop_icon_t *icon)
{
    if (!env || !icon) {
        return -EINVAL;
    }
    
    s32 grid_x = (icon->x / env->grid_size) * env->grid_size;
    s32 grid_y = (icon->y / env->grid_size) * env->grid_size;
    
    icon->x = grid_x + 10;  /* Margin */
    icon->y = grid_y + 10;
    
    return 0;
}

/**
 * desktop_delete_icon - Delete icon (with confirmation)
 * @env: Pointer to desktop environment structure
 * @icon_id: Icon ID to delete
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_delete_icon(desktop_environment_t *env, u32 icon_id)
{
    /* Would show confirmation dialog */
    return desktop_remove_icon(env, icon_id);
}

/*===========================================================================*/
/*                         PANEL MANAGEMENT                                  */
/*===========================================================================*/

/**
 * create_default_panels - Create default panels
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int create_default_panels(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    /* Create taskbar (bottom) */
    create_taskbar(env);
    
    /* Create menubar (top) */
    create_menubar(env);
    
    return 0;
}

/**
 * create_taskbar - Create taskbar panel
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int create_taskbar(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    desktop_panel_t *panel = &env->panels[env->panel_count++];
    env->taskbar = panel;
    
    panel->panel_id = env->panel_count;
    strcpy(panel->name, "Taskbar");
    panel->position = PANEL_POSITION_BOTTOM;
    panel->x = 0;
    panel->y = 1080 - 48;
    panel->width = 1920;
    panel->height = 48;
    panel->visible = true;
    panel->autohide = false;
    panel->background = color_from_rgba(22, 22, 38, 240);
    panel->foreground = color_from_rgba(255, 255, 255, 255);
    panel->icon_size = 32;
    panel->spacing = 4;
    panel->margin = 4;
    
    /* Add start button */
    panel_item_t start_btn;
    memset(&start_btn, 0, sizeof(panel_item_t));
    start_btn.item_id = 1;
    strcpy(start_btn.name, "Start");
    strcpy(start_btn.tooltip, "Open Start Menu");
    start_btn.x = 4;
    start_btn.y = 4;
    start_btn.width = 40;
    start_btn.height = 40;
    start_btn.visible = true;
    start_btn.enabled = true;
    start_btn.on_click = NULL;  /* Would set callback */
    desktop_add_panel_item(env, panel->panel_id, &start_btn);
    
    /* Add task list area */
    /* Add system tray area */
    /* Add clock area */
    
    return 0;
}

/**
 * create_menubar - Create menu bar panel
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int create_menubar(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    desktop_panel_t *panel = &env->panels[env->panel_count++];
    env->menubar = panel;
    
    panel->panel_id = env->panel_count;
    strcpy(panel->name, "Menu Bar");
    panel->position = PANEL_POSITION_TOP;
    panel->x = 0;
    panel->y = 0;
    panel->width = 1920;
    panel->height = 28;
    panel->visible = true;
    panel->autohide = false;
    panel->background = color_from_rgba(22, 22, 38, 240);
    panel->foreground = color_from_rgba(255, 255, 255, 255);
    panel->icon_size = 16;
    panel->spacing = 2;
    panel->margin = 2;
    
    /* Add menu items: File, Edit, View, Go, Help */
    /* Would add actual menu items */
    
    return 0;
}

/**
 * desktop_create_panel - Create panel
 * @env: Pointer to desktop environment structure
 * @name: Panel name
 * @position: Panel position
 * @height: Panel height
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_create_panel(desktop_environment_t *env, const char *name, u32 position, u32 height)
{
    if (!env || !name) {
        return -EINVAL;
    }
    
    if (env->panel_count >= DESKTOP_MAX_PANELS) {
        return -ENOSPC;
    }
    
    desktop_panel_t *panel = &env->panels[env->panel_count++];
    
    panel->panel_id = env->panel_count;
    strncpy(panel->name, name, sizeof(panel->name) - 1);
    panel->position = position;
    panel->height = height;
    panel->visible = true;
    
    /* Set position based on panel position */
    switch (position) {
        case PANEL_POSITION_TOP:
            panel->x = 0;
            panel->y = 0;
            panel->width = 1920;
            break;
        case PANEL_POSITION_BOTTOM:
            panel->x = 0;
            panel->y = 1080 - height;
            panel->width = 1920;
            break;
        case PANEL_POSITION_LEFT:
            panel->x = 0;
            panel->y = 0;
            panel->width = height;
            panel->height = 1080;
            break;
        case PANEL_POSITION_RIGHT:
            panel->x = 1920 - height;
            panel->y = 0;
            panel->width = height;
            panel->height = 1080;
            break;
        case PANEL_POSITION_FLOATING:
            panel->x = 100;
            panel->y = 100;
            panel->width = 200;
            break;
    }
    
    return 0;
}

/**
 * desktop_remove_panel - Remove panel
 * @env: Pointer to desktop environment structure
 * @panel_id: Panel ID to remove
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_remove_panel(desktop_environment_t *env, u32 panel_id)
{
    if (!env || panel_id == 0) {
        return -EINVAL;
    }
    
    /* Find and remove panel */
    for (u32 i = 0; i < env->panel_count; i++) {
        if (env->panels[i].panel_id == panel_id) {
            /* Shift remaining panels */
            for (u32 j = i; j < env->panel_count - 1; j++) {
                env->panels[j] = env->panels[j + 1];
            }
            env->panel_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * desktop_add_panel_item - Add item to panel
 * @env: Pointer to desktop environment structure
 * @panel_id: Panel ID
 * @item: Item to add
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_add_panel_item(desktop_environment_t *env, u32 panel_id, panel_item_t *item)
{
    if (!env || !item) {
        return -EINVAL;
    }
    
    /* Find panel */
    for (u32 i = 0; i < env->panel_count; i++) {
        if (env->panels[i].panel_id == panel_id) {
            /* Add item to panel */
            /* In real implementation, would allocate and add to panel's item list */
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * desktop_remove_panel_item - Remove item from panel
 * @env: Pointer to desktop environment structure
 * @panel_id: Panel ID
 * @item_id: Item ID to remove
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_remove_panel_item(desktop_environment_t *env, u32 panel_id, u32 item_id)
{
    (void)env; (void)panel_id; (void)item_id;
    return 0;
}

/**
 * desktop_get_panel - Get panel
 * @env: Pointer to desktop environment structure
 * @panel_id: Panel ID
 *
 * Returns: Panel pointer, or NULL if not found
 */
desktop_panel_t *desktop_get_panel(desktop_environment_t *env, u32 panel_id)
{
    if (!env || panel_id == 0) {
        return NULL;
    }
    
    for (u32 i = 0; i < env->panel_count; i++) {
        if (env->panels[i].panel_id == panel_id) {
            return &env->panels[i];
        }
    }
    
    return NULL;
}

/**
 * desktop_set_panel_position - Set panel position
 * @env: Pointer to desktop environment structure
 * @panel_id: Panel ID
 * @position: New position
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_set_panel_position(desktop_environment_t *env, u32 panel_id, u32 position)
{
    desktop_panel_t *panel = desktop_get_panel(env, panel_id);
    if (!panel) {
        return -ENOENT;
    }
    
    panel->position = position;
    
    /* Update panel geometry based on new position */
    /* Would recalculate x, y, width, height */
    
    return 0;
}

/*===========================================================================*/
/*                         MENU OPERATIONS                                   */
/*===========================================================================*/

/**
 * create_start_menu - Create start menu
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int create_start_menu(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    desktop_menu_t *menu = &env->start_menu;
    
    menu->menu_id = 1;
    menu->menu_type = MENU_TYPE_START;
    strcpy(menu->title, "Start");
    menu->x = 4;
    menu->y = 1080 - 500;
    menu->width = 400;
    menu->height = 500;
    menu->visible = false;
    menu->auto_hide = true;
    menu->background = color_from_rgba(30, 30, 45, 250);
    menu->foreground = color_from_rgba(255, 255, 255, 255);
    menu->hover = color_from_rgba(0, 120, 215, 255);
    menu->font_size = 14;
    
    /* Add default menu items */
    /* Applications, Settings, Power, etc. */
    
    return 0;
}

/**
 * create_menu_item - Create menu item
 * @label: Item label
 * @icon: Icon path
 * @shortcut: Keyboard shortcut
 *
 * Returns: Pointer to created menu item
 */
static menu_item_t *create_menu_item(const char *label, const char *icon, const char *shortcut)
{
    static menu_item_t item;
    
    memset(&item, 0, sizeof(menu_item_t));
    strncpy(item.label, label, sizeof(item.label) - 1);
    if (icon) strncpy(item.icon_path, icon, sizeof(item.icon_path) - 1);
    if (shortcut) strncpy(item.shortcut, shortcut, sizeof(item.shortcut) - 1);
    item.item_type = 0;  /* Normal item */
    item.enabled = true;
    
    return &item;
}

/**
 * desktop_show_start_menu - Show start menu
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_show_start_menu(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->start_menu.visible = true;
    env->active_menu = &env->start_menu;
    
    return 0;
}

/**
 * desktop_hide_start_menu - Hide start menu
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_hide_start_menu(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->start_menu.visible = false;
    if (env->active_menu == &env->start_menu) {
        env->active_menu = NULL;
    }
    
    return 0;
}

/**
 * desktop_toggle_start_menu - Toggle start menu
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_toggle_start_menu(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    if (env->start_menu.visible) {
        return desktop_hide_start_menu(env);
    } else {
        return desktop_show_start_menu(env);
    }
}

/**
 * desktop_show_context_menu - Show context menu
 * @env: Pointer to desktop environment structure
 * @x: X position
 * @y: Y position
 *
 * Returns: 0 on success
 */
int desktop_show_context_menu(desktop_environment_t *env, s32 x, s32 y)
{
    if (!env) {
        return -EINVAL;
    }
    
    desktop_menu_t *menu = &env->context_menu;
    
    menu->menu_id = 2;
    menu->menu_type = MENU_TYPE_CONTEXT;
    menu->x = x;
    menu->y = y;
    menu->visible = true;
    env->active_menu = menu;
    
    return 0;
}

/**
 * desktop_hide_context_menu - Hide context menu
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_hide_context_menu(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->context_menu.visible = false;
    if (env->active_menu == &env->context_menu) {
        env->active_menu = NULL;
    }
    
    return 0;
}

/**
 * desktop_add_menu_item - Add item to menu
 * @menu: Menu to add to
 * @item: Item to add
 *
 * Returns: 0 on success
 */
int desktop_add_menu_item(desktop_menu_t *menu, menu_item_t *item)
{
    if (!menu || !item) {
        return -EINVAL;
    }
    
    /* In real implementation, would add to menu's item list */
    menu->item_count++;
    
    return 0;
}

/**
 * desktop_remove_menu_item - Remove item from menu
 * @menu: Menu to remove from
 * @item_id: Item ID to remove
 *
 * Returns: 0 on success
 */
int desktop_remove_menu_item(desktop_menu_t *menu, u32 item_id)
{
    if (!menu || item_id == 0) {
        return -EINVAL;
    }
    
    /* In real implementation, would remove from menu's item list */
    menu->item_count--;
    
    return 0;
}

/*===========================================================================*/
/*                         SEARCH OPERATIONS                                 */
/*===========================================================================*/

/**
 * desktop_show_search - Show search bar
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_show_search(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->search_bar.visible = true;
    env->search_bar.focused = true;
    env->search_active = true;
    
    return 0;
}

/**
 * desktop_hide_search - Hide search bar
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_hide_search(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->search_bar.visible = false;
    env->search_bar.focused = false;
    env->search_active = false;
    
    return desktop_clear_search(env);
}

/**
 * desktop_toggle_search - Toggle search bar
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_toggle_search(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    if (env->search_active) {
        return desktop_hide_search(env);
    } else {
        return desktop_show_search(env);
    }
}

/**
 * desktop_search_query - Search with query
 * @env: Pointer to desktop environment structure
 * @query: Search query
 *
 * Returns: 0 on success
 */
int desktop_search_query(desktop_environment_t *env, const char *query)
{
    if (!env || !query) {
        return -EINVAL;
    }
    
    strncpy(env->search_bar.query, query, sizeof(env->search_bar.query) - 1);
    
    /* Update suggestions */
    update_search_suggestions(env);
    
    /* Call search callback if set */
    if (env->search_bar.on_search) {
        env->search_bar.on_search(&env->search_bar, query);
    }
    
    return 0;
}

/**
 * desktop_clear_search - Clear search
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_clear_search(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->search_bar.query[0] = '\0';
    env->search_bar.suggestion_count = 0;
    env->search_bar.selected_suggestion = 0;
    
    return 0;
}

/**
 * update_search_suggestions - Update search suggestions
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int update_search_suggestions(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    /* In real implementation, would search for matching items */
    /* For now, add mock suggestions */
    
    env->search_bar.suggestion_count = 0;
    
    if (strlen(env->search_bar.query) > 0) {
        strcpy(env->search_bar.suggestions[env->search_bar.suggestion_count++], "Applications");
        strcpy(env->search_bar.suggestions[env->search_bar.suggestion_count++], "Files");
        strcpy(env->search_bar.suggestions[env->search_bar.suggestion_count++], "Settings");
        strcpy(env->search_bar.suggestions[env->search_bar.suggestion_count++], "Web Search");
    }
    
    return 0;
}

/*===========================================================================*/
/*                         SYSTEM TRAY                                       */
/*===========================================================================*/

/**
 * desktop_add_tray_item - Add system tray item
 * @env: Pointer to desktop environment structure
 * @item: Item to add
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_add_tray_item(desktop_environment_t *env, system_tray_item_t *item)
{
    if (!env || !item) {
        return -EINVAL;
    }
    
    if (env->system_tray.item_count >= 32) {
        return -ENOSPC;
    }
    
    memcpy(&env->system_tray.items[env->system_tray.item_count], item, sizeof(system_tray_item_t));
    env->system_tray.items[env->system_tray.item_count].tray_id = env->system_tray.item_count + 1;
    env->system_tray.item_count++;
    
    return 0;
}

/**
 * desktop_remove_tray_item - Remove system tray item
 * @env: Pointer to desktop environment structure
 * @tray_id: Tray item ID to remove
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_remove_tray_item(desktop_environment_t *env, u32 tray_id)
{
    if (!env || tray_id == 0) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < env->system_tray.item_count; i++) {
        if (env->system_tray.items[i].tray_id == tray_id) {
            /* Shift remaining items */
            for (u32 j = i; j < env->system_tray.item_count - 1; j++) {
                env->system_tray.items[j] = env->system_tray.items[j + 1];
            }
            env->system_tray.item_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * desktop_update_tray_item - Update system tray item
 * @env: Pointer to desktop environment structure
 * @tray_id: Tray item ID
 * @item: New item data
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_update_tray_item(desktop_environment_t *env, u32 tray_id, system_tray_item_t *item)
{
    if (!env || !item) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < env->system_tray.item_count; i++) {
        if (env->system_tray.items[i].tray_id == tray_id) {
            memcpy(&env->system_tray.items[i], item, sizeof(system_tray_item_t));
            env->system_tray.items[i].tray_id = tray_id;
            return 0;
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         NOTIFICATIONS                                     */
/*===========================================================================*/

/**
 * desktop_show_notification - Show notification
 * @env: Pointer to desktop environment structure
 * @notify: Notification to show
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_show_notification(desktop_environment_t *env, notification_t *notify)
{
    if (!env || !notify) {
        return -EINVAL;
    }
    
    if (env->notifications.notification_count >= 16) {
        return -ENOSPC;
    }
    
    memcpy(&env->notifications.notifications[env->notifications.notification_count], 
           notify, sizeof(notification_t));
    env->notifications.notifications[env->notifications.notification_count].notify_id = 
        env->notifications.notification_count + 1;
    env->notifications.notifications[env->notifications.notification_count].timestamp = get_time_ms();
    env->notifications.notification_count++;
    
    return 0;
}

/**
 * desktop_dismiss_notification - Dismiss notification
 * @env: Pointer to desktop environment structure
 * @notify_id: Notification ID to dismiss
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_dismiss_notification(desktop_environment_t *env, u32 notify_id)
{
    if (!env || notify_id == 0) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < env->notifications.notification_count; i++) {
        if (env->notifications.notifications[i].notify_id == notify_id) {
            /* Shift remaining notifications */
            for (u32 j = i; j < env->notifications.notification_count - 1; j++) {
                env->notifications.notifications[j] = env->notifications.notifications[j + 1];
            }
            env->notifications.notification_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * desktop_clear_notifications - Clear all notifications
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_clear_notifications(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->notifications.notification_count = 0;
    
    return 0;
}

/*===========================================================================*/
/*                         WIDGETS                                           */
/*===========================================================================*/

/**
 * desktop_update_clock - Update clock widget
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_update_clock(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    /* In real implementation, would get current time and update display */
    
    return 0;
}

/**
 * desktop_switch_workspace - Switch workspace
 * @env: Pointer to desktop environment structure
 * @workspace: Workspace number
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_switch_workspace(desktop_environment_t *env, u32 workspace)
{
    if (!env || workspace >= DESKTOP_MAX_WORKSPACES) {
        return -EINVAL;
    }
    
    env->workspace_switcher.current_workspace = workspace;
    
    if (env->on_workspace_changed) {
        env->on_workspace_changed(workspace);
    }
    
    return 0;
}

/**
 * desktop_show_quick_settings - Show quick settings
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_show_quick_settings(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->quick_settings.visible = true;
    
    return 0;
}

/**
 * desktop_hide_quick_settings - Hide quick settings
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_hide_quick_settings(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->quick_settings.visible = false;
    
    return 0;
}

/*===========================================================================*/
/*                         THEME                                             */
/*===========================================================================*/

/**
 * desktop_set_theme - Set desktop theme
 * @env: Pointer to desktop environment structure
 * @theme_name: Theme name
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_set_theme(desktop_environment_t *env, const char *theme_name)
{
    if (!env || !theme_name) {
        return -EINVAL;
    }
    
    strncpy(env->theme_name, theme_name, sizeof(env->theme_name) - 1);
    
    /* In real implementation, would load theme colors */
    
    return 0;
}

/**
 * desktop_load_theme - Load theme from file
 * @env: Pointer to desktop environment structure
 * @path: Theme file path
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_load_theme(desktop_environment_t *env, const char *path)
{
    if (!env || !path) {
        return -EINVAL;
    }
    
    /* In real implementation, would load theme from file */
    
    return 0;
}

/**
 * desktop_save_theme - Save theme to file
 * @env: Pointer to desktop environment structure
 * @path: Theme file path
 *
 * Returns: 0 on success, negative error code on failure
 */
int desktop_save_theme(desktop_environment_t *env, const char *path)
{
    if (!env || !path) {
        return -EINVAL;
    }
    
    /* In real implementation, would save theme to file */
    
    return 0;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

/**
 * desktop_render - Render desktop
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_render(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    /* Render background */
    desktop_render_background(env);
    
    /* Render icons */
    desktop_render_icons(env);
    
    /* Render panels */
    desktop_render_panels(env);
    
    /* Render system tray */
    render_system_tray(env);
    
    /* Render notifications */
    render_notifications(env);
    
    return 0;
}

/**
 * desktop_render_icons - Render desktop icons
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_render_icons(desktop_environment_t *env)
{
    if (!env || !env->show_icons) {
        return 0;
    }
    
    /* In real implementation, would render each icon */
    for (u32 i = 0; i < env->icon_count; i++) {
        desktop_icon_t *icon = &env->icons[i];
        
        if (icon->visible) {
            /* Render icon image */
            /* Render icon label */
            /* Render selection highlight if selected */
        }
    }
    
    return 0;
}

/**
 * desktop_render_panels - Render all panels
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
int desktop_render_panels(desktop_environment_t *env)
{
    if (!env) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < env->panel_count; i++) {
        render_panel(env, &env->panels[i]);
    }
    
    return 0;
}

/**
 * render_panel - Render single panel
 * @env: Pointer to desktop environment structure
 * @panel: Panel to render
 *
 * Returns: 0 on success
 */
static int render_panel(desktop_environment_t *env, desktop_panel_t *panel)
{
    if (!env || !panel || !panel->visible) {
        return 0;
    }
    
    /* In real implementation, would render panel background and items */
    
    return 0;
}

/**
 * render_system_tray - Render system tray
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int render_system_tray(desktop_environment_t *env)
{
    if (!env || !env->system_tray.visible) {
        return 0;
    }
    
    /* In real implementation, would render tray items */
    
    return 0;
}

/**
 * render_notifications - Render notifications
 * @env: Pointer to desktop environment structure
 *
 * Returns: 0 on success
 */
static int render_notifications(desktop_environment_t *env)
{
    if (!env || !env->notifications.visible) {
        return 0;
    }
    
    /* In real implementation, would render notifications */
    
    return 0;
}

/*===========================================================================*/
/*                         INPUT HANDLING                                    */
/*===========================================================================*/

/**
 * desktop_handle_mouse - Handle mouse input
 * @env: Pointer to desktop environment structure
 * @x: Mouse X position
 * @y: Mouse Y position
 * @buttons: Mouse buttons
 *
 * Returns: 0 on success
 */
int desktop_handle_mouse(desktop_environment_t *env, s32 x, s32 y, u32 buttons)
{
    if (!env) {
        return -EINVAL;
    }
    
    env->mouse_x = x;
    env->mouse_y = y;
    
    /* Check for icon hover */
    env->hover_icon = NULL;
    for (u32 i = 0; i < env->icon_count; i++) {
        desktop_icon_t *icon = &env->icons[i];
        if (x >= icon->x && x < icon->x + icon->width &&
            y >= icon->y && y < icon->y + icon->height) {
            env->hover_icon = icon;
            break;
        }
    }
    
    /* Handle clicks */
    if (buttons & 0x01) {  /* Left button */
        /* Check for icon click */
        if (env->hover_icon) {
            if (env->hover_icon->on_click) {
                env->hover_icon->on_click(env->hover_icon);
            }
        }
    }
    
    if (buttons & 0x02) {  /* Right button */
        /* Show context menu */
        desktop_show_context_menu(env, x, y);
    }
    
    return 0;
}

/**
 * desktop_handle_key - Handle keyboard input
 * @env: Pointer to desktop environment structure
 * @key: Key code
 * @modifiers: Modifier keys
 *
 * Returns: 0 on success
 */
int desktop_handle_key(desktop_environment_t *env, u32 key, u32 modifiers)
{
    if (!env) {
        return -EINVAL;
    }
    
    /* Handle global shortcuts */
    if (key == 0x5B) {  /* Super/Windows key */
        desktop_toggle_start_menu(env);
    }
    
    if (key == 0x53 && (modifiers & 0x01)) {  /* Super+S */
        desktop_toggle_search(env);
    }
    
    if (key == 0x44 && (modifiers & 0x01)) {  /* Super+D */
        /* Show desktop */
    }
    
    /* Workspace switching */
    if ((modifiers & 0x04) && (modifiers & 0x02)) {  /* Ctrl+Alt */
        if (key == 0x51) {  /* Left arrow */
            u32 ws = env->workspace_switcher.current_workspace;
            if (ws > 0) desktop_switch_workspace(env, ws - 1);
        }
        if (key == 0x52) {  /* Right arrow */
            u32 ws = env->workspace_switcher.current_workspace;
            if (ws < DESKTOP_MAX_WORKSPACES - 1) desktop_switch_workspace(env, ws + 1);
        }
    }
    
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION HELPERS                            */
/*===========================================================================*/

/**
 * init_default_theme - Initialize default theme
 * @env: Pointer to desktop environment structure
 */
static void init_default_theme(desktop_environment_t *env)
{
    if (!env) return;
    
    strcpy(env->theme_name, "dark-modern");
    strcpy(env->font_name, "Nexus Sans");
    env->font_size = 12;
    
    /* Theme colors */
    env->theme_colors[0] = color_from_rgba(22, 22, 38, 255);   /* Background */
    env->theme_colors[1] = color_from_rgba(30, 30, 45, 255);   /* Panel */
    env->theme_colors[2] = color_from_rgba(255, 255, 255, 255); /* Text */
    env->theme_colors[3] = color_from_rgba(0, 120, 215, 255);  /* Accent */
    env->theme_colors[4] = color_from_rgba(100, 100, 110, 255); /* Dim */
}

/**
 * init_default_icons - Initialize default icons
 * @env: Pointer to desktop environment structure
 */
static void init_default_icons(desktop_environment_t *env)
{
    if (!env) return;
    
    /* Add default desktop icons */
    desktop_icon_t icon;
    
    /* Trash icon */
    memset(&icon, 0, sizeof(desktop_icon_t));
    strcpy(icon.name, "Trash");
    strcpy(icon.path, "/trash");
    strcpy(icon.icon_path, "/usr/share/icons/trash.png");
    icon.icon_type = 0;  /* System */
    icon.x = 20;
    icon.y = 20;
    icon.workspace = 0;
    desktop_add_icon(env, &icon);
    
    /* Home folder icon */
    memset(&icon, 0, sizeof(desktop_icon_t));
    strcpy(icon.name, "Home");
    strcpy(icon.path, "/home");
    strcpy(icon.icon_path, "/usr/share/icons/home.png");
    icon.icon_type = 1;  /* Folder */
    icon.x = 20;
    icon.y = 120;
    icon.workspace = 0;
    desktop_add_icon(env, &icon);
    
    /* Computer icon */
    memset(&icon, 0, sizeof(desktop_icon_t));
    strcpy(icon.name, "Computer");
    strcpy(icon.path, "/");
    strcpy(icon.icon_path, "/usr/share/icons/computer.png");
    icon.icon_type = 2;  /* Device */
    icon.x = 20;
    icon.y = 220;
    icon.workspace = 0;
    desktop_add_icon(env, &icon);
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * desktop_get_bg_type_name - Get background type name
 * @type: Background type
 *
 * Returns: Type name string
 */
const char *desktop_get_bg_type_name(u32 type)
{
    switch (type) {
        case BG_TYPE_SOLID_COLOR:     return "Solid Color";
        case BG_TYPE_GRADIENT_H:      return "Horizontal Gradient";
        case BG_TYPE_GRADIENT_V:      return "Vertical Gradient";
        case BG_TYPE_GRADIENT_RADIAL: return "Radial Gradient";
        case BG_TYPE_IMAGE:           return "Image";
        case BG_TYPE_IMAGE_SLIDESHOW: return "Slideshow";
        case BG_TYPE_PATTERN:         return "Pattern";
        default:                      return "Unknown";
    }
}

/**
 * desktop_get_panel_position_name - Get panel position name
 * @position: Panel position
 *
 * Returns: Position name string
 */
const char *desktop_get_panel_position_name(u32 position)
{
    switch (position) {
        case PANEL_POSITION_TOP:      return "Top";
        case PANEL_POSITION_BOTTOM:   return "Bottom";
        case PANEL_POSITION_LEFT:     return "Left";
        case PANEL_POSITION_RIGHT:    return "Right";
        case PANEL_POSITION_FLOATING: return "Floating";
        default:                      return "Unknown";
    }
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * desktop_env_get_instance - Get global desktop environment instance
 *
 * Returns: Pointer to global instance
 */
desktop_environment_t *desktop_env_get_instance(void)
{
    return &g_desktop_env;
}
