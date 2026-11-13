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

/*
 * A window can carry a widget tree in user_data. Widgets are laid out in local
 * coordinates, so the tree is painted with the window origin as its base.
 * widget_t is opaque here on purpose: this file only needs to know that the
 * paint entry exists.
 */
typedef struct widget widget_t;
extern void widget_paint_tree(widget_t *widget, graphics_context_t *ctx,
                              s32 origin_x, s32 origin_y);

/* Forward declaration: defined further down this file. */
int window_manager_register_class(const char *name,
                                   void (*wnd_proc)(window_t *, u32, void *, void *),
                                   u32 style, color_t background);

/*===========================================================================*/
/*                         WINDOW MANAGER STATE                              */
/*===========================================================================*/

typedef struct window_class {
    char name[64];
    void (*wnd_proc)(window_t *, u32, void *, void *);
    u32 style;
    color_t background;
} window_class_t;

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
    window_class_t window_classes[32];
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

    pr_info("Initializing window manager...\n");

    /* Zero the whole structure BEFORE taking its lock.
     *
     * memset used to run while g_window_manager.lock was held. That wipes the
     * lock word (and the bookkeeping spinlock_init would have set up) out from
     * under the caller, so the matching spin_unlock at the end released a lock
     * nobody held. Reset first, then initialise the lock properly, and only
     * then take it. */
    memset(&g_window_manager, 0, sizeof(g_window_manager));
    g_window_manager.next_window_id = 1;
    spinlock_init(&g_window_manager.lock);

    /* Register default window classes.
     *
     * Done WITHOUT the manager lock held: window_manager_register_class()
     * takes it itself, and spinlock_t is not recursive, so holding it across
     * these calls deadlocked the boot the moment the window manager was
     * actually initialised. */
    window_manager_register_class("Normal", NULL, 0, (color_t)COLOR_WHITE);
    window_manager_register_class("Dialog", NULL, WINDOW_FLAG_MODAL, (color_t)COLOR_WHITE);
    window_manager_register_class("Popup", NULL, WINDOW_FLAG_POPUP | WINDOW_FLAG_BORDERLESS, 
                                   (color_t)COLOR_TRANSPARENT);

    /*
     * Create the desktop window.
     *
     * It is the bottom of the z-order and the thing that guarantees a defined
     * background, so it has to cover the screen. Its bounds used to be left at
     * zero, so it was created as a 0x0 window at the origin and painted
     * nothing at all - which is why the screen stayed black even once the
     * windows were finally being drawn.
     */
    rect_t screen = {0, 0, 1024, 768};
    boot_params_t *bp = get_boot_params();
    if (bp && bp->gfx_width && bp->gfx_height) {
        screen.width = (s32)bp->gfx_width;
        screen.height = (s32)bp->gfx_height;
    }

    window_props_t desktop_props = {0};
    strncpy(desktop_props.title, "Desktop", sizeof(desktop_props.title) - 1);
    desktop_props.type = WINDOW_TYPE_SYSTEM;
    desktop_props.flags = WINDOW_FLAG_VISIBLE;
    desktop_props.bounds = screen;
    desktop_props.background = (color_t){{{0, 100, 200, 255}}};  /* Blue desktop */
    
    /* Publish the flag before creating the desktop window: the flag is what
     * makes the manager usable, and create_internal needs it set. */
    g_window_manager.initialized = true;

    g_window_manager.desktop_window = window_manager_create_internal(&desktop_props);
    if (!g_window_manager.desktop_window) {
        pr_err("Window manager: failed to create desktop window\n");
        g_window_manager.initialized = false;
        return -1;
    }

    pr_info("Window manager initialized\n");

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

    /* Auto-focus first window.
     *
     * Calls the internal variant on purpose: the public window_focus() takes
     * g_window_manager.lock, which window_create() already holds when it gets
     * here, and spinlock_t is not recursive - the first window created through
     * the public API would have locked the machine. */
    if (g_window_manager.num_windows == 1) {
        window_manager_set_focus(window);
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

    /*
     * Bring the whole owning stack to front. Focusing a child used to raise
     * only the child itself, leaving it drawn underneath unrelated top-level
     * windows. Walking to the top-level ancestor is what this walk exists for.
     */
    {
        window_t *top = window_manager_find_parent(window);

        window_manager_update_z_order(top ? top : window);

        if (top && top != window) {
            top->dirty = true;
        }
    }

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

    window_class_t *cls = &g_window_manager.window_classes[g_window_manager.num_classes];

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
 * window_manager_find_parent - Walk up from a window to its top-level owner
 * @window: Window to start from
 *
 * Declared here and never defined, which is why nothing could reason about
 * ownership: z-order promotion, focus and teardown all need to know which
 * top-level window a child belongs to.
 *
 * Returns: The top-level ancestor, or @window itself if it has no parent.
 */
static window_t *window_manager_find_parent(window_t *window)
{
    window_t *cur = window;

    if (!window) {
        return NULL;
    }

    /* Bounded: a cycle in the parent chain must not hang the compositor. */
    for (u32 depth = 0; cur->parent && depth < MAX_WINDOWS; depth++) {
        cur = cur->parent;
    }

    return cur;
}

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

    /* Restore visibility inline rather than through window_show(): that
     * function takes g_window_manager.lock, and we are already holding it.
     * spinlock_t is not recursive, so calling it here deadlocked the first
     * time a minimised window was restored. */
    window->visible = true;
    window->dirty = true;
    window->z_order = ++g_window_manager.top_z_order;

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

/*===========================================================================*/
/*                            DISPLAY OUTPUT                                 */
/*===========================================================================*/

/*
 * Nothing in the tree ever called window->on_paint, so windows never drew a
 * single pixel: the compositor sat on a separate, unused layer stack that no
 * window was ever registered with, and init.c called compositor_present()
 * directly, which only swaps and memcpy's a framebuffer nobody had rendered
 * into. The result was the black screen after a perfectly healthy boot.
 *
 * These two functions close that loop. The window manager owns the window
 * list and its z-order, so it is the right place to walk them back-to-front.
 * Window #1 is the desktop, which fills the screen; that guarantees a defined
 * background even when no other window has painted anything yet.
 */

/**
 * window_manager_render - Draw every visible window into a framebuffer
 * @fb:    Destination, 0xAARRGGBB pixels
 * @pitch: Pixels per row of @fb (NOT bytes)
 * @fb_w:  Visible width
 * @fb_h:  Visible height
 *
 * Returns: 0 on success, -EINVAL on a bad framebuffer
 *
 * Window #1 is the desktop and covers the screen, so a frame always starts
 * from a defined background even after a resize or a cleared buffer.
 *
 * Each window is painted ONCE, in z-order, into one shared context that spans
 * the whole screen. The context's clip rectangle is narrowed to the window's
 * bounds, so a widget that overflows its window is clipped to it by the
 * rasteriser instead of scribbling over its neighbours.
 *
 * An earlier version of this walked the screen one scanline at a time and
 * built a fresh graphics context for every window on every row. That is 768
 * allocations per frame and the entire widget tree traversed 768 times, which
 * would have made the renderer unusable even once the pixels were correct.
 */
int window_manager_render(u32 *fb, u32 pitch, u32 fb_w, u32 fb_h)
{
    if (!fb || !pitch || !fb_w || !fb_h) {
        return -EINVAL;
    }

    graphics_context_t *ctx = graphics_create_context(fb);
    if (!ctx) {
        return -ENOMEM;
    }

    ctx->width = pitch;
    ctx->height = fb_h;

    s32 screen_w = (s32)fb_w;
    s32 screen_h = (s32)fb_h;

    for (u32 i = 0; i < g_window_manager.num_windows; i++) {
        window_t *w = g_window_manager.windows[i];

        /*
         * No w->surface check here: window_t.surface is declared but nothing
         * in the tree ever assigns it, so requiring it would skip every window
         * and produce a black screen. Windows are drawn straight to the
         * framebuffer; a per-window backing store is not implemented yet.
         */
        if (!w || !w->visible || !w->enabled) {
            continue;
        }

        rect_t b = w->props.bounds;
        if (b.width <= 0 || b.height <= 0) {
            continue;
        }

        s32 x0 = b.x;
        s32 y0 = b.y;
        s32 x1 = b.x + b.width;
        s32 y1 = b.y + b.height;

        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > screen_w) x1 = screen_w;
        if (y1 > screen_h) y1 = screen_h;
        if (x1 <= x0 || y1 <= y0) {
            continue;
        }

        /* Clip everything this window draws to the window rectangle. */
        ctx->clip_rect = (rect_t){x0, y0, x1 - x0, y1 - y0};

        /* Background, so a window with no widgets still shows its colour. */
        color_t bg = w->props.background;
        if (bg.a != 0) {
            rect_t full = {x0, y0, x1 - x0, y1 - y0};
            graphics_fill_rect(ctx, &full, bg);
        }

        if (w->user_data) {
            widget_paint_tree((widget_t *)w->user_data, ctx, b.x, b.y);
        }

        if (w->on_paint) {
            w->on_paint(w, ctx);
        }
    }

    graphics_destroy_context(ctx);

    return 0;
}
