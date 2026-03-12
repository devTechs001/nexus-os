/*
 * NEXUS OS - GUI Framework
 * gui/widgets/widget.c
 *
 * Widget Base Class
 *
 * This module implements the base widget class which provides
 * common functionality for all UI controls in the NEXUS GUI framework.
 */

#include "../gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================*/
/*                         WIDGET CLASS DEFINITION                           */
/*===========================================================================*/

/**
 * Widget Structure (extends window)
 */
typedef struct widget {
    /* Inherit from window */
    window_t window;
    
    /* Widget-specific properties */
    char widget_type[32];       /* Widget type name */
    char name[64];              /* Widget name */
    char text[256];             /* Widget text/label */
    bool enabled;               /* Widget enabled */
    bool visible;               /* Widget visible */
    bool focused;               /* Widget has focus */
    bool hovered;               /* Mouse is over widget */
    bool pressed;               /* Widget is pressed */
    bool checked;               /* Check state (for checkboxes) */
    
    /* Appearance */
    color_t foreground;         /* Foreground color */
    color_t background;         /* Background color */
    color_t border_color;       /* Border color */
    u32 border_width;           /* Border width */
    u32 corner_radius;          /* Corner radius */
    u32 padding;                /* Internal padding */
    u32 margin;                 /* External margin */
    
    /* Font */
    u32 font_id;                /* Font ID */
    u32 font_size;              /* Font size */
    
    /* Layout */
    u32 layout_type;            /* Layout type */
    u32 horizontal_align;       /* Horizontal alignment */
    u32 vertical_align;         /* Vertical alignment */
    
    /* Children */
    struct widget *children;    /* Child widgets */
    struct widget *next_sibling;
    struct widget *prev_sibling;
    u32 child_count;
    
    /* Parent */
    struct widget *parent;
    
    /* Event handlers */
    void (*on_click)(struct widget *);
    void (*on_double_click)(struct widget *);
    void (*on_mouse_enter)(struct widget *);
    void (*on_mouse_leave)(struct widget *);
    void (*on_mouse_down)(struct widget *, s32, s32);
    void (*on_mouse_up)(struct widget *, s32, s32);
    void (*on_mouse_move)(struct widget *, s32, s32);
    void (*on_key_down)(struct widget *, u32);
    void (*on_key_up)(struct widget *, u32);
    void (*on_text_changed)(struct widget *, const char *);
    void (*on_paint)(struct widget *, graphics_context_t *);
    
    /* User data */
    void *user_data;
    void (*user_data_free)(void *);
    
    /* Tag */
    u32 tag;
    
    /* Animation */
    animation_t *animation;
    
    /* Reference counting */
    atomic_t refcount;
} widget_t;

/*===========================================================================*/
/*                         WIDGET CLASS REGISTRY                             */
/*===========================================================================*/

static struct {
    char name[32];
    widget_t *(*create)(void);
} g_widget_classes[32];
static u32 g_num_widget_classes = 0;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static void widget_default_paint(widget_t *widget, graphics_context_t *ctx);
static void widget_default_on_mouse_down(widget_t *widget, s32 x, s32 y);
static void widget_default_on_mouse_up(widget_t *widget, s32 x, s32 y);
static void widget_default_on_mouse_enter(widget_t *widget);
static void widget_default_on_mouse_leave(widget_t *widget);
static void widget_on_window_paint(window_t *window, void *dc);
static void widget_on_window_mouse_down(window_t *window, s32 x, s32 y, u32 buttons);
static void widget_on_window_mouse_up(window_t *window, s32 x, s32 y, u32 buttons);
static void widget_on_window_mouse_move(window_t *window, s32 x, s32 y, u32 buttons);
static void widget_on_window_key_down(window_t *window, u32 key, u32 modifiers);

/*===========================================================================*/
/*                         WIDGET CREATION/DESTRUCTION                       */
/*===========================================================================*/

/**
 * widget_create - Create a new widget
 * @type: Widget type
 * @parent: Parent widget
 *
 * Creates a new widget of the specified type.
 *
 * Returns: Pointer to created widget, or NULL on failure
 */
widget_t *widget_create(const char *type, widget_t *parent)
{
    widget_t *widget = NULL;

    /* Find widget class */
    for (u32 i = 0; i < g_num_widget_classes; i++) {
        if (strcmp(g_widget_classes[i].name, type) == 0) {
            widget = g_widget_classes[i].create();
            break;
        }
    }

    /* Default to base widget if type not found */
    if (!widget) {
        widget = (widget_t *)kzalloc(sizeof(widget_t));
        if (!widget) {
            return NULL;
        }
        strncpy(widget->widget_type, "Widget", sizeof(widget->widget_type) - 1);
    }

    /* Initialize widget */
    widget->enabled = true;
    widget->visible = true;
    widget->focused = false;
    widget->hovered = false;
    widget->pressed = false;
    widget->checked = false;

    /* Default colors */
    widget->foreground = (color_t)COLOR_BLACK;
    widget->background = (color_t)COLOR_WHITE;
    widget->border_color = (color_t){128, 128, 128, 255};
    widget->border_width = 1;
    widget->corner_radius = 0;
    widget->padding = 4;
    widget->margin = 0;

    /* Default font */
    widget->font_size = 12;

    /* Default alignment */
    widget->horizontal_align = 0;  /* Left */
    widget->vertical_align = 0;    /* Top */

    /* Parent */
    widget->parent = parent;
    widget->children = NULL;
    widget->next_sibling = NULL;
    widget->prev_sibling = NULL;
    widget->child_count = 0;

    /* Default event handlers */
    widget->on_click = NULL;
    widget->on_double_click = NULL;
    widget->on_mouse_enter = widget_default_on_mouse_enter;
    widget->on_mouse_leave = widget_default_on_mouse_leave;
    widget->on_mouse_down = widget_default_on_mouse_down;
    widget->on_mouse_up = widget_default_on_mouse_up;
    widget->on_mouse_move = NULL;
    widget->on_key_down = NULL;
    widget->on_key_up = NULL;
    widget->on_text_changed = NULL;
    widget->on_paint = widget_default_paint;

    widget->user_data = NULL;
    widget->user_data_free = NULL;
    widget->tag = 0;
    widget->animation = NULL;

    atomic_set(&widget->refcount, 1);

    /* Add to parent if specified */
    if (parent) {
        widget_add_child(parent, widget);
    }

    return widget;
}

/**
 * widget_destroy - Destroy a widget
 * @widget: Widget to destroy
 */
void widget_destroy(widget_t *widget)
{
    if (!widget) {
        return;
    }

    /* Remove from parent */
    if (widget->parent) {
        widget_remove_child(widget->parent, widget);
    }

    /* Destroy children */
    widget_t *child = widget->children;
    while (child) {
        widget_t *next = child->next_sibling;
        widget_destroy(child);
        child = next;
    }

    /* Free user data */
    if (widget->user_data && widget->user_data_free) {
        widget->user_data_free(widget->user_data);
    }

    /* Free animation */
    if (widget->animation) {
        animation_destroy(widget->animation);
    }

    kfree(widget);
}

/*===========================================================================*/
/*                         WIDGET INITIALIZATION                             */
/*===========================================================================*/

/**
 * widget_init - Initialize a widget
 * @widget: Widget to initialize
 * @type: Widget type
 * @name: Widget name
 * @text: Widget text
 * @bounds: Widget bounds
 *
 * Returns: 0 on success, error code on failure
 */
int widget_init(widget_t *widget, const char *type, const char *name,
                const char *text, rect_t *bounds)
{
    if (!widget) {
        return -1;
    }

    strncpy(widget->widget_type, type ? type : "Widget", sizeof(widget->widget_type) - 1);
    
    if (name) {
        strncpy(widget->name, name, sizeof(widget->name) - 1);
    }
    
    if (text) {
        strncpy(widget->text, text, sizeof(widget->text) - 1);
    }

    /* Create underlying window */
    window_props_t props = {0};
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_CHILD | WINDOW_FLAG_VISIBLE;
    props.bounds = *bounds;
    props.background = widget->background;
    
    /* Initialize window part */
    widget->window.window_id = 0;
    widget->window.props = props;
    widget->window.visible = true;
    widget->window.focused = false;
    widget->window.enabled = true;
    widget->window.dirty = true;
    widget->window.on_paint = widget_on_window_paint;
    widget->window.on_mouse_down = widget_on_window_mouse_down;
    widget->window.on_mouse_up = widget_on_window_mouse_up;
    widget->window.on_mouse_move = widget_on_window_mouse_move;
    widget->window.on_key_down = widget_on_window_key_down;
    widget->window.user_data = widget;  /* Link back to widget */

    return 0;
}

/*===========================================================================*/
/*                         WIDGET HIERARCHY                                  */
/*===========================================================================*/

/**
 * widget_add_child - Add a child widget
 * @parent: Parent widget
 * @child: Child widget
 *
 * Returns: 0 on success, error code on failure
 */
int widget_add_child(widget_t *parent, widget_t *child)
{
    if (!parent || !child) {
        return -1;
    }

    child->parent = parent;
    child->next_sibling = parent->children;
    
    if (parent->children) {
        parent->children->prev_sibling = child;
    }
    
    parent->children = child;
    parent->child_count++;

    return 0;
}

/**
 * widget_remove_child - Remove a child widget
 * @parent: Parent widget
 * @child: Child widget
 *
 * Returns: 0 on success, error code on failure
 */
int widget_remove_child(widget_t *parent, widget_t *child)
{
    if (!parent || !child) {
        return -1;
    }

    if (child->prev_sibling) {
        child->prev_sibling->next_sibling = child->next_sibling;
    } else {
        parent->children = child->next_sibling;
    }
    
    if (child->next_sibling) {
        child->next_sibling->prev_sibling = child->prev_sibling;
    }

    child->parent = NULL;
    child->next_sibling = NULL;
    child->prev_sibling = NULL;
    parent->child_count--;

    return 0;
}

/**
 * widget_find_child - Find a child widget by name
 * @parent: Parent widget
 * @name: Widget name
 *
 * Returns: Pointer to child widget, or NULL if not found
 */
widget_t *widget_find_child(widget_t *parent, const char *name)
{
    if (!parent || !name) {
        return NULL;
    }

    widget_t *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

/**
 * widget_find_by_point - Find widget at point
 * @widget: Root widget
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: Widget at point, or NULL
 */
widget_t *widget_find_by_point(widget_t *widget, s32 x, s32 y)
{
    if (!widget || !widget->visible) {
        return NULL;
    }

    rect_t bounds = widget->window.props.bounds;
    
    /* Check if point is within widget */
    if (x < bounds.x || x >= bounds.x + bounds.width ||
        y < bounds.y || y >= bounds.y + bounds.height) {
        return NULL;
    }

    /* Check children first (topmost) */
    widget_t *child = widget->children;
    while (child) {
        widget_t *found = widget_find_by_point(child, x, y);
        if (found) {
            return found;
        }
        child = child->next_sibling;
    }

    return widget;
}

/*===========================================================================*/
/*                         WIDGET PROPERTIES                                 */
/*===========================================================================*/

/**
 * widget_set_text - Set widget text
 * @widget: Widget
 * @text: Text
 *
 * Returns: 0 on success, error code on failure
 */
int widget_set_text(widget_t *widget, const char *text)
{
    if (!widget || !text) {
        return -1;
    }

    strncpy(widget->text, text, sizeof(widget->text) - 1);
    widget->window.dirty = true;

    return 0;
}

/**
 * widget_get_text - Get widget text
 * @widget: Widget
 *
 * Returns: Widget text
 */
const char *widget_get_text(widget_t *widget)
{
    if (!widget) {
        return "";
    }
    return widget->text;
}

/**
 * widget_set_enabled - Set widget enabled state
 * @widget: Widget
 * @enabled: Enabled state
 *
 * Returns: 0 on success, error code on failure
 */
int widget_set_enabled(widget_t *widget, bool enabled)
{
    if (!widget) {
        return -1;
    }

    widget->enabled = enabled;
    widget->window.enabled = enabled;
    widget->window.dirty = true;

    if (!enabled && widget->focused) {
        widget->focused = false;
    }

    return 0;
}

/**
 * widget_is_enabled - Check if widget is enabled
 * @widget: Widget
 *
 * Returns: true if enabled, false otherwise
 */
bool widget_is_enabled(widget_t *widget)
{
    return widget ? widget->enabled : false;
}

/**
 * widget_set_visible - Set widget visibility
 * @widget: Widget
 * @visible: Visibility
 *
 * Returns: 0 on success, error code on failure
 */
int widget_set_visible(widget_t *widget, bool visible)
{
    if (!widget) {
        return -1;
    }

    widget->visible = visible;
    widget->window.visible = visible;
    
    if (visible) {
        window_show(&widget->window);
    } else {
        window_hide(&widget->window);
    }

    return 0;
}

/**
 * widget_set_bounds - Set widget bounds
 * @widget: Widget
 * @x: X position
 * @y: Y position
 * @width: Width
 * @height: Height
 *
 * Returns: 0 on success, error code on failure
 */
int widget_set_bounds(widget_t *widget, s32 x, s32 y, s32 width, s32 height)
{
    if (!widget) {
        return -1;
    }

    return window_set_bounds(&widget->window, x, y, width, height);
}

/**
 * widget_set_colors - Set widget colors
 * @widget: Widget
 * @foreground: Foreground color
 * @background: Background color
 *
 * Returns: 0 on success, error code on failure
 */
int widget_set_colors(widget_t *widget, color_t foreground, color_t background)
{
    if (!widget) {
        return -1;
    }

    widget->foreground = foreground;
    widget->background = background;
    widget->window.props.background = background;
    widget->window.dirty = true;

    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLING                                    */
/*===========================================================================*/

/**
 * widget_handle_event - Handle widget event
 * @widget: Widget
 * @event: Event
 *
 * Returns: 0 if handled, 1 if not handled
 */
int widget_handle_event(widget_t *widget, input_event_t *event)
{
    if (!widget || !event) {
        return 1;
    }

    if (!widget->enabled || !widget->visible) {
        return 1;
    }

    switch (event->type) {
        case EVENT_MOUSE_DOWN:
            if (widget->on_mouse_down) {
                widget->on_mouse_down(widget, event->mouse.x, event->mouse.y);
            }
            return 0;

        case EVENT_MOUSE_UP:
            if (widget->on_mouse_up) {
                widget->on_mouse_up(widget, event->mouse.x, event->mouse.y);
            }
            return 0;

        case EVENT_MOUSE_MOVE:
            if (widget->on_mouse_move) {
                widget->on_mouse_move(widget, event->mouse.x, event->mouse.y);
            }
            return 0;

        case EVENT_KEY_DOWN:
            if (widget->on_key_down) {
                widget->on_key_down(widget, event->keyboard.key_code);
            }
            return 0;

        case EVENT_KEY_UP:
            if (widget->on_key_up) {
                widget->on_key_up(widget, event->keyboard.key_code);
            }
            return 0;

        default:
            return 1;
    }
}

/**
 * widget_on_window_paint - Window paint callback
 * @window: Window
 * @dc: Device context
 */
static void widget_on_window_paint(window_t *window, void *dc)
{
    widget_t *widget = (widget_t *)window->user_data;
    if (!widget) {
        return;
    }

    graphics_context_t *ctx = (graphics_context_t *)dc;
    if (widget->on_paint) {
        widget->on_paint(widget, ctx);
    } else {
        widget_default_paint(widget, ctx);
    }
}

/**
 * widget_on_window_mouse_down - Window mouse down callback
 * @window: Window
 * @x: X position
 * @y: Y position
 * @buttons: Button state
 */
static void widget_on_window_mouse_down(window_t *window, s32 x, s32 y, u32 buttons)
{
    widget_t *widget = (widget_t *)window->user_data;
    if (!widget) {
        return;
    }

    widget->pressed = true;
    widget->window.dirty = true;

    if (widget->on_mouse_down) {
        widget->on_mouse_down(widget, x, y);
    }
}

/**
 * widget_on_window_mouse_up - Window mouse up callback
 * @window: Window
 * @x: X position
 * @y: Y position
 * @buttons: Button state
 */
static void widget_on_window_mouse_up(window_t *window, s32 x, s32 y, u32 buttons)
{
    widget_t *widget = (widget_t *)window->user_data;
    if (!widget) {
        return;
    }

    if (widget->pressed) {
        /* Trigger click if released over widget */
        rect_t bounds = window->props.bounds;
        if (x >= bounds.x && x < bounds.x + bounds.width &&
            y >= bounds.y && y < bounds.y + bounds.height) {
            if (widget->on_click) {
                widget->on_click(widget);
            }
        }
    }

    widget->pressed = false;
    widget->window.dirty = true;

    if (widget->on_mouse_up) {
        widget->on_mouse_up(widget, x, y);
    }
}

/**
 * widget_on_window_mouse_move - Window mouse move callback
 * @window: Window
 * @x: X position
 * @y: Y position
 * @buttons: Button state
 */
static void widget_on_window_mouse_move(window_t *window, s32 x, s32 y, u32 buttons)
{
    widget_t *widget = (widget_t *)window->user_data;
    if (!widget) {
        return;
    }

    if (widget->on_mouse_move) {
        widget->on_mouse_move(widget, x, y);
    }
}

/**
 * widget_on_window_key_down - Window key down callback
 * @window: Window
 * @key: Key code
 * @modifiers: Modifiers
 */
static void widget_on_window_key_down(window_t *window, u32 key, u32 modifiers)
{
    widget_t *widget = (widget_t *)window->user_data;
    if (!widget) {
        return;
    }

    /* Handle space/enter for button activation */
    if ((key == 32 || key == 13) && widget->on_click) {
        widget->on_click(widget);
        return;
    }

    if (widget->on_key_down) {
        widget->on_key_down(widget, key);
    }
}

/*===========================================================================*/
/*                         DEFAULT IMPLEMENTATIONS                           */
/*===========================================================================*/

/**
 * widget_default_paint - Default paint implementation
 * @widget: Widget
 * @ctx: Graphics context
 */
static void widget_default_paint(widget_t *widget, graphics_context_t *ctx)
{
    if (!widget || !ctx) {
        return;
    }

    rect_t bounds = widget->window.props.bounds;

    /* Fill background */
    graphics_fill_rect(ctx, &bounds, widget->background);

    /* Draw border */
    if (widget->border_width > 0) {
        graphics_draw_rect(ctx, &bounds);
    }

    /* Draw text */
    if (widget->text[0]) {
        s32 text_x = bounds.x + widget->padding;
        s32 text_y = bounds.y + widget->padding;
        graphics_draw_text(ctx, text_x, text_y, widget->text);
    }
}

/**
 * widget_default_on_mouse_down - Default mouse down handler
 * @widget: Widget
 * @x: X position
 * @y: Y position
 */
static void widget_default_on_mouse_down(widget_t *widget, s32 x, s32 y)
{
    (void)x;
    (void)y;
    widget->pressed = true;
    widget->window.dirty = true;
}

/**
 * widget_default_on_mouse_up - Default mouse up handler
 * @widget: Widget
 * @x: X position
 * @y: Y position
 */
static void widget_default_on_mouse_up(widget_t *widget, s32 x, s32 y)
{
    (void)x;
    (void)y;
    widget->pressed = false;
    widget->window.dirty = true;
}

/**
 * widget_default_on_mouse_enter - Default mouse enter handler
 * @widget: Widget
 */
static void widget_default_on_mouse_enter(widget_t *widget)
{
    widget->hovered = true;
    widget->window.dirty = true;
}

/**
 * widget_default_on_mouse_leave - Default mouse leave handler
 * @widget: Widget
 */
static void widget_default_on_mouse_leave(widget_t *widget)
{
    widget->hovered = false;
    widget->pressed = false;
    widget->window.dirty = true;
}

/*===========================================================================*/
/*                         WIDGET CLASS REGISTRATION                         */
/*===========================================================================*/

/**
 * widget_register_class - Register a widget class
 * @name: Class name
 * @create: Create function
 *
 * Returns: 0 on success, error code on failure
 */
int widget_register_class(const char *name, widget_t *(*create)(void))
{
    if (!name || !create || g_num_widget_classes >= 32) {
        return -1;
    }

    strncpy(g_widget_classes[g_num_widget_classes].name, name, 32);
    g_widget_classes[g_num_widget_classes].create = create;
    g_num_widget_classes++;

    return 0;
}

/*===========================================================================*/
/*                         STANDARD WIDGETS                                  */
/*===========================================================================*/

/**
 * widget_create_button - Create a button widget
 * @text: Button text
 * @x: X position
 * @y: Y position
 * @width: Width
 * @height: Height
 *
 * Returns: Pointer to created button
 */
widget_t *widget_create_button(const char *text, s32 x, s32 y, s32 width, s32 height)
{
    widget_t *button = widget_create("Button", NULL);
    if (!button) {
        return NULL;
    }

    rect_t bounds = {x, y, width, height};
    widget_init(button, "Button", NULL, text, &bounds);

    button->corner_radius = 4;
    button->border_width = 1;

    return button;
}

/**
 * widget_create_label - Create a label widget
 * @text: Label text
 * @x: X position
 * @y: Y position
 * @width: Width
 * @height: Height
 *
 * Returns: Pointer to created label
 */
widget_t *widget_create_label(const char *text, s32 x, s32 y, s32 width, s32 height)
{
    widget_t *label = widget_create("Label", NULL);
    if (!label) {
        return NULL;
    }

    rect_t bounds = {x, y, width, height};
    widget_init(label, "Label", NULL, text, &bounds);

    label->border_width = 0;

    return label;
}

/**
 * widget_create_checkbox - Create a checkbox widget
 * @text: Checkbox text
 * @x: X position
 * @y: Y position
 *
 * Returns: Pointer to created checkbox
 */
widget_t *widget_create_checkbox(const char *text, s32 x, s32 y)
{
    widget_t *checkbox = widget_create("Checkbox", NULL);
    if (!checkbox) {
        return NULL;
    }

    rect_t bounds = {x, y, 20, 20};
    widget_init(checkbox, "Checkbox", NULL, text, &bounds);

    checkbox->checked = false;

    return checkbox;
}
