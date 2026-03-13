/*
 * NEXUS OS - Desktop Grid & Effects
 * gui/desktop/desktop_grid.c
 *
 * Desktop icon grid, workspace switcher, desktop effects
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         DESKTOP GRID CONFIGURATION                        */
/*===========================================================================*/

#define DESKTOP_MAX_ICONS           256
#define DESKTOP_ICON_SIZE           64
#define DESKTOP_ICON_SPACING        80
#define DESKTOP_MAX_WORKSPACES      10
#define DESKTOP_ANIMATION_FPS       60

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 icon_id;
    char name[128];
    char path[256];
    char icon_path[256];
    s32 x;
    s32 y;
    u32 width;
    u32 height;
    bool selected;
    bool dragging;
    void *data;
} desktop_icon_t;

typedef struct {
    u32 workspace_id;
    char name[32];
    desktop_icon_t icons[DESKTOP_MAX_ICONS];
    u32 icon_count;
    u32 columns;
    u32 rows;
    bool active;
} desktop_workspace_t;

typedef struct {
    bool enabled;
    u32 type;                       /* FADE/SLIDE/ZOOM */
    u32 duration_ms;
    u32 start_time;
    float progress;
    bool running;
} desktop_animation_t;

typedef struct {
    bool initialized;
    desktop_workspace_t workspaces[DESKTOP_MAX_WORKSPACES];
    u32 workspace_count;
    u32 active_workspace;
    desktop_icon_t *dragged_icon;
    s32 drag_offset_x;
    s32 drag_offset_y;
    rect_t grid_rect;
    u32 icon_size;
    u32 icon_spacing;
    bool auto_arrange;
    bool show_hidden;
    char wallpaper_path[256];
    u32 wallpaper_mode;             /* FILL/FIT/STRETCH/TILE/CENTER */
    desktop_animation_t animation;
    void (*on_icon_click)(desktop_icon_t *);
    void (*on_icon_double_click)(desktop_icon_t *);
    void (*on_workspace_change)(u32 old, u32 new);
} desktop_grid_t;

static desktop_grid_t g_desktop;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int desktop_grid_init(void)
{
    printk("[DESKTOP] Initializing desktop grid...\n");
    
    memset(&g_desktop, 0, sizeof(desktop_grid_t));
    g_desktop.initialized = true;
    g_desktop.workspace_count = DESKTOP_MAX_WORKSPACES;
    g_desktop.active_workspace = 0;
    g_desktop.icon_size = DESKTOP_ICON_SIZE;
    g_desktop.icon_spacing = DESKTOP_ICON_SPACING;
    g_desktop.auto_arrange = true;
    g_desktop.wallpaper_mode = 0;  /* FILL */
    
    /* Initialize workspaces */
    for (u32 i = 0; i < g_desktop.workspace_count; i++) {
        desktop_workspace_t *ws = &g_desktop.workspaces[i];
        ws->workspace_id = i;
        snprintf(ws->name, sizeof(ws->name), "Workspace %d", i + 1);
        ws->columns = 12;
        ws->rows = 8;
        ws->active = (i == 0);
    }
    
    /* Add default icons */
    desktop_icon_t *icon;
    
    /* Trash */
    icon = &g_desktop.workspaces[0].icons[g_desktop.workspaces[0].icon_count++];
    icon->icon_id = 1;
    strcpy(icon->name, "Trash");
    strcpy(icon->icon_path, "/icons/trash.png");
    icon->x = 0;
    icon->y = 0;
    icon->width = DESKTOP_ICON_SIZE;
    icon->height = DESKTOP_ICON_SIZE;
    
    /* Home */
    icon = &g_desktop.workspaces[0].icons[g_desktop.workspaces[0].icon_count++];
    icon->icon_id = 2;
    strcpy(icon->name, "Home");
    strcpy(icon->icon_path, "/icons/home.png");
    icon->x = 0;
    icon->y = 1;
    icon->width = DESKTOP_ICON_SIZE;
    icon->height = DESKTOP_ICON_SIZE;
    
    /* Computer */
    icon = &g_desktop.workspaces[0].icons[g_desktop.workspaces[0].icon_count++];
    icon->icon_id = 3;
    strcpy(icon->name, "Computer");
    strcpy(icon->icon_path, "/icons/computer.png");
    icon->x = 0;
    icon->y = 2;
    icon->width = DESKTOP_ICON_SIZE;
    icon->height = DESKTOP_ICON_SIZE;
    
    printk("[DESKTOP] Desktop grid initialized (%d workspaces)\n", 
           g_desktop.workspace_count);
    return 0;
}

int desktop_grid_shutdown(void)
{
    printk("[DESKTOP] Shutting down desktop grid...\n");
    g_desktop.initialized = false;
    return 0;
}

/*===========================================================================*/
/*                         ICON MANAGEMENT                                   */
/*===========================================================================*/

desktop_icon_t *desktop_grid_add_icon(u32 workspace, const char *name, 
                                       const char *path, const char *icon_path)
{
    if (workspace >= g_desktop.workspace_count) {
        return NULL;
    }
    
    desktop_workspace_t *ws = &g_desktop.workspaces[workspace];
    if (ws->icon_count >= DESKTOP_MAX_ICONS) {
        return NULL;
    }
    
    desktop_icon_t *icon = &ws->icons[ws->icon_count++];
    icon->icon_id = ws->icon_count;
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    strncpy(icon->path, path, sizeof(icon->path) - 1);
    strncpy(icon->icon_path, icon_path, sizeof(icon->icon_path) - 1);
    icon->width = g_desktop.icon_size;
    icon->height = g_desktop.icon_size;
    icon->selected = false;
    icon->dragging = false;
    
    /* Auto-arrange position */
    if (g_desktop.auto_arrange) {
        icon->x = (icon->icon_id - 1) % ws->columns;
        icon->y = (icon->icon_id - 1) / ws->columns;
    }
    
    return icon;
}

int desktop_grid_remove_icon(u32 icon_id)
{
    for (u32 w = 0; w < g_desktop.workspace_count; w++) {
        desktop_workspace_t *ws = &g_desktop.workspaces[w];
        for (u32 i = 0; i < ws->icon_count; i++) {
            if (ws->icons[i].icon_id == icon_id) {
                /* Shift icons */
                for (u32 j = i; j < ws->icon_count - 1; j++) {
                    ws->icons[j] = ws->icons[j + 1];
                }
                ws->icon_count--;
                return 0;
            }
        }
    }
    return -ENOENT;
}

desktop_icon_t *desktop_grid_get_icon(u32 icon_id)
{
    for (u32 w = 0; w < g_desktop.workspace_count; w++) {
        desktop_workspace_t *ws = &g_desktop.workspaces[w];
        for (u32 i = 0; i < ws->icon_count; i++) {
            if (ws->icons[i].icon_id == icon_id) {
                return &ws->icons[i];
            }
        }
    }
    return NULL;
}

int desktop_grid_select_icon(u32 icon_id, bool selected)
{
    desktop_icon_t *icon = desktop_grid_get_icon(icon_id);
    if (!icon) return -ENOENT;
    
    icon->selected = selected;
    return 0;
}

/*===========================================================================*/
/*                         WORKSPACE MANAGEMENT                              */
/*===========================================================================*/

int desktop_grid_switch_workspace(u32 workspace)
{
    if (workspace >= g_desktop.workspace_count) {
        return -EINVAL;
    }
    
    u32 old = g_desktop.active_workspace;
    if (old == workspace) return 0;
    
    g_desktop.workspaces[old].active = false;
    g_desktop.workspaces[workspace].active = true;
    g_desktop.active_workspace = workspace;
    
    printk("[DESKTOP] Switched to workspace %d\n", workspace);
    
    if (g_desktop.on_workspace_change) {
        g_desktop.on_workspace_change(old, workspace);
    }
    
    /* Start animation */
    g_desktop.animation.enabled = true;
    g_desktop.animation.type = 1;  /* SLIDE */
    g_desktop.animation.duration_ms = 300;
    g_desktop.animation.start_time = ktime_get_ms();
    g_desktop.animation.running = true;
    
    return 0;
}

int desktop_grid_next_workspace(void)
{
    u32 next = (g_desktop.active_workspace + 1) % g_desktop.workspace_count;
    return desktop_grid_switch_workspace(next);
}

int desktop_grid_prev_workspace(void)
{
    s32 prev = (s32)g_desktop.active_workspace - 1;
    if (prev < 0) prev = g_desktop.workspace_count - 1;
    return desktop_grid_switch_workspace((u32)prev);
}

/*===========================================================================*/
/*                         DRAG AND DROP                                     */
/*===========================================================================*/

int desktop_grid_icon_click(desktop_icon_t *icon, s32 x, s32 y)
{
    if (!icon) return -EINVAL;
    
    icon->selected = true;
    icon->dragging = true;
    g_desktop.dragged_icon = icon;
    
    /* Calculate drag offset */
    g_desktop.drag_offset_x = x - icon->x * g_desktop.icon_spacing;
    g_desktop.drag_offset_y = y - icon->y * g_desktop.icon_spacing;
    
    if (g_desktop.on_icon_click) {
        g_desktop.on_icon_click(icon);
    }
    
    return 0;
}

int desktop_grid_icon_release(desktop_icon_t *icon, s32 x, s32 y)
{
    if (!icon) return -EINVAL;
    
    icon->dragging = false;
    g_desktop.dragged_icon = NULL;
    
    /* Update position */
    icon->x = (x - g_desktop.drag_offset_x) / g_desktop.icon_spacing;
    icon->y = (y - g_desktop.drag_offset_y) / g_desktop.icon_spacing;
    
    return 0;
}

int desktop_grid_icon_double_click(desktop_icon_t *icon)
{
    if (!icon) return -EINVAL;
    
    if (g_desktop.on_icon_double_click) {
        g_desktop.on_icon_double_click(icon);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         ANIMATIONS                                        */
/*===========================================================================*/

static void update_animation(void)
{
    if (!g_desktop.animation.running) return;
    
    u32 elapsed = ktime_get_ms() - g_desktop.animation.start_time;
    
    if (elapsed >= g_desktop.animation.duration_ms) {
        g_desktop.animation.progress = 1.0f;
        g_desktop.animation.running = false;
    } else {
        g_desktop.animation.progress = (float)elapsed / g_desktop.animation.duration_ms;
        
        /* Easing function (ease-out-cubic) */
        float t = 1.0f - g_desktop.animation.progress;
        g_desktop.animation.progress = 1.0f - t * t * t;
    }
}

int desktop_grid_enable_effects(bool enable)
{
    g_desktop.animation.enabled = enable;
    printk("[DESKTOP] Desktop effects %s\n", enable ? "enabled" : "disabled");
    return 0;
}

/*===========================================================================*/
/*                         WALLPAPER                                         */
/*===========================================================================*/

int desktop_grid_set_wallpaper(const char *path, u32 mode)
{
    if (!path) return -EINVAL;
    
    strncpy(g_desktop.wallpaper_path, path, sizeof(g_desktop.wallpaper_path) - 1);
    g_desktop.wallpaper_mode = mode;
    
    printk("[DESKTOP] Wallpaper set: %s (mode %d)\n", path, mode);
    return 0;
}

int desktop_grid_get_wallpaper(char *path, u32 path_size, u32 *mode)
{
    if (path) {
        strncpy(path, g_desktop.wallpaper_path, path_size - 1);
    }
    if (mode) {
        *mode = g_desktop.wallpaper_mode;
    }
    return 0;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

int desktop_grid_render(void *surface, rect_t *clip)
{
    if (!g_desktop.initialized || !surface) return -EINVAL;
    
    desktop_workspace_t *ws = &g_desktop.workspaces[g_desktop.active_workspace];
    
    /* Render wallpaper */
    /* In real implementation, would draw wallpaper image */
    
    /* Render icons */
    for (u32 i = 0; i < ws->icon_count; i++) {
        desktop_icon_t *icon = &ws->icons[i];
        
        s32 x = icon->x * g_desktop.icon_spacing;
        s32 y = icon->y * g_desktop.icon_spacing;
        
        /* Apply animation offset */
        if (g_desktop.animation.running && g_desktop.animation.type == 1) {
            s32 offset = (s32)((1.0f - g_desktop.animation.progress) * 200);
            x += offset;
        }
        
        /* Draw icon background if selected */
        if (icon->selected) {
            /* Draw selection rectangle */
        }
        
        /* Draw icon image */
        /* Draw icon label */
    }
    
    /* Update animations */
    update_animation();
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

int desktop_grid_list_icons(u32 workspace, desktop_icon_t *icons, u32 *count)
{
    if (workspace >= g_desktop.workspace_count || !icons || !count) {
        return -EINVAL;
    }
    
    desktop_workspace_t *ws = &g_desktop.workspaces[workspace];
    u32 copy = (*count < ws->icon_count) ? *count : ws->icon_count;
    
    memcpy(icons, ws->icons, sizeof(desktop_icon_t) * copy);
    *count = copy;
    
    return 0;
}

int desktop_grid_arrange_icons(u32 workspace)
{
    if (workspace >= g_desktop.workspace_count) {
        return -EINVAL;
    }
    
    desktop_workspace_t *ws = &g_desktop.workspaces[workspace];
    
    for (u32 i = 0; i < ws->icon_count; i++) {
        ws->icons[i].x = i % ws->columns;
        ws->icons[i].y = i / ws->columns;
    }
    
    return 0;
}

desktop_grid_t *desktop_grid_get(void)
{
    return &g_desktop;
}
