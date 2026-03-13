/*
 * NEXUS OS - Notification Center Implementation
 * gui/notification-center/notification-center.c
 *
 * Modern notification center with grouping and history
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "notification-center/notification-center.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL NOTIFICATION CENTER                        */
/*===========================================================================*/

static notification_center_t g_notification_center;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int create_notification_center_ui(notification_center_t *center);
static int create_notification_list(notification_center_t *center);
static int create_notification_item(notification_center_t *center, notification_t *notify);
static int create_groups_view(notification_center_t *center);
static int create_history_view(notification_center_t *center);

static void on_notification_clicked(widget_t *widget);
static void on_dismiss_clicked(widget_t *widget);
static void on_action_clicked(widget_t *widget);
static void on_clear_all_clicked(widget_t *widget);

static notification_t *create_notification_internal(const char *app_name,
                                                     const char *title,
                                                     const char *message,
                                                     const char *icon,
                                                     u32 urgency,
                                                     u32 timeout);

/*===========================================================================*/
/*                         NOTIFICATION CENTER LIFECYCLE                     */
/*===========================================================================*/

/**
 * notification_center_init - Initialize notification center
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int notification_center_init(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    printk("[NOTIFICATION] ========================================\n");
    printk("[NOTIFICATION] NEXUS OS Notification Center v%s\n", NOTIFICATION_CENTER_VERSION);
    printk("[NOTIFICATION] ========================================\n");
    printk("[NOTIFICATION] Initializing notification center...\n");
    
    /* Clear structure */
    memset(center, 0, sizeof(notification_center_t));
    center->initialized = true;
    center->visible = false;
    
    /* Default settings */
    center->dnd_enabled = false;
    center->show_preview = true;
    center->group_notifications = true;
    center->max_visible = 8;
    center->timeout_default = 5000;
    
    /* Create UI */
    if (create_notification_center_ui(center) != 0) {
        printk("[NOTIFICATION] Failed to create UI\n");
        return -1;
    }
    
    printk("[NOTIFICATION] Notification center initialized\n");
    printk("[NOTIFICATION] ========================================\n");
    
    return 0;
}

/**
 * notification_center_show - Show notification center
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_show(notification_center_t *center)
{
    if (!center || !center->initialized) {
        return -EINVAL;
    }
    
    if (center->center_window) {
        window_show(center->center_window);
        center->visible = true;
    }
    
    return 0;
}

/**
 * notification_center_hide - Hide notification center
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_hide(notification_center_t *center)
{
    if (!center || !center->initialized) {
        return -EINVAL;
    }
    
    if (center->center_window) {
        window_hide(center->center_window);
        center->visible = false;
    }
    
    return 0;
}

/**
 * notification_center_toggle - Toggle notification center
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_toggle(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    if (center->visible) {
        return notification_center_hide(center);
    } else {
        return notification_center_show(center);
    }
}

/**
 * notification_center_shutdown - Shutdown notification center
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_shutdown(notification_center_t *center)
{
    if (!center || !center->initialized) {
        return -EINVAL;
    }
    
    /* Free all notifications */
    notification_t *notify = center->notifications;
    while (notify) {
        notification_t *next = notify->next;
        kfree(notify);
        notify = next;
    }
    
    /* Free history */
    notify = center->history;
    while (notify) {
        notification_t *next = notify->next;
        kfree(notify);
        notify = next;
    }
    
    if (center->center_window) {
        window_hide(center->center_window);
    }
    
    center->initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_notification_center_ui - Create notification center UI
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_notification_center_ui(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    /* Create notification center window */
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "Notification Center");
    props.type = WINDOW_TYPE_POPUP;
    props.flags = WINDOW_FLAG_BORDERLESS;
    props.bounds.x = 1920 - NOTIFICATION_WIDTH - 10;
    props.bounds.y = 42;
    props.bounds.width = NOTIFICATION_WIDTH;
    props.bounds.height = 600;
    props.background = color_from_rgba(30, 30, 45, 245);
    
    center->center_window = window_create(&props);
    if (!center->center_window) {
        return -1;
    }
    
    /* Create main widget */
    center->center_widget = panel_create(NULL, 0, 0, NOTIFICATION_WIDTH, 600);
    if (!center->center_widget) {
        return -1;
    }
    
    widget_set_colors(center->center_widget,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(30, 30, 45, 245),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create header */
    widget_t *header = label_create(center->center_widget, "Notifications", 15, 10, 200, 30);
    if (header) {
        widget_set_font(header, 0, 16);
        widget_set_colors(header,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(0, 0, 0, 0),
                          color_from_rgba(0, 0, 0, 0));
    }
    
    /* Create clear all button */
    widget_t *clear_all = button_create(center->center_widget, "Clear All", 250, 10, 80, 28);
    if (clear_all) {
        clear_all->on_click = on_clear_all_clicked;
        clear_all->user_data = center;
        widget_set_colors(clear_all,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(60, 60, 80, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    /* Create notification list */
    create_notification_list(center);
    
    return 0;
}

/**
 * create_notification_list - Create notification list
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
static int create_notification_list(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    /* Create scrollable notification list */
    center->center_widget = panel_create(center->center_widget, 10, 50, 330, 540);
    if (!center->center_widget) {
        return -1;
    }
    
    widget_set_colors(center->center_widget,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_notification_item - Create notification item widget
 * @center: Pointer to notification center structure
 * @notify: Notification to create widget for
 *
 * Returns: 0 on success
 */
static int create_notification_item(notification_center_t *center, notification_t *notify)
{
    if (!center || !notify) {
        return -EINVAL;
    }
    
    /* In real implementation, would create notification widget with:
     * - App icon
     * - Title
     * - Message
     * - Timestamp
     * - Dismiss button
     * - Action buttons
     */
    
    return 0;
}

/**
 * create_groups_view - Create grouped notifications view
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
static int create_groups_view(notification_center_t *center)
{
    (void)center;
    return 0;
}

/**
 * create_history_view - Create notification history view
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
static int create_history_view(notification_center_t *center)
{
    (void)center;
    return 0;
}

/*===========================================================================*/
/*                         NOTIFICATION MANAGEMENT                           */
/*===========================================================================*/

/**
 * notification_center_add - Add notification
 * @center: Pointer to notification center structure
 * @app_name: Application name
 * @title: Notification title
 * @message: Notification message
 * @icon: Icon path
 * @urgency: Urgency level
 * @timeout: Timeout in ms
 *
 * Returns: Pointer to created notification, or NULL on failure
 */
notification_t *notification_center_add(notification_center_t *center,
                                         const char *app_name,
                                         const char *title,
                                         const char *message,
                                         const char *icon,
                                         u32 urgency,
                                         u32 timeout)
{
    if (!center || !app_name || !title || !message) {
        return NULL;
    }
    
    /* Check Do Not Disturb */
    if (center->dnd_enabled && urgency != NOTIFICATION_URGENCY_CRITICAL) {
        printk("[NOTIFICATION] DND enabled, skipping notification\n");
        return NULL;
    }
    
    /* Create notification */
    notification_t *notify = create_notification_internal(app_name, title, message, icon, urgency, timeout);
    if (!notify) {
        return NULL;
    }
    
    /* Add to list */
    notify->next = center->notifications;
    center->notifications = notify;
    center->notification_count++;
    
    if (!notify->is_read) {
        center->unread_count++;
    }
    
    /* Create widget */
    create_notification_item(center, notify);
    
    /* Callback */
    if (center->on_notification_added) {
        center->on_notification_added(notify);
    }
    
    printk("[NOTIFICATION] Added: %s - %s\n", app_name, title);
    
    return notify;
}

/**
 * create_notification_internal - Create notification internally
 */
static notification_t *create_notification_internal(const char *app_name,
                                                     const char *title,
                                                     const char *message,
                                                     const char *icon,
                                                     u32 urgency,
                                                     u32 timeout)
{
    notification_t *notify = kzalloc(sizeof(notification_t));
    if (!notify) {
        return NULL;
    }
    
    notify->notify_id = (u32)get_time_ms();
    strncpy(notify->app_name, app_name, sizeof(notify->app_name) - 1);
    strncpy(notify->title, title, sizeof(notify->title) - 1);
    strncpy(notify->message, message, sizeof(notify->message) - 1);
    
    if (icon) {
        strncpy(notify->icon_path, icon, sizeof(notify->icon_path) - 1);
    }
    
    notify->urgency = urgency;
    notify->timeout = timeout;
    notify->timestamp = get_time_ms();
    notify->is_read = false;
    notify->is_dismissed = false;
    
    return notify;
}

/**
 * notification_center_dismiss - Dismiss notification
 * @center: Pointer to notification center structure
 * @notify_id: Notification ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int notification_center_dismiss(notification_center_t *center, u32 notify_id)
{
    if (!center) {
        return -EINVAL;
    }
    
    notification_t **prev = &center->notifications;
    notification_t *curr = center->notifications;
    
    while (curr) {
        if (curr->notify_id == notify_id) {
            *prev = curr->next;
            
            /* Add to history */
            curr->is_dismissed = true;
            curr->next = center->history;
            center->history = curr;
            center->history_count++;
            
            if (!curr->is_read) {
                center->unread_count--;
            }
            
            center->notification_count--;
            
            if (center->on_notification_dismissed) {
                center->on_notification_dismissed(curr);
            }
            
            return 0;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    return -ENOENT;
}

/**
 * notification_center_dismiss_all - Dismiss all notifications
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_dismiss_all(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    while (center->notifications) {
        notification_center_dismiss(center, center->notifications->notify_id);
    }
    
    return 0;
}

/**
 * notification_center_dismiss_app - Dismiss notifications from app
 * @center: Pointer to notification center structure
 * @app_name: Application name
 *
 * Returns: Number of dismissed notifications
 */
int notification_center_dismiss_app(notification_center_t *center, const char *app_name)
{
    if (!center || !app_name) {
        return -EINVAL;
    }
    
    int count = 0;
    notification_t *curr = center->notifications;
    
    while (curr) {
        if (strcmp(curr->app_name, app_name) == 0) {
            u32 id = curr->notify_id;
            notification_center_dismiss(center, id);
            count++;
        }
        curr = curr->next;
    }
    
    return count;
}

/**
 * notification_center_mark_read - Mark notification as read
 * @center: Pointer to notification center structure
 * @notify_id: Notification ID
 *
 * Returns: 0 on success
 */
int notification_center_mark_read(notification_center_t *center, u32 notify_id)
{
    if (!center) {
        return -EINVAL;
    }
    
    notification_t *notify = notification_center_get(center, notify_id);
    if (notify && !notify->is_read) {
        notify->is_read = true;
        center->unread_count--;
    }
    
    return 0;
}

/**
 * notification_center_mark_all_read - Mark all notifications as read
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_mark_all_read(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    notification_t *notify = center->notifications;
    while (notify) {
        if (!notify->is_read) {
            notify->is_read = true;
            center->unread_count--;
        }
        notify = notify->next;
    }
    
    return 0;
}

/**
 * notification_center_get - Get notification by ID
 * @center: Pointer to notification center structure
 * @notify_id: Notification ID
 *
 * Returns: Notification pointer, or NULL if not found
 */
notification_t *notification_center_get(notification_center_t *center, u32 notify_id)
{
    if (!center) {
        return NULL;
    }
    
    notification_t *notify = center->notifications;
    while (notify) {
        if (notify->notify_id == notify_id) {
            return notify;
        }
        notify = notify->next;
    }
    
    return NULL;
}

/*===========================================================================*/
/*                         NOTIFICATION ACTIONS                              */
/*===========================================================================*/

/**
 * notification_center_add_action - Add action to notification
 * @notify: Notification
 * @label: Action label
 * @callback: Callback function
 * @user_data: User data
 *
 * Returns: 0 on success
 */
int notification_center_add_action(notification_t *notify, const char *label,
                                    void (*callback)(void *), void *user_data)
{
    if (!notify || !label) {
        return -EINVAL;
    }
    
    if (notify->action_count >= NOTIFICATION_MAX_ACTIONS) {
        return -ENOSPC;
    }
    
    notification_action_t *action = &notify->actions[notify->action_count];
    strncpy(action->label, label, sizeof(action->label) - 1);
    action->callback = callback;
    action->user_data = user_data;
    
    notify->action_count++;
    notify->has_actions = true;
    
    return 0;
}

/**
 * notification_center_trigger_action - Trigger notification action
 * @center: Pointer to notification center structure
 * @notify_id: Notification ID
 * @action_index: Action index
 *
 * Returns: 0 on success
 */
int notification_center_trigger_action(notification_center_t *center,
                                        u32 notify_id, u32 action_index)
{
    if (!center) {
        return -EINVAL;
    }
    
    notification_t *notify = notification_center_get(center, notify_id);
    if (!notify || action_index >= notify->action_count) {
        return -EINVAL;
    }
    
    notification_action_t *action = &notify->actions[action_index];
    
    if (action->callback) {
        action->callback(action->user_data);
    }
    
    if (center->on_action_triggered) {
        center->on_action_triggered(notify, action_index);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         SETTINGS                                          */
/*===========================================================================*/

/**
 * notification_center_set_dnd - Set Do Not Disturb
 * @center: Pointer to notification center structure
 * @enabled: Enabled
 *
 * Returns: 0 on success
 */
int notification_center_set_dnd(notification_center_t *center, bool enabled)
{
    if (!center) {
        return -EINVAL;
    }
    
    center->dnd_enabled = enabled;
    
    printk("[NOTIFICATION] Do Not Disturb %s\n", enabled ? "enabled" : "disabled");
    
    return 0;
}

/**
 * notification_center_set_preview - Set preview setting
 * @center: Pointer to notification center structure
 * @enabled: Enabled
 *
 * Returns: 0 on success
 */
int notification_center_set_preview(notification_center_t *center, bool enabled)
{
    if (!center) {
        return -EINVAL;
    }
    
    center->show_preview = enabled;
    
    return 0;
}

/**
 * notification_center_set_grouping - Set notification grouping
 * @center: Pointer to notification center structure
 * @enabled: Enabled
 *
 * Returns: 0 on success
 */
int notification_center_set_grouping(notification_center_t *center, bool enabled)
{
    if (!center) {
        return -EINVAL;
    }
    
    center->group_notifications = enabled;
    
    return 0;
}

/**
 * notification_center_set_max_visible - Set max visible notifications
 * @center: Pointer to notification center structure
 * @max: Max visible
 *
 * Returns: 0 on success
 */
int notification_center_set_max_visible(notification_center_t *center, u32 max)
{
    if (!center) {
        return -EINVAL;
    }
    
    center->max_visible = max;
    
    return 0;
}

/**
 * notification_center_set_timeout - Set default timeout
 * @center: Pointer to notification center structure
 * @timeout: Timeout in ms
 *
 * Returns: 0 on success
 */
int notification_center_set_timeout(notification_center_t *center, u32 timeout)
{
    if (!center) {
        return -EINVAL;
    }
    
    center->timeout_default = timeout;
    
    return 0;
}

/*===========================================================================*/
/*                         HISTORY                                           */
/*===========================================================================*/

/**
 * notification_center_show_history - Show notification history
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_show_history(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    /* In real implementation, would show history view */
    
    return 0;
}

/**
 * notification_center_clear_history - Clear notification history
 * @center: Pointer to notification center structure
 *
 * Returns: 0 on success
 */
int notification_center_clear_history(notification_center_t *center)
{
    if (!center) {
        return -EINVAL;
    }
    
    /* Free history */
    notification_t *notify = center->history;
    while (notify) {
        notification_t *next = notify->next;
        kfree(notify);
        notify = next;
    }
    
    center->history = NULL;
    center->history_count = 0;
    
    return 0;
}

/**
 * notification_center_get_history - Get notification history
 * @center: Pointer to notification center structure
 * @notifications: Notifications array
 * @count: Count pointer
 *
 * Returns: 0 on success
 */
int notification_center_get_history(notification_center_t *center,
                                     notification_t *notifications,
                                     u32 *count)
{
    if (!center || !notifications || !count) {
        return -EINVAL;
    }
    
    u32 copy_count = (*count < center->history_count) ? *count : center->history_count;
    
    notification_t *notify = center->history;
    for (u32 i = 0; i < copy_count && notify; i++) {
        memcpy(&notifications[i], notify, sizeof(notification_t));
        notify = notify->next;
    }
    
    *count = copy_count;
    
    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

static void on_notification_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    notification_t *notify = (notification_t *)widget->user_data;
    notification_center_t *center = &g_notification_center;
    
    printk("[NOTIFICATION] Notification clicked: %s\n", notify->title);
    
    notification_center_mark_read(center, notify->notify_id);
    
    if (center->on_notification_clicked) {
        center->on_notification_clicked(notify);
    }
}

static void on_dismiss_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    notification_t *notify = (notification_t *)widget->user_data;
    notification_center_t *center = &g_notification_center;
    
    notification_center_dismiss(center, notify->notify_id);
}

static void on_action_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    /* In real implementation, would trigger action */
}

static void on_clear_all_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    notification_center_t *center = (notification_center_t *)widget->user_data;
    notification_center_dismiss_all(center);
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * notification_get_urgency_name - Get urgency level name
 * @urgency: Urgency level
 *
 * Returns: Urgency name string
 */
const char *notification_get_urgency_name(u32 urgency)
{
    switch (urgency) {
        case NOTIFICATION_URGENCY_LOW:      return "Low";
        case NOTIFICATION_URGENCY_NORMAL:   return "Normal";
        case NOTIFICATION_URGENCY_HIGH:     return "High";
        case NOTIFICATION_URGENCY_CRITICAL: return "Critical";
        default:                            return "Unknown";
    }
}

/**
 * notification_center_get_instance - Get global notification center instance
 *
 * Returns: Pointer to global instance
 */
notification_center_t *notification_center_get_instance(void)
{
    return &g_notification_center;
}
