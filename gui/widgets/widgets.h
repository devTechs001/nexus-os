/*
 * NEXUS OS - GUI Widgets Header
 * gui/widgets/widgets.h
 *
 * Widget definitions for GUI controls
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _WIDGETS_H
#define _WIDGETS_H

#include "../gui.h"

/*===========================================================================*/
/*                         WIDGET TYPES                                      */
/*===========================================================================*/

#define WIDGET_TYPE_BASE        "Widget"
#define WIDGET_TYPE_BUTTON      "Button"
#define WIDGET_TYPE_LABEL       "Label"
#define WIDGET_TYPE_EDIT        "Edit"
#define WIDGET_TYPE_COMBOBOX    "ComboBox"
#define WIDGET_TYPE_LISTBOX     "ListBox"
#define WIDGET_TYPE_CHECKBOX    "CheckBox"
#define WIDGET_TYPE_RADIOBUTTON "RadioButton"
#define WIDGET_TYPE_PROGRESSBAR "ProgressBar"
#define WIDGET_TYPE_SLIDER      "Slider"
#define WIDGET_TYPE_TABCONTROL  "TabControl"
#define WIDGET_TYPE_TREEVIEW    "TreeView"
#define WIDGET_TYPE_LISTVIEW    "ListView"
#define WIDGET_TYPE_IMAGE       "Image"
#define WIDGET_TYPE_PANEL       "Panel"
#define WIDGET_TYPE_GROUPBOX    "GroupBox"
#define WIDGET_TYPE_MENU        "Menu"
#define WIDGET_TYPE_TOOLBAR     "ToolBar"
#define WIDGET_TYPE_STATUSBAR   "StatusBar"

/*===========================================================================*/
/*                         WIDGET STRUCTURE                                  */
/*===========================================================================*/

typedef struct widget {
    /* Inherit from window */
    window_t window;
    
    /* Widget properties */
    char widget_type[32];
    char name[64];
    char text[256];
    bool enabled;
    bool visible;
    bool focused;
    bool hovered;
    bool pressed;
    bool checked;
    
    /* Appearance */
    color_t foreground;
    color_t background;
    color_t border_color;
    u32 border_width;
    u32 corner_radius;
    u32 padding;
    u32 margin;
    u32 font_size;
    
    /* Layout */
    u32 horizontal_align;
    u32 vertical_align;
    
    /* Children */
    struct widget *children;
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
    u32 tag;
    
    /* Reference counting */
    atomic_t refcount;
} widget_t;

/*===========================================================================*/
/*                         WIDGET FUNCTIONS                                  */
/*===========================================================================*/

/* Base widget operations */
widget_t *widget_create(const char *type, widget_t *parent);
void widget_destroy(widget_t *widget);
int widget_set_text(widget_t *widget, const char *text);
const char *widget_get_text(widget_t *widget);
int widget_set_position(widget_t *widget, s32 x, s32 y);
int widget_set_size(widget_t *widget, s32 width, s32 height);
int widget_set_rect(widget_t *widget, s32 x, s32 y, s32 width, s32 height);
int widget_get_rect(widget_t *widget, rect_t *rect);
int widget_show(widget_t *widget);
int widget_hide(widget_t *widget);
int widget_enable(widget_t *widget);
int widget_disable(widget_t *widget);
int widget_focus(widget_t *widget);
int widget_set_font(widget_t *widget, u32 font_id, u32 font_size);
int widget_set_colors(widget_t *widget, color_t fg, color_t bg, color_t border);

/* Widget types */
widget_t *button_create(widget_t *parent, const char *text, s32 x, s32 y, s32 w, s32 h);
widget_t *label_create(widget_t *parent, const char *text, s32 x, s32 y, s32 w, s32 h);
widget_t *edit_create(widget_t *parent, const char *text, s32 x, s32 y, s32 w, s32 h);
widget_t *checkbox_create(widget_t *parent, const char *text, s32 x, s32 y, s32 w, s32 h);
widget_t *radiobutton_create(widget_t *parent, const char *text, s32 x, s32 y, s32 w, s32 h);
widget_t *progressbar_create(widget_t *parent, s32 x, s32 y, s32 w, s32 h);
widget_t *combobox_create(widget_t *parent, s32 x, s32 y, s32 w, s32 h);
widget_t *listbox_create(widget_t *parent, s32 x, s32 y, s32 w, s32 h);
widget_t *panel_create(widget_t *parent, s32 x, s32 y, s32 w, s32 h);
widget_t *groupbox_create(widget_t *parent, const char *title, s32 x, s32 y, s32 w, s32 h);
widget_t *image_create(widget_t *parent, void *bitmap, s32 x, s32 y, s32 w, s32 h);
widget_t *tabcontrol_create(widget_t *parent, s32 x, s32 y, s32 w, s32 h);

/* Widget utilities */
widget_t *widget_find_by_name(widget_t *parent, const char *name);
widget_t *widget_find_at(widget_t *parent, s32 x, s32 y);
int widget_add_child(widget_t *parent, widget_t *child);
int widget_remove_child(widget_t *parent, widget_t *child);
void widget_update(widget_t *widget);
void widget_invalidate(widget_t *widget);

#endif /* _WIDGETS_H */
