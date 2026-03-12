/*
 * NEXUS OS - GUI Framework
 * gui/compositor/window_manager.c
 *
 * Window Manager
 *
 * This module implements the window manager which handles window
 * lifecycle, focus management, z-ordering, and window decorations.
 */

#include "../gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================*/
/*                         WINDOW MANAGER STATE                              */
/*===========================================================================*/

static struct {
    bool initialized;
    
    /* Windows */
    window_t *windows[MAX_WINDOWS];
    u32 num_windows;
    u32 next_window_id;
    
    /* Focus management */
    window_t *focused_window;
    window_t *active_window;
    window_t *capture_window;
    window_t *hover_window;
    
    /* Z-order management */
    u32 top_z_order;
    
    /* Window classes */
    struct {
        char name[64];
        void (*wnd_proc)(window_t *, u32, void *, void *);
        u32 style;
        color_t background;
    } window_classes[32];
    u32 num_classes;
    
    /* Desktop window */
    window_t *desktop_window;
    window_t *taskbar_window;
    
    /* Minimized windows */
    window_t *minimized_windows[MAX_WINDOWS];
    u32 num_minimized;
    
    /* Callbacks */
    void (*on_window_created)(window_t *);
    void (*on_window_destroyed)(window_t *);
    void (*on_window_focused)(window_t *);
    void (*on_window_minimized)(window_t *);
    void (*on_window_restored)(window_t *);
    
    /* Synchronization */
    spinlock_t lock;
} g_window_manager = {
    .initialized = false,
    .next_window_id = 1,
    .top_z_order = 0,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static window_t *window_manager_create_internal(window_props_t *props);
static void window_manager_destroy_internal(window_t *window);
static void window_manager_set_focus(window_t *window);
static void window_manager_update_z_order(window_t *window);
static window_t *window_manager_find_parent(window_t *window);
static void window_manager_notify_event(window_t *window, u32 event, void *param);

/*===========================================================================*/
/*                         WINDOW MANAGER INITIALIZATION                     */
/*===========================================================================*/

/**
 * window_manager_init - Initialize the window manager
 *
 * Returns: 0 on success, error code on failure
 */
int window_manager_init(void)
{
    if (g_window_manager.initialized) {
        pr_debug("Window manager already initialized\n");
        return 0;
    }

    spin_lock(&g_window_manager.lock);

    pr_info("Initializing window manager...\n");

    memset(&g_window_manager, 0, sizeof(g_window_manager));
    g_window_manager.next_window_id = 1;
    g_window_manager.lock.lock = 0;

    /* Register default window classes */
    window_manager_register_class("Normal", NULL, 0, (color_t)COLOR_WHITE);
    window_manager_register_class("Dialog", NULL, WINDOW_FLAG_MODAL, (color_t)COLOR_WHITE);
    window_manager_register_class("Popup", NULL, WINDOW_FLAG_POPUP | WINDOW_FLAG_BORDERLESS, 
                                   (color_t)COLOR_TRANSPARENT);

    /* Create desktop window */
    window_props_t desktop_props = {0};
    strncpy(desktop_props.title, "Desktop", sizeof(desktop_props.title) - 1);
    desktop_props.type = WINDOW_TYPE_SYSTEM;
    desktop_props.flags = WINDOW_FLAG_VISIBLE;
    desktop_props.background = (color_t){0, 100, 200, 255};  /* Blue desktop */
    
    g_window_manager.desktop_window = window_manager_create_internal(&desktop_props);

    g_window_manager.initialized = true;

    pr_info("Window manager initialized\n");

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_manager_shutdown - Shutdown the window manager
 */
void window_manager_shutdown(void)
{
    if (!g_window_manager.initialized) {
        return;
    }

    spin_lock(&g_window_manager.lock);

    pr_info("Shutting down window manager...\n");

    /* Destroy all windows */
    for (u32 i = g_window_manager.num_windows; i > 0; i--) {
        window_t *window = g_window_manager.windows[i - 1];
        if (window) {
            window_manager_destroy_internal(window);
        }
    }

    g_window_manager.initialized = false;

    pr_info("Window manager shutdown complete\n");

    spin_unlock(&g_window_manager.lock);
}

/*===========================================================================*/
/*                         WINDOW CREATION/DESTRUCTION                       */
/*===========================================================================*/

/**
 * window_create - Create a new window
 * @props: Window properties
 *
 * Creates a new window with the specified properties.
 *
 * Returns: Pointer to created window, or NULL on failure
 */
window_t *window_create(window_props_t *props)
{
    window_t *window;

    if (!g_window_manager.initialized) {
        pr_err("Window manager not initialized\n");
        return NULL;
    }

    spin_lock(&g_window_manager.lock);

    window = window_manager_create_internal(props);

    spin_unlock(&g_window_manager.lock);

    return window;
}

/**
 * window_manager_create_internal - Internal window creation
 * @props: Window properties
 *
 * Returns: Pointer to created window
 */
static window_t *window_manager_create_internal(window_props_t *props)
{
    window_t *window;

    if (g_window_manager.num_windows >= MAX_WINDOWS) {
        pr_err("Maximum window count reached\n");
        return NULL;
    }

    /* Allocate window structure */
    window = (window_t *)kzalloc(sizeof(window_t));
    if (!window) {
        pr_err("Failed to allocate window\n");
        return NULL;
    }

    /* Initialize window */
    window->window_id = g_window_manager.next_window_id++;
    window->props = *props;
    window->visible = (props->flags & WINDOW_FLAG_VISIBLE) != 0;
    window->focused = false;
    window->enabled = true;
    window->dirty = true;
    window->z_order = ++g_window_manager.top_z_order;

    /* Set default callbacks */
    window->on_create = NULL;
    window->on_destroy = NULL;
    window->on_paint = NULL;
    window->on_resize = NULL;
    window->on_move = NULL;
    window->on_focus = NULL;
    window->on_blur = NULL;
    window->on_mouse_move = NULL;
    window->on_mouse_down = NULL;
    window->on_mouse_up = NULL;
    window->on_key_down = NULL;
    window->on_key_up = NULL;

    /* Initialize reference count */
    atomic_set(&window->refcount, 1);

    /* Add to window list */
    g_window_manager.windows[g_window_manager.num_windows] = window;
    g_window_manager.num_windows++;

    /* Add to compositor */
    compositor_add_window(window);

    pr_debug("Window created: ID=%u, Title=\"%s\", Bounds=(%d,%d,%d,%d)\n",
             window->window_id, props->title,
             props->bounds.x, props->bounds.y,
             props->bounds.width, props->bounds.height);

    /* Notify creation */
    window_manager_notify_event(window, EVENT_WINDOW_CREATE, NULL);
    if (g_window_manager.on_window_created) {
        g_window_manager.on_window_created(window);
    }

    /* Auto-focus first window */
    if (g_window_manager.num_windows == 1) {
        window_focus(window);
    }

    return window;
}

/**
 * window_destroy - Destroy a window
 * @window: Window to destroy
 */
void window_destroy(window_t *window)
{
    if (!window) {
        return;
    }

    spin_lock(&g_window_manager.lock);
    window_manager_destroy_internal(window);
    spin_unlock(&g_window_manager.lock);
}

/**
 * window_manager_destroy_internal - Internal window destruction
 * @window: Window to destroy
 */
static void window_manager_destroy_internal(window_t *window)
{
    if (!window) {
        return;
    }

    pr_debug("Destroying window: ID=%u\n", window->window_id);

    /* Remove from compositor */
    compositor_remove_window(window);

    /* Remove from window list */
    for (u32 i = 0; i < g_window_manager.num_windows; i++) {
        if (g_window_manager.windows[i] == window) {
            /* Shift remaining windows */
            for (u32 j = i; j < g_window_manager.num_windows - 1; j++) {
                g_window_manager.windows[j] = g_window_manager.windows[j + 1];
            }
            g_window_manager.num_windows--;
            break;
        }
    }

    /* Update focus if needed */
    if (g_window_manager.focused_window == window) {
        g_window_manager.focused_window = NULL;
    }
    if (g_window_manager.active_window == window) {
        g_window_manager.active_window = NULL;
    }
    if (g_window_manager.capture_window == window) {
        g_window_manager.capture_window = NULL;
    }
    if (g_window_manager.hover_window == window) {
        g_window_manager.hover_window = NULL;
    }

    /* Notify destruction */
    window_manager_notify_event(window, EVENT_WINDOW_DESTROY, NULL);
    if (window->on_destroy) {
        window->on_destroy(window);
    }
    if (g_window_manager.on_window_destroyed) {
        g_window_manager.on_window_destroyed(window);
    }

    /* Free window resources */
    if (window->surface) {
        kfree(window->surface);
    }
    if (window->back_buffer) {
        kfree(window->back_buffer);
    }

    /* Free window */
    kfree(window);
}

/*===========================================================================*/
/*                         WINDOW OPERATIONS                                 */
/*===========================================================================*/

/**
 * window_show - Show a window
 * @window: Window to show
 *
 * Returns: 0 on success, error code on failure
 */
int window_show(window_t *window)
{
    if (!window) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    if (window->visible) {
        spin_unlock(&g_window_manager.lock);
        return 0;
    }

    window->visible = true;
    window->dirty = true;

    /* Remove from minimized list if present */
    for (u32 i = 0; i < g_window_manager.num_minimized; i++) {
        if (g_window_manager.minimized_windows[i] == window) {
            for (u32 j = i; j < g_window_manager.num_minimized - 1; j++) {
                g_window_manager.minimized_windows[j] = 
                    g_window_manager.minimized_windows[j + 1];
            }
            g_window_manager.num_minimized--;
            break;
        }
    }

    window->props.flags &= ~WINDOW_FLAG_MINIMIZED;

    compositor_update_window(window);
    window_manager_notify_event(window, EVENT_WINDOW_SHOW, NULL);

    pr_debug("Window %u shown\n", window->window_id);

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_hide - Hide a window
 * @window: Window to hide
 *
 * Returns: 0 on success, error code on failure
 */
int window_hide(window_t *window)
{
    if (!window) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    if (!window->visible) {
        spin_unlock(&g_window_manager.lock);
        return 0;
    }

    window->visible = false;
    window->dirty = true;

    /* Update focus if needed */
    if (g_window_manager.focused_window == window) {
        g_window_manager.focused_window = NULL;
    }

    compositor_update_window(window);
    window_manager_notify_event(window, EVENT_WINDOW_HIDE, NULL);

    pr_debug("Window %u hidden\n", window->window_id);

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_close - Close a window
 * @window: Window to close
 *
 * Returns: 0 on success, error code on failure
 */
int window_close(window_t *window)
{
    if (!window) {
        return -1;
    }

    /* In a real implementation, this would:
     * - Send close message to window
     * - Allow window to cancel close
     * - Save window state
     */

    window_destroy(window);

    return 0;
}

/**
 * window_set_title - Set window title
 * @window: Window
 * @title: New title
 *
 * Returns: 0 on success, error code on failure
 */
int window_set_title(window_t *window, const char *title)
{
    if (!window || !title) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    strncpy(window->props.title, title, sizeof(window->props.title) - 1);
    window->dirty = true;

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_set_bounds - Set window bounds
 * @window: Window
 * @x: X position
 * @y: Y position
 * @width: Width
 * @height: Height
 *
 * Returns: 0 on success, error code on failure
 */
int window_set_bounds(window_t *window, s32 x, s32 y, s32 width, s32 height)
{
    if (!window) {
        return -1;
    }

    if (width <= 0 || height <= 0) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    s32 old_x = window->props.bounds.x;
    s32 old_y = window->props.bounds.y;
    s32 old_w = window->props.bounds.width;
    s32 old_h = window->props.bounds.height;

    window->props.bounds.x = x;
    window->props.bounds.y = y;
    window->props.bounds.width = width;
    window->props.bounds.height = height;
    window->dirty = true;

    /* Calculate client rect */
    window->props.client_rect.x = 0;
    window->props.client_rect.y = 0;
    window->props.client_rect.width = width;
    window->props.client_rect.height = height;

    /* Notify resize/move */
    if (width != old_w || height != old_h) {
        window_manager_notify_event(window, EVENT_WINDOW_RESIZE, NULL);
        if (window->on_resize) {
            window->on_resize(window, width, height);
        }
    }
    if (x != old_x || y != old_y) {
        window_manager_notify_event(window, EVENT_WINDOW_MOVE, NULL);
        if (window->on_move) {
            window->on_move(window, x, y);
        }
    }

    compositor_update_window(window);

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_set_position - Set window position
 * @window: Window
 * @x: X position
 * @y: Y position
 *
 * Returns: 0 on success, error code on failure
 */
int window_set_position(window_t *window, s32 x, s32 y)
{
    return window_set_bounds(window, x, y, 
                             window->props.bounds.width,
                             window->props.bounds.height);
}

/**
 * window_set_size - Set window size
 * @window: Window
 * @width: Width
 * @height: Height
 *
 * Returns: 0 on success, error code on failure
 */
int window_set_size(window_t *window, s32 width, s32 height)
{
    return window_set_bounds(window, window->props.bounds.x,
                             window->props.bounds.y, width, height);
}

/**
 * window_get_bounds - Get window bounds
 * @window: Window
 * @rect: Output rectangle
 *
 * Returns: 0 on success, error code on failure
 */
int window_get_bounds(window_t *window, rect_t *rect)
{
    if (!window || !rect) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);
    *rect = window->props.bounds;
    spin_unlock(&g_window_manager.lock);

    return 0;
}

/*===========================================================================*/
/*                         FOCUS MANAGEMENT                                  */
/*===========================================================================*/

/**
 * window_focus - Focus a window
 * @window: Window to focus
 *
 * Returns: 0 on success, error code on failure
 */
int window_focus(window_t *window)
{
    if (!window) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);
    window_manager_set_focus(window);
    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_manager_set_focus - Internal focus setting
 * @window: Window to focus
 */
static void window_manager_set_focus(window_t *window)
{
    if (!window->visible || !window->enabled) {
        return;
    }

    /* Blur current focused window */
    if (g_window_manager.focused_window && 
        g_window_manager.focused_window != window) {
        window_t *old_focus = g_window_manager.focused_window;
        old_focus->focused = false;
        old_focus->dirty = true;
        window_manager_notify_event(old_focus, EVENT_WINDOW_BLUR, NULL);
        if (old_focus->on_blur) {
            old_focus->on_blur(old_focus);
        }
    }

    /* Focus new window */
    window->focused = true;
    window->dirty = true;
    g_window_manager.focused_window = window;
    g_window_manager.active_window = window;

    /* Bring to front */
    window_manager_update_z_order(window);

    window_manager_notify_event(window, EVENT_WINDOW_FOCUS, NULL);
    if (window->on_focus) {
        window->on_focus(window);
    }
    if (g_window_manager.on_window_focused) {
        g_window_manager.on_window_focused(window);
    }

    pr_debug("Window %u focused\n", window->window_id);
}

/**
 * window_blur - Remove focus from a window
 * @window: Window to blur
 *
 * Returns: 0 on success, error code on failure
 */
int window_blur(window_t *window)
{
    if (!window) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    if (g_window_manager.focused_window == window) {
        window->focused = false;
        window->dirty = true;
        g_window_manager.focused_window = NULL;
        
        window_manager_notify_event(window, EVENT_WINDOW_BLUR, NULL);
        if (window->on_blur) {
            window->on_blur(window);
        }
    }

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_manager_update_z_order - Update window z-order
 * @window: Window
 */
static void window_manager_update_z_order(window_t *window)
{
    /* Bring window to front */
    window->z_order = ++g_window_manager.top_z_order;
    window->props.flags |= WINDOW_FLAG_TOPMOST;
}

/*===========================================================================*/
/*                         WINDOW LOOKUP                                     */
/*===========================================================================*/

/**
 * window_find_by_id - Find window by ID
 * @window_id: Window ID
 *
 * Returns: Pointer to window, or NULL if not found
 */
window_t *window_find_by_id(u32 window_id)
{
    if (!g_window_manager.initialized) {
        return NULL;
    }

    spin_lock(&g_window_manager.lock);

    for (u32 i = 0; i < g_window_manager.num_windows; i++) {
        if (g_window_manager.windows[i]->window_id == window_id) {
            window_t *window = g_window_manager.windows[i];
            spin_unlock(&g_window_manager.lock);
            return window;
        }
    }

    spin_unlock(&g_window_manager.lock);
    return NULL;
}

/**
 * window_find_at - Find window at position
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: Topmost window at position, or NULL
 */
window_t *window_find_at(s32 x, s32 y)
{
    if (!g_window_manager.initialized) {
        return NULL;
    }

    spin_lock(&g_window_manager.lock);

    window_t *found = NULL;
    u32 max_z_order = 0;

    for (u32 i = 0; i < g_window_manager.num_windows; i++) {
        window_t *window = g_window_manager.windows[i];
        
        if (!window->visible) {
            continue;
        }

        rect_t *bounds = &window->props.bounds;
        if (x >= bounds->x && x < bounds->x + bounds->width &&
            y >= bounds->y && y < bounds->y + bounds->height) {
            
            if (window->z_order > max_z_order) {
                max_z_order = window->z_order;
                found = window;
            }
        }
    }

    spin_unlock(&g_window_manager.lock);
    return found;
}

/*===========================================================================*/
/*                         WINDOW CLASSES                                    */
/*===========================================================================*/

/**
 * window_manager_register_class - Register a window class
 * @name: Class name
 * @wnd_proc: Window procedure
 * @style: Default style
 * @background: Default background color
 *
 * Returns: 0 on success, error code on failure
 */
int window_manager_register_class(const char *name, 
                                   void (*wnd_proc)(window_t *, u32, void *, void *),
                                   u32 style, color_t background)
{
    if (!name || g_window_manager.num_classes >= 32) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    struct {
        char name[64];
        void (*wnd_proc)(window_t *, u32, void *, void *);
        u32 style;
        color_t background;
    } *cls = &g_window_manager.window_classes[g_window_manager.num_classes];

    strncpy(cls->name, name, sizeof(cls->name) - 1);
    cls->wnd_proc = wnd_proc;
    cls->style = style;
    cls->background = background;
    g_window_manager.num_classes++;

    pr_debug("Window class registered: %s\n", name);

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/*===========================================================================*/
/*                         EVENT NOTIFICATION                                */
/*===========================================================================*/

/**
 * window_manager_notify_event - Notify window of event
 * @window: Window
 * @event: Event type
 * @param: Event parameter
 */
static void window_manager_notify_event(window_t *window, u32 event, void *param)
{
    /* Create input event */
    input_event_t input_event = {0};
    input_event.type = event;
    input_event.timestamp = get_time_ms();
    input_event.window_id = window->window_id;

    /* Post to input queue */
    /* (In a real implementation, this would use proper event queue) */
    (void)input_event;
    (void)param;
}

/**
 * window_manager_dispatch_event - Dispatch event to window
 * @window: Window
 * @event: Event to dispatch
 */
void window_manager_dispatch_event(window_t *window, input_event_t *event)
{
    if (!window || !event) {
        return;
    }

    spin_lock(&g_window_manager.lock);

    switch (event->type) {
        case EVENT_MOUSE_MOVE:
            if (window->on_mouse_move) {
                window->on_mouse_move(window, event->mouse.x, event->mouse.y,
                                       event->mouse.buttons);
            }
            break;

        case EVENT_MOUSE_DOWN:
            if (window->on_mouse_down) {
                window->on_mouse_down(window, event->mouse.x, event->mouse.y,
                                       event->mouse.buttons);
            }
            /* Focus window on click */
            if (event->mouse.buttons & MOUSE_BUTTON_LEFT) {
                window_manager_set_focus(window);
            }
            break;

        case EVENT_MOUSE_UP:
            if (window->on_mouse_up) {
                window->on_mouse_up(window, event->mouse.x, event->mouse.y,
                                     event->mouse.buttons);
            }
            break;

        case EVENT_KEY_DOWN:
            if (window->on_key_down) {
                window->on_key_down(window, event->keyboard.key_code,
                                     event->keyboard.modifiers);
            }
            break;

        case EVENT_KEY_UP:
            if (window->on_key_up) {
                window->on_key_up(window, event->keyboard.key_code,
                                   event->keyboard.modifiers);
            }
            break;

        default:
            break;
    }

    spin_unlock(&g_window_manager.lock);
}

/*===========================================================================*/
/*                         MINIMIZE/RESTORE                                  */
/*===========================================================================*/

/**
 * window_minimize - Minimize a window
 * @window: Window to minimize
 *
 * Returns: 0 on success, error code on failure
 */
int window_minimize(window_t *window)
{
    if (!window) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    if (window->props.flags & WINDOW_FLAG_MINIMIZED) {
        spin_unlock(&g_window_manager.lock);
        return 0;
    }

    window->props.flags |= WINDOW_FLAG_MINIMIZED;
    window->visible = false;

    /* Add to minimized list */
    if (g_window_manager.num_minimized < MAX_WINDOWS) {
        g_window_manager.minimized_windows[g_window_manager.num_minimized] = window;
        g_window_manager.num_minimized++;
    }

    compositor_update_window(window);

    if (g_window_manager.on_window_minimized) {
        g_window_manager.on_window_minimized(window);
    }

    pr_debug("Window %u minimized\n", window->window_id);

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/**
 * window_restore - Restore a minimized window
 * @window: Window to restore
 *
 * Returns: 0 on success, error code on failure
 */
int window_restore(window_t *window)
{
    if (!window) {
        return -1;
    }

    spin_lock(&g_window_manager.lock);

    if (!(window->props.flags & WINDOW_FLAG_MINIMIZED)) {
        spin_unlock(&g_window_manager.lock);
        return 0;
    }

    window->props.flags &= ~WINDOW_FLAG_MINIMIZED;

    /* Remove from minimized list */
    for (u32 i = 0; i < g_window_manager.num_minimized; i++) {
        if (g_window_manager.minimized_windows[i] == window) {
            for (u32 j = i; j < g_window_manager.num_minimized - 1; j++) {
                g_window_manager.minimized_windows[j] = 
                    g_window_manager.minimized_windows[j + 1];
            }
            g_window_manager.num_minimized--;
            break;
        }
    }

    window_show(window);

    if (g_window_manager.on_window_restored) {
        g_window_manager.on_window_restored(window);
    }

    pr_debug("Window %u restored\n", window->window_id);

    spin_unlock(&g_window_manager.lock);

    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * window_manager_get_stats - Get window manager statistics
 * @num_windows: Output window count
 * @num_minimized: Output minimized count
 * @focused_window_id: Output focused window ID
 */
void window_manager_get_stats(u32 *num_windows, u32 *num_minimized, u32 *focused_window_id)
{
    spin_lock(&g_window_manager.lock);

    if (num_windows) *num_windows = g_window_manager.num_windows;
    if (num_minimized) *num_minimized = g_window_manager.num_minimized;
    if (focused_window_id && g_window_manager.focused_window) {
        *focused_window_id = g_window_manager.focused_window->window_id;
    }

    spin_unlock(&g_window_manager.lock);
}
