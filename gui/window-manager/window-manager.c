/*
 * NEXUS OS - Advanced Window Manager Implementation
 * gui/window-manager/window-manager.c
 *
 * Modern compositing window manager with advanced features
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "window-manager.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL WINDOW MANAGER INSTANCE                    */
/*===========================================================================*/

static window_manager_t g_window_manager;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* Window operations */
static wm_window_t *create_window_internal(window_manager_t *wm, window_props_t *props);
static int destroy_window_internal(window_manager_t *wm, wm_window_t *win);
static int update_window_geometry(window_manager_t *wm, wm_window_t *win);
static int repaint_window(window_manager_t *wm, wm_window_t *win);

/* Window state */
static int set_window_state(window_manager_t *wm, wm_window_t *win, u32 state);
static int save_restored_geometry(window_manager_t *wm, wm_window_t *win);
static int restore_geometry(window_manager_t *wm, wm_window_t *win);

/* Focus management */
static int focus_window_internal(window_manager_t *wm, wm_window_t *win);
static int blur_window_internal(window_manager_t *wm, wm_window_t *win);

/* Z-Order */
static int raise_window_internal(window_manager_t *wm, wm_window_t *win);
static int lower_window_internal(window_manager_t *wm, wm_window_t *win);

/* Snapping */
static rect_t calculate_snap_rect(window_manager_t *wm, u32 snap_state, wm_monitor_t *mon);
static int apply_snap_geometry(window_manager_t *wm, wm_window_t *win);

/* Rendering */
static int render_window_decorations(window_manager_t *wm, wm_window_t *win);
static int render_title_bar(window_manager_t *wm, wm_window_t *win);
static int render_border(window_manager_t *wm, wm_window_t *win);
static int render_shadow(window_manager_t *wm, wm_window_t *win);

/* Animation */
static int start_animation(window_manager_t *wm, wm_window_t *win, u32 type, window_geometry_t *to);
static int update_animation(window_manager_t *wm, wm_window_t *win);
static float ease_out_cubic(float t);
static float ease_in_out_quad(float t);

/* Input handling */
static int handle_title_bar_drag(window_manager_t *wm, wm_window_t *win, s32 x, s32 y);
static int handle_resize_drag(window_manager_t *wm, wm_window_t *win, s32 x, s32 y);
static wm_window_t *find_window_at_point(window_manager_t *wm, s32 x, s32 y);

/* Utilities */
static void init_default_theme(window_manager_t *wm);
static void init_snap_layouts(window_manager_t *wm);
static void init_workspaces(window_manager_t *wm);

/*===========================================================================*/
/*                         MANAGER INITIALIZATION                            */
/*===========================================================================*/

/**
 * wm_init - Initialize window manager
 * @wm: Pointer to window manager structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_init(window_manager_t *wm)
{
    if (!wm) {
        return -EINVAL;
    }
    
    printk("[WM] ========================================\n");
    printk("[WM] NEXUS OS Window Manager v%s\n", WM_VERSION);
    printk("[WM] ========================================\n");
    printk("[WM] Initializing window manager...\n");
    
    /* Clear structure */
    memset(wm, 0, sizeof(window_manager_t));
    wm->initialized = true;
    wm->window_count = 0;
    wm->next_window_id = 1;
    wm->focused_window = NULL;
    wm->active_window = NULL;
    
    /* Initialize default theme */
    init_default_theme(wm);
    
    /* Initialize workspaces */
    init_workspaces(wm);
    
    /* Initialize snap layouts */
    init_snap_layouts(wm);
    
    /* Default settings */
    wm->focus_follows_mouse = false;
    wm->raise_on_focus = true;
    wm->snap_enabled = true;
    wm->animations_enabled = true;
    wm->shadows_enabled = true;
    wm->snap_threshold = WM_SNAP_THRESHOLD;
    wm->title_height = 32;
    wm->border_width = 1;
    
    /* Virtual desktop */
    wm->virtual_desktop.x = 0;
    wm->virtual_desktop.y = 0;
    wm->virtual_desktop.width = 1920;
    wm->virtual_desktop.height = 1080;
    
    /* Detect monitors */
    wm_detect_monitors(wm);
    
    printk("[WM] Window manager initialized\n");
    printk("[WM] %d workspace(s), %d monitor(s)\n", 
           wm->workspace_count, wm->monitor_count);
    printk("[WM] ========================================\n");
    
    return 0;
}

/**
 * wm_run - Start window manager
 * @wm: Pointer to window manager structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_run(window_manager_t *wm)
{
    if (!wm || !wm->initialized) {
        return -EINVAL;
    }
    
    printk("[WM] Starting window manager...\n");
    
    wm->running = true;
    wm->last_frame_time = get_time_ms();
    
    return 0;
}

/**
 * wm_shutdown - Shutdown window manager
 * @wm: Pointer to window manager structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_shutdown(window_manager_t *wm)
{
    if (!wm || !wm->initialized) {
        return -EINVAL;
    }
    
    printk("[WM] Shutting down window manager...\n");
    
    wm->running = false;
    
    /* Destroy all windows */
    wm_window_t *win = wm->windows;
    while (win) {
        wm_window_t *next = win->next;
        wm_destroy_window(wm, win);
        win = next;
    }
    
    wm->windows = NULL;
    wm->window_count = 0;
    wm->initialized = false;
    
    printk("[WM] Window manager shutdown complete\n");
    
    return 0;
}

/**
 * wm_is_running - Check if window manager is running
 * @wm: Pointer to window manager structure
 *
 * Returns: true if running, false otherwise
 */
bool wm_is_running(window_manager_t *wm)
{
    return wm && wm->running;
}

/*===========================================================================*/
/*                         WINDOW CREATION/DESTRUCTION                       */
/*===========================================================================*/

/**
 * wm_create_window - Create a new window
 * @wm: Pointer to window manager structure
 * @props: Window properties
 *
 * Returns: Pointer to created window, or NULL on failure
 */
wm_window_t *wm_create_window(window_manager_t *wm, window_props_t *props)
{
    if (!wm || !props) {
        return NULL;
    }
    
    wm_window_t *win = create_window_internal(wm, props);
    if (!win) {
        return NULL;
    }
    
    /* Add to window list */
    win->next = wm->windows;
    wm->windows = win;
    wm->window_count++;
    
    /* Set as focused if parent is null */
    if (!props->parent) {
        wm_focus_window(wm, win);
    }
    
    /* Call callback */
    if (win->on_create) {
        win->on_create(win);
    }
    
    if (wm->on_window_created) {
        wm->on_window_created(win);
    }
    
    printk("[WM] Created window %d: %s\n", win->window_id, win->title);
    
    return win;
}

/**
 * create_window_internal - Create window internally
 * @wm: Pointer to window manager structure
 * @props: Window properties
 *
 * Returns: Pointer to created window
 */
static wm_window_t *create_window_internal(window_manager_t *wm, window_props_t *props)
{
    (void)wm;
    
    /* Allocate window structure */
    wm_window_t *win = kzalloc(sizeof(wm_window_t));
    if (!win) {
        return NULL;
    }
    
    /* Initialize window */
    win->window_id = wm->next_window_id++;
    strncpy(win->title, props->title, sizeof(win->title) - 1);
    win->workspace = wm->current_workspace;
    
    /* Set geometry */
    win->geometry.x = props->bounds.x;
    win->geometry.y = props->bounds.y;
    win->geometry.width = props->bounds.width;
    win->geometry.height = props->bounds.height;
    win->geometry.min_width = WM_MIN_WINDOW_WIDTH;
    win->geometry.min_height = WM_MIN_WINDOW_HEIGHT;
    
    /* Set state */
    win->state = WINDOW_STATE_NORMAL;
    win->flags = props->flags;
    win->visible = (props->flags & WINDOW_FLAG_VISIBLE) != 0;
    win->focused = false;
    win->active = false;
    
    /* Set decorations */
    win->decorations = DECOR_ALL;
    memcpy(&win->decor_colors, &wm->theme, sizeof(window_decor_colors_t));
    
    /* Set shadow */
    win->shadow.enabled = wm->shadows_enabled;
    win->shadow.size = 10;
    win->shadow.opacity = 128;
    win->shadow.offset_x = 2;
    win->shadow.offset_y = 4;
    win->shadow.color = wm->theme.shadow;
    
    /* Set Z-order */
    win->z_order = wm->window_count;
    win->layer = 0;
    
    /* Initialize reference count */
    atomic_set(&win->refcount, 1);
    
    return win;
}

/**
 * wm_destroy_window - Destroy a window
 * @wm: Pointer to window manager structure
 * @win: Window to destroy
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_destroy_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    printk("[WM] Destroying window %d: %s\n", win->window_id, win->title);
    
    /* Call callback */
    if (win->on_destroy) {
        win->on_destroy(win);
    }
    
    /* Remove from window list */
    wm_window_t **prev = &wm->windows;
    wm_window_t *curr = wm->windows;
    
    while (curr) {
        if (curr == win) {
            *prev = curr->next;
            wm->window_count--;
            break;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    /* Update focused window if needed */
    if (wm->focused_window == win) {
        wm->focused_window = NULL;
        wm->active_window = NULL;
    }
    
    /* Free window resources */
    if (win->surface) {
        kfree(win->surface);
    }
    if (win->back_buffer) {
        kfree(win->back_buffer);
    }
    
    /* Free window structure */
    kfree(win);
    
    if (wm->on_window_destroyed) {
        wm->on_window_destroyed(win);
    }
    
    return 0;
}

/**
 * wm_close_window - Close a window
 * @wm: Pointer to window manager structure
 * @win: Window to close
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_close_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    /* Hide window first */
    wm_hide_window(wm, win);
    
    /* Then destroy */
    return wm_destroy_window(wm, win);
}

/*===========================================================================*/
/*                         WINDOW OPERATIONS                                 */
/*===========================================================================*/

/**
 * wm_show_window - Show a window
 * @wm: Pointer to window manager structure
 * @win: Window to show
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_show_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    win->visible = true;
    win->state = WINDOW_STATE_NORMAL;
    
    /* Raise window */
    wm_raise_window(wm, win);
    
    /* Repaint */
    repaint_window(wm, win);
    
    return 0;
}

/**
 * wm_hide_window - Hide a window
 * @wm: Pointer to window manager structure
 * @win: Window to hide
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_hide_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    win->visible = false;
    
    /* Blur if focused */
    if (wm->focused_window == win) {
        blur_window_internal(wm, win);
    }
    
    return 0;
}

/**
 * wm_focus_window - Focus a window
 * @wm: Pointer to window manager structure
 * @win: Window to focus
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_focus_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    /* Blur current focused window */
    if (wm->focused_window && wm->focused_window != win) {
        blur_window_internal(wm, wm->focused_window);
    }
    
    /* Focus new window */
    focus_window_internal(wm, win);
    
    /* Raise window */
    if (wm->raise_on_focus) {
        wm_raise_window(wm, win);
    }
    
    return 0;
}

/**
 * focus_window_internal - Focus window internally
 * @wm: Pointer to window manager structure
 * @win: Window to focus
 *
 * Returns: 0 on success
 */
static int focus_window_internal(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    wm->focused_window = win;
    wm->active_window = win;
    win->focused = true;
    win->active = true;
    
    /* Call callback */
    if (win->on_focus) {
        win->on_focus(win);
    }
    
    if (wm->on_focus_changed) {
        wm->on_focus_changed(win);
    }
    
    /* Repaint title bar */
    repaint_window(wm, win);
    
    return 0;
}

/**
 * blur_window_internal - Blur window internally
 * @wm: Pointer to window manager structure
 * @win: Window to blur
 *
 * Returns: 0 on success
 */
static int blur_window_internal(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    win->focused = false;
    win->active = false;
    
    /* Call callback */
    if (win->on_blur) {
        win->on_blur(win);
    }
    
    /* Repaint title bar */
    repaint_window(wm, win);
    
    return 0;
}

/**
 * wm_raise_window - Raise window to top
 * @wm: Pointer to window manager structure
 * @win: Window to raise
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_raise_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    return raise_window_internal(wm, win);
}

/**
 * raise_window_internal - Raise window internally
 * @wm: Pointer to window manager structure
 * @win: Window to raise
 *
 * Returns: 0 on success
 */
static int raise_window_internal(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    /* Remove from current position */
    wm_window_t **prev = &wm->windows;
    wm_window_t *curr = wm->windows;
    
    while (curr) {
        if (curr == win) {
            *prev = curr->next;
            break;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    /* Add to head of list (top) */
    win->next = wm->windows;
    wm->windows = win;
    
    /* Update Z-order */
    u32 z = 0;
    curr = wm->windows;
    while (curr) {
        curr->z_order = z++;
        curr = curr->next;
    }
    
    return 0;
}

/**
 * wm_lower_window - Lower window to bottom
 * @wm: Pointer to window manager structure
 * @win: Window to lower
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_lower_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    return lower_window_internal(wm, win);
}

/**
 * lower_window_internal - Lower window internally
 * @wm: Pointer to window manager structure
 * @win: Window to lower
 *
 * Returns: 0 on success
 */
static int lower_window_internal(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    /* Remove from current position */
    wm_window_t **prev = &wm->windows;
    wm_window_t *curr = wm->windows;
    
    while (curr) {
        if (curr == win) {
            *prev = curr->next;
            break;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    /* Find tail and add there */
    curr = wm->windows;
    if (!curr) {
        wm->windows = win;
        win->next = NULL;
    } else {
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = win;
        win->next = NULL;
    }
    
    /* Update Z-order */
    u32 z = 0;
    curr = wm->windows;
    while (curr) {
        curr->z_order = z++;
        curr = curr->next;
    }
    
    return 0;
}

/**
 * wm_move_window - Move window to position
 * @wm: Pointer to window manager structure
 * @win: Window to move
 * @x: New X position
 * @y: New Y position
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_move_window(window_manager_t *wm, wm_window_t *win, s32 x, s32 y)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    win->geometry.x = x;
    win->geometry.y = y;
    
    update_window_geometry(wm, win);
    
    if (win->on_move) {
        win->on_move(win, x, y);
    }
    
    return 0;
}

/**
 * wm_resize_window - Resize window
 * @wm: Pointer to window manager structure
 * @win: Window to resize
 * @w: New width
 * @h: New height
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_resize_window(window_manager_t *wm, wm_window_t *win, s32 w, s32 h)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    /* Apply minimum size */
    if (w < win->geometry.min_width) w = win->geometry.min_width;
    if (h < win->geometry.min_height) h = win->geometry.min_height;
    
    /* Apply maximum size */
    if (w > win->geometry.max_width && win->geometry.max_width > 0) {
        w = win->geometry.max_width;
    }
    if (h > win->geometry.max_height && win->geometry.max_height > 0) {
        h = win->geometry.max_height;
    }
    
    win->geometry.width = w;
    win->geometry.height = h;
    
    update_window_geometry(wm, win);
    
    if (win->on_resize) {
        win->on_resize(win, w, h);
    }
    
    return 0;
}

/**
 * wm_set_geometry - Set window geometry
 * @wm: Pointer to window manager structure
 * @win: Window to update
 * @x: X position
 * @y: Y position
 * @w: Width
 * @h: Height
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_set_geometry(window_manager_t *wm, wm_window_t *win, s32 x, s32 y, s32 w, s32 h)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    win->geometry.x = x;
    win->geometry.y = y;
    win->geometry.width = w;
    win->geometry.height = h;
    
    update_window_geometry(wm, win);
    
    return 0;
}

/**
 * update_window_geometry - Update window geometry
 * @wm: Pointer to window manager structure
 * @win: Window to update
 *
 * Returns: 0 on success
 */
static int update_window_geometry(window_manager_t *wm, wm_window_t *win)
{
    (void)wm;
    (void)win;
    
    /* In real implementation, would update surface/buffer */
    
    return 0;
}

/**
 * repaint_window - Repaint window
 * @wm: Pointer to window manager structure
 * @win: Window to repaint
 *
 * Returns: 0 on success
 */
static int repaint_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    /* Mark entire window as dirty */
    win->dirty_rects[0].x = win->geometry.x;
    win->dirty_rects[0].y = win->geometry.y;
    win->dirty_rects[0].width = win->geometry.width;
    win->dirty_rects[0].height = win->geometry.height;
    win->dirty_count = 1;
    
    return 0;
}

/*===========================================================================*/
/*                         WINDOW STATE                                      */
/*===========================================================================*/

/**
 * wm_maximize_window - Maximize window
 * @wm: Pointer to window manager structure
 * @win: Window to maximize
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_maximize_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    /* Save current geometry */
    save_restored_geometry(wm, win);
    
    /* Get work area */
    wm_monitor_t *mon = wm_get_primary_monitor(wm);
    rect_t work_area = mon ? mon->work_area : wm->virtual_desktop;
    
    /* Set maximized geometry */
    win->geometry.x = work_area.x;
    win->geometry.y = work_area.y;
    win->geometry.width = work_area.width;
    win->geometry.height = work_area.height;
    
    set_window_state(wm, win, WINDOW_STATE_MAXIMIZED);
    update_window_geometry(wm, win);
    
    return 0;
}

/**
 * wm_minimize_window - Minimize window
 * @wm: Pointer to window manager structure
 * @win: Window to minimize
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_minimize_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    set_window_state(wm, win, WINDOW_STATE_MINIMIZED);
    wm_hide_window(wm, win);
    
    return 0;
}

/**
 * wm_restore_window - Restore window
 * @wm: Pointer to window manager structure
 * @win: Window to restore
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_restore_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    if (win->state == WINDOW_STATE_MAXIMIZED || 
        win->state == WINDOW_STATE_MINIMIZED ||
        win->state == WINDOW_STATE_FULLSCREEN) {
        
        restore_geometry(wm, win);
        set_window_state(wm, win, WINDOW_STATE_NORMAL);
        update_window_geometry(wm, win);
        wm_show_window(wm, win);
    }
    
    return 0;
}

/**
 * wm_fullscreen_window - Toggle fullscreen
 * @wm: Pointer to window manager structure
 * @win: Window to fullscreen
 *
 * Returns: 0 on success, negative error code on failure
 */
int wm_fullscreen_window(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    if (win->state == WINDOW_STATE_FULLSCREEN) {
        /* Restore from fullscreen */
        restore_geometry(wm, win);
        set_window_state(wm, win, WINDOW_STATE_NORMAL);
    } else {
        /* Save current geometry */
        save_restored_geometry(wm, win);
        
        /* Get monitor geometry */
        wm_monitor_t *mon = wm_get_primary_monitor(wm);
        rect_t mon_geo = mon ? mon->geometry : wm->virtual_desktop;
        
        /* Set fullscreen */
        win->geometry.x = mon_geo.x;
        win->geometry.y = mon_geo.y;
        win->geometry.width = mon_geo.width;
        win->geometry.height = mon_geo.height;
        
        set_window_state(wm, win, WINDOW_STATE_FULLSCREEN);
    }
    
    update_window_geometry(wm, win);
    
    return 0;
}

/**
 * wm_toggle_maximize - Toggle maximize
 * @wm: Pointer to window manager structure
 * @win: Window to toggle
 *
 * Returns: 0 on success
 */
int wm_toggle_maximize(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    if (win->state == WINDOW_STATE_MAXIMIZED) {
        wm_restore_window(wm, win);
    } else {
        wm_maximize_window(wm, win);
    }
    
    return 0;
}

/**
 * set_window_state - Set window state
 * @wm: Pointer to window manager structure
 * @win: Window to update
 * @state: New state
 *
 * Returns: 0 on success
 */
static int set_window_state(window_manager_t *wm, wm_window_t *win, u32 state)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    u32 old_state = win->state;
    win->state = state;
    
    if (win->on_state_change) {
        win->on_state_change(win, state);
    }
    
    /* Repaint */
    repaint_window(wm, win);
    
    (void)old_state;
    
    return 0;
}

/**
 * save_restored_geometry - Save geometry for restore
 * @wm: Pointer to window manager structure
 * @win: Window to save
 *
 * Returns: 0 on success
 */
static int save_restored_geometry(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    if (win->state != WINDOW_STATE_NORMAL) {
        return 0;  /* Already saved */
    }
    
    win->restored.x = win->geometry.x;
    win->restored.y = win->geometry.y;
    win->restored.width = win->geometry.width;
    win->restored.height = win->geometry.height;
    
    return 0;
}

/**
 * restore_geometry - Restore saved geometry
 * @wm: Pointer to window manager structure
 * @win: Window to restore
 *
 * Returns: 0 on success
 */
static int restore_geometry(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    win->geometry.x = win->restored.x;
    win->geometry.y = win->restored.y;
    win->geometry.width = win->restored.width;
    win->geometry.height = win->restored.height;
    
    return 0;
}

/*===========================================================================*/
/*                         SNAPPING AND TILING                               */
/*===========================================================================*/

/**
 * wm_snap_window_left - Snap window to left
 * @wm: Pointer to window manager structure
 * @win: Window to snap
 *
 * Returns: 0 on success
 */
int wm_snap_window_left(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    wm_monitor_t *mon = wm_get_primary_monitor(wm);
    win->snap_rect = calculate_snap_rect(wm, WINDOW_STATE_SNAPPED_LEFT, mon);
    apply_snap_geometry(wm, win);
    
    return 0;
}

/**
 * wm_snap_window_right - Snap window to right
 * @wm: Pointer to window manager structure
 * @win: Window to snap
 *
 * Returns: 0 on success
 */
int wm_snap_window_right(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    wm_monitor_t *mon = wm_get_primary_monitor(wm);
    win->snap_rect = calculate_snap_rect(wm, WINDOW_STATE_SNAPPED_RIGHT, mon);
    apply_snap_geometry(wm, win);
    
    return 0;
}

/**
 * calculate_snap_rect - Calculate snap rectangle
 * @wm: Pointer to window manager structure
 * @snap_state: Snap state
 * @mon: Monitor
 *
 * Returns: Snap rectangle
 */
static rect_t calculate_snap_rect(window_manager_t *wm, u32 snap_state, wm_monitor_t *mon)
{
    rect_t result = {0, 0, 0, 0};
    
    if (!mon) {
        mon = wm_get_primary_monitor(wm);
    }
    
    rect_t work = mon ? mon->work_area : wm->virtual_desktop;
    
    switch (snap_state) {
        case WINDOW_STATE_SNAPPED_LEFT:
            result.x = work.x;
            result.y = work.y;
            result.width = work.width / 2;
            result.height = work.height;
            break;
            
        case WINDOW_STATE_SNAPPED_RIGHT:
            result.x = work.x + work.width / 2;
            result.y = work.y;
            result.width = work.width / 2;
            result.height = work.height;
            break;
            
        case WINDOW_STATE_SNAPPED_TOP:
            result.x = work.x;
            result.y = work.y;
            result.width = work.width;
            result.height = work.height / 2;
            break;
            
        case WINDOW_STATE_SNAPPED_BOTTOM:
            result.x = work.x;
            result.y = work.y + work.height / 2;
            result.width = work.width;
            result.height = work.height / 2;
            break;
            
        default:
            result = work;
    }
    
    return result;
}

/**
 * apply_snap_geometry - Apply snap geometry to window
 * @wm: Pointer to window manager structure
 * @win: Window to apply
 *
 * Returns: 0 on success
 */
static int apply_snap_geometry(window_manager_t *wm, wm_window_t *win)
{
    if (!wm || !win) {
        return -EINVAL;
    }
    
    win->geometry = win->snap_rect;
    update_window_geometry(wm, win);
    
    return 0;
}

/**
 * wm_tile_windows - Tile all windows
 * @wm: Pointer to window manager structure
 *
 * Returns: 0 on success
 */
int wm_tile_windows(window_manager_t *wm)
{
    if (!wm) {
        return -EINVAL;
    }
    
    wm_monitor_t *mon = wm_get_primary_monitor(wm);
    rect_t work = mon ? mon->work_area : wm->virtual_desktop;
    
    /* Calculate grid */
    u32 count = 0;
    wm_window_t *win = wm->windows;
    while (win) {
        if (win->visible) count++;
        win = win->next;
    }
    
    if (count == 0) return 0;
    
    u32 cols = (count <= 2) ? 1 : 2;
    u32 rows = (count + cols - 1) / cols;
    
    s32 tile_w = work.width / cols;
    s32 tile_h = work.height / rows;
    
    u32 idx = 0;
    win = wm->windows;
    while (win && idx < count) {
        if (win->visible) {
            u32 col = idx % cols;
            u32 row = idx / cols;
            
            win->geometry.x = work.x + col * tile_w;
            win->geometry.y = work.y + row * tile_h;
            win->geometry.width = tile_w;
            win->geometry.height = tile_h;
            
            update_window_geometry(wm, win);
            idx++;
        }
        win = win->next;
    }
    
    return 0;
}

/**
 * wm_cascade_windows - Cascade windows
 * @wm: Pointer to window manager structure
 *
 * Returns: 0 on success
 */
int wm_cascade_windows(window_manager_t *wm)
{
    if (!wm) {
        return -EINVAL;
    }
    
    wm_monitor_t *mon = wm_get_primary_monitor(wm);
    rect_t work = mon ? mon->work_area : wm->virtual_desktop;
    
    s32 x = work.x + 50;
    s32 y = work.y + 50;
    s32 step = 30;
    
    wm_window_t *win = wm->windows;
    while (win) {
        if (win->visible) {
            win->geometry.x = x;
            win->geometry.y = y;
            
            x += step;
            y += step;
            
            /* Wrap around if needed */
            if (x + win->geometry.width > work.x + work.width) {
                x = work.x + 50;
            }
            if (y + win->geometry.height > work.y + work.height) {
                y = work.y + 50;
            }
            
            update_window_geometry(wm, win);
        }
        win = win->next;
    }
    
    return 0;
}

/*===========================================================================*/
/*                         WINDOW SEARCH                                     */
/*===========================================================================*/

/**
 * wm_get_window_by_id - Get window by ID
 * @wm: Pointer to window manager structure
 * @id: Window ID
 *
 * Returns: Window pointer, or NULL if not found
 */
wm_window_t *wm_get_window_by_id(window_manager_t *wm, u32 id)
{
    if (!wm) {
        return NULL;
    }
    
    wm_window_t *win = wm->windows;
    while (win) {
        if (win->window_id == id) {
            return win;
        }
        win = win->next;
    }
    
    return NULL;
}

/**
 * wm_get_window_at - Get window at point
 * @wm: Pointer to window manager structure
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: Window pointer, or NULL if none
 */
wm_window_t *wm_get_window_at(window_manager_t *wm, s32 x, s32 y)
{
    if (!wm) {
        return NULL;
    }
    
    return find_window_at_point(wm, x, y);
}

/**
 * find_window_at_point - Find window at point
 * @wm: Pointer to window manager structure
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: Window pointer, or NULL if none
 */
static wm_window_t *find_window_at_point(window_manager_t *wm, s32 x, s32 y)
{
    if (!wm) {
        return NULL;
    }
    
    /* Search from top to bottom */
    wm_window_t *win = wm->windows;
    while (win) {
        if (win->visible &&
            x >= win->geometry.x &&
            x < win->geometry.x + win->geometry.width &&
            y >= win->geometry.y &&
            y < win->geometry.y + win->geometry.height) {
            return win;
        }
        win = win->next;
    }
    
    return NULL;
}

/**
 * wm_get_focused_window - Get focused window
 * @wm: Pointer to window manager structure
 *
 * Returns: Focused window, or NULL if none
 */
wm_window_t *wm_get_focused_window(window_manager_t *wm)
{
    return wm ? wm->focused_window : NULL;
}

/**
 * wm_list_windows - List all windows
 * @wm: Pointer to window manager structure
 * @wins: Array to store windows
 * @count: Pointer to count
 *
 * Returns: 0 on success
 */
int wm_list_windows(window_manager_t *wm, wm_window_t *wins, u32 *count)
{
    if (!wm || !wins || !count) {
        return -EINVAL;
    }
    
    u32 copy_count = (*count < wm->window_count) ? *count : wm->window_count;
    
    wm_window_t *win = wm->windows;
    for (u32 i = 0; i < copy_count && win; i++) {
        memcpy(&wins[i], win, sizeof(wm_window_t));
        win = win->next;
    }
    
    *count = copy_count;
    
    return 0;
}

/*===========================================================================*/
/*                         MONITOR MANAGEMENT                                */
/*===========================================================================*/

/**
 * wm_detect_monitors - Detect monitors
 * @wm: Pointer to window manager structure
 *
 * Returns: Number of monitors detected
 */
int wm_detect_monitors(window_manager_t *wm)
{
    if (!wm) {
        return -EINVAL;
    }
    
    /* In real implementation, would query display hardware */
    /* For now, create mock primary monitor */
    
    wm_monitor_t *mon = &wm->monitors[0];
    mon->monitor_id = 0;
    strcpy(mon->name, "Primary Display");
    mon->is_primary = true;
    mon->is_active = true;
    mon->geometry.x = 0;
    mon->geometry.y = 0;
    mon->geometry.width = 1920;
    mon->geometry.height = 1080;
    mon->work_area = mon->geometry;
    mon->scale_factor = 1;
    mon->rotation = 0;
    mon->refresh_rate = 60;
    
    wm->monitor_count = 1;
    wm->primary_monitor = 0;
    
    printk("[WM] Detected %d monitor(s)\n", wm->monitor_count);
    
    return wm->monitor_count;
}

/**
 * wm_get_monitor - Get monitor by ID
 * @wm: Pointer to window manager structure
 * @id: Monitor ID
 *
 * Returns: Monitor pointer, or NULL if not found
 */
wm_monitor_t *wm_get_monitor(window_manager_t *wm, u32 id)
{
    if (!wm || id >= wm->monitor_count) {
        return NULL;
    }
    
    return &wm->monitors[id];
}

/**
 * wm_get_primary_monitor - Get primary monitor
 * @wm: Pointer to window manager structure
 *
 * Returns: Primary monitor pointer
 */
wm_monitor_t *wm_get_primary_monitor(window_manager_t *wm)
{
    if (!wm || wm->monitor_count == 0) {
        return NULL;
    }
    
    return &wm->monitors[wm->primary_monitor];
}

/**
 * wm_set_primary_monitor - Set primary monitor
 * @wm: Pointer to window manager structure
 * @id: Monitor ID
 *
 * Returns: 0 on success
 */
int wm_set_primary_monitor(window_manager_t *wm, u32 id)
{
    if (!wm || id >= wm->monitor_count) {
        return -EINVAL;
    }
    
    wm->monitors[wm->primary_monitor].is_primary = false;
    wm->primary_monitor = id;
    wm->monitors[id].is_primary = true;
    
    return 0;
}

/*===========================================================================*/
/*                         WORKSPACE MANAGEMENT                              */
/*===========================================================================*/

/**
 * wm_switch_workspace - Switch to workspace
 * @wm: Pointer to window manager structure
 * @workspace: Workspace number
 *
 * Returns: 0 on success
 */
int wm_switch_workspace(window_manager_t *wm, u32 workspace)
{
    if (!wm || workspace >= wm->workspace_count) {
        return -EINVAL;
    }
    
    /* Hide windows on current workspace */
    wm_window_t *win = wm->windows;
    while (win) {
        if (win->workspace == wm->current_workspace && win->visible) {
            wm_hide_window(wm, win);
        }
        win = win->next;
    }
    
    /* Switch workspace */
    wm->current_workspace = workspace;
    
    /* Show windows on new workspace */
    win = wm->windows;
    while (win) {
        if (win->workspace == workspace && win->visible) {
            wm_show_window(wm, win);
        }
        win = win->next;
    }
    
    if (wm->on_workspace_changed) {
        wm->on_workspace_changed(workspace);
    }
    
    return 0;
}

/**
 * wm_move_window_to_workspace - Move window to workspace
 * @wm: Pointer to window manager structure
 * @win: Window to move
 * @workspace: Workspace number
 *
 * Returns: 0 on success
 */
int wm_move_window_to_workspace(window_manager_t *wm, wm_window_t *win, u32 workspace)
{
    if (!wm || !win || workspace >= wm->workspace_count) {
        return -EINVAL;
    }
    
    win->workspace = workspace;
    
    /* Hide if not on current workspace */
    if (workspace != wm->current_workspace) {
        wm_hide_window(wm, win);
    }
    
    return 0;
}

/**
 * wm_get_workspace - Get workspace
 * @wm: Pointer to window manager structure
 * @id: Workspace ID
 *
 * Returns: Workspace pointer, or NULL if not found
 */
wm_workspace_t *wm_get_workspace(window_manager_t *wm, u32 id)
{
    if (!wm || id >= wm->workspace_count) {
        return NULL;
    }
    
    return &wm->workspaces[id];
}

/**
 * wm_rename_workspace - Rename workspace
 * @wm: Pointer to window manager structure
 * @id: Workspace ID
 * @name: New name
 *
 * Returns: 0 on success
 */
int wm_rename_workspace(window_manager_t *wm, u32 id, const char *name)
{
    if (!wm || !name || id >= wm->workspace_count) {
        return -EINVAL;
    }
    
    strncpy(wm->workspaces[id].name, name, sizeof(wm->workspaces[id].name) - 1);
    
    return 0;
}

/*===========================================================================*/
/*                         THEME                                             */
/*===========================================================================*/

/**
 * wm_set_theme - Set window manager theme
 * @wm: Pointer to window manager structure
 * @theme_name: Theme name
 *
 * Returns: 0 on success
 */
int wm_set_theme(window_manager_t *wm, const char *theme_name)
{
    if (!wm || !theme_name) {
        return -EINVAL;
    }
    
    strncpy(wm->theme_name, theme_name, sizeof(wm->theme_name) - 1);
    
    /* In real implementation, would load theme from file */
    
    return 0;
}

/**
 * wm_get_theme - Get current theme
 * @wm: Pointer to window manager structure
 *
 * Returns: Theme colors pointer
 */
window_decor_colors_t *wm_get_theme(window_manager_t *wm)
{
    return wm ? &wm->theme : NULL;
}

/**
 * init_default_theme - Initialize default theme
 * @wm: Pointer to window manager structure
 */
static void init_default_theme(window_manager_t *wm)
{
    if (!wm) return;
    
    /* Dark modern theme */
    wm->theme.title_bg = color_from_rgba(45, 45, 60, 255);
    wm->theme.title_fg = color_from_rgba(255, 255, 255, 255);
    wm->theme.border = color_from_rgba(60, 60, 80, 255);
    wm->theme.shadow = color_from_rgba(0, 0, 0, 128);
    wm->theme.active = color_from_rgba(0, 120, 215, 255);
    wm->theme.inactive = color_from_rgba(80, 80, 95, 255);
    
    strcpy(wm->theme_name, "dark-modern");
}

/**
 * init_workspaces - Initialize workspaces
 * @wm: Pointer to window manager structure
 */
static void init_workspaces(window_manager_t *wm)
{
    if (!wm) return;
    
    wm->workspace_count = WM_MAX_WORKSPACES;
    wm->current_workspace = 0;
    
    for (u32 i = 0; i < wm->workspace_count; i++) {
        wm_workspace_t *ws = &wm->workspaces[i];
        ws->workspace_id = i;
        snprintf(ws->name, sizeof(ws->name), "Workspace %d", i + 1);
        ws->is_active = (i == 0);
        ws->window_count = 0;
        ws->windows = NULL;
        ws->background = color_from_rgba(30, 30, 40, 255);
    }
}

/**
 * init_snap_layouts - Initialize snap layouts
 * @wm: Pointer to window manager structure
 */
static void init_snap_layouts(window_manager_t *wm)
{
    if (!wm) return;
    
    /* Default snap layouts */
    wm->snap_layout_count = 4;
    
    /* Left snap */
    wm->snap_layouts[0].layout_id = 0;
    strcpy(wm->snap_layouts[0].name, "Left");
    wm->snap_layouts[0].zone_count = 1;
    
    /* Right snap */
    wm->snap_layouts[1].layout_id = 1;
    strcpy(wm->snap_layouts[1].name, "Right");
    wm->snap_layouts[1].zone_count = 1;
    
    /* Top snap */
    wm->snap_layouts[2].layout_id = 2;
    strcpy(wm->snap_layouts[2].name, "Top");
    wm->snap_layouts[2].zone_count = 1;
    
    /* Bottom snap */
    wm->snap_layouts[3].layout_id = 3;
    strcpy(wm->snap_layouts[3].name, "Bottom");
    wm->snap_layouts[3].zone_count = 1;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * wm_get_state_name - Get window state name
 * @state: Window state
 *
 * Returns: State name string
 */
const char *wm_get_state_name(u32 state)
{
    switch (state) {
        case WINDOW_STATE_NORMAL:         return "Normal";
        case WINDOW_STATE_MAXIMIZED:      return "Maximized";
        case WINDOW_STATE_MINIMIZED:      return "Minimized";
        case WINDOW_STATE_FULLSCREEN:     return "Fullscreen";
        case WINDOW_STATE_SNAPPED_LEFT:   return "Snapped Left";
        case WINDOW_STATE_SNAPPED_RIGHT:  return "Snapped Right";
        case WINDOW_STATE_SNAPPED_TOP:    return "Snapped Top";
        case WINDOW_STATE_SNAPPED_BOTTOM: return "Snapped Bottom";
        case WINDOW_STATE_TILED:          return "Tiled";
        default:                          return "Unknown";
    }
}

/**
 * wm_get_version - Get window manager version
 *
 * Returns: Version string
 */
const char *wm_get_version(void)
{
    return WM_VERSION;
}

/**
 * wm_get_work_area - Get work area for monitor
 * @wm: Pointer to window manager structure
 * @mon: Monitor pointer
 *
 * Returns: Work area rectangle
 */
rect_t wm_get_work_area(window_manager_t *wm, wm_monitor_t *mon)
{
    if (!wm) {
        rect_t empty = {0, 0, 0, 0};
        return empty;
    }
    
    if (!mon) {
        mon = wm_get_primary_monitor(wm);
    }
    
    return mon ? mon->work_area : wm->virtual_desktop;
}

/**
 * wm_get_snap_rect - Get snap rectangle
 * @wm: Pointer to window manager structure
 * @snap_state: Snap state
 * @mon: Monitor pointer
 *
 * Returns: Snap rectangle
 */
rect_t wm_get_snap_rect(window_manager_t *wm, u32 snap_state, wm_monitor_t *mon)
{
    return calculate_snap_rect(wm, snap_state, mon);
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * wm_get_instance - Get global window manager instance
 *
 * Returns: Pointer to global instance
 */
window_manager_t *wm_get_instance(void)
{
    return &g_window_manager;
}
