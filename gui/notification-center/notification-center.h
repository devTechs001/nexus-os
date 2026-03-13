/*
 * NEXUS OS - Notification Center
 * gui/notification-center/notification-center.h
 *
 * Modern notification center with grouping and history
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _NOTIFICATION_CENTER_H
#define _NOTIFICATION_CENTER_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         NOTIFICATION CONFIGURATION                        */
/*===========================================================================*/

#define NOTIFICATION_CENTER_VERSION "1.0.0"
#define NOTIFICATION_MAX_COUNT      64
#define NOTIFICATION_MAX_HISTORY    256
#define NOTIFICATION_MAX_ACTIONS    4
#define NOTIFICATION_WIDTH          350
#define NOTIFICATION_HEIGHT_AUTO    0

/*===========================================================================*/
/*                         NOTIFICATION URGENCY                              */
/*===========================================================================*/

#define NOTIFICATION_URGENCY_LOW     0
#define NOTIFICATION_URGENCY_NORMAL  1
#define NOTIFICATION_URGENCY_HIGH    2
#define NOTIFICATION_URGENCY_CRITICAL 3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Notification Action
 */
typedef struct {
    char label[64];                 /* Action Label */
    void (*callback)(void *);       /* Callback */
    void *user_data;                /* User Data */
} notification_action_t;

/**
 * Notification
 */
typedef struct notification {
    u32 notify_id;                  /* Notification ID */
    char app_name[128];             /* Application Name */
    char title[256];                /* Notification Title */
    char message[1024];             /* Notification Message */
    char icon_path[512];            /* Icon Path */
    u32 urgency;                    /* Urgency Level */
    u64 timestamp;                  /* Timestamp */
    u32 timeout;                    /* Timeout (ms) */
    bool has_actions;               /* Has Actions */
    notification_action_t actions[NOTIFICATION_MAX_ACTIONS]; /* Actions */
    u32 action_count;               /* Action Count */
    bool is_read;                   /* Is Read */
    bool is_dismissed;              /* Is Dismissed */
    char app_icon[512];             /* App Icon Path */
    void *data;                     /* Notification Data */
    struct notification *next;      /* Next Notification */
} notification_t;

/**
 * Notification Group
 */
typedef struct {
    char app_name[128];             /* Application Name */
    char app_icon[512];             /* App Icon */
    notification_t *notifications;  /* Notifications */
    u32 count;                      /* Notification Count */
    u32 unread_count;               /* Unread Count */
    bool is_expanded;               /* Is Expanded */
} notification_group_t;

/**
 * Notification Center State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *center_window;        /* Center Window */
    widget_t *center_widget;        /* Center Widget */
    
    /* Notifications */
    notification_t *notifications;  /* All Notifications */
    u32 notification_count;         /* Total Count */
    u32 unread_count;               /* Unread Count */
    
    /* Groups */
    notification_group_t groups[32]; /* Notification Groups */
    u32 group_count;                /* Group Count */
    
    /* History */
    notification_t *history;        /* Notification History */
    u32 history_count;              /* History Count */
    
    /* Settings */
    bool dnd_enabled;               /* Do Not Disturb */
    bool show_preview;              /* Show Preview */
    bool group_notifications;       /* Group Notifications */
    u32 max_visible;                /* Max Visible Notifications */
    u32 timeout_default;            /* Default Timeout */
    
    /* Callbacks */
    void (*on_notification_added)(notification_t *);
    void (*on_notification_dismissed)(notification_t *);
    void (*on_notification_clicked)(notification_t *);
    void (*on_action_triggered)(notification_t *, u32);
    
    /* User Data */
    void *user_data;
    
} notification_center_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Notification center lifecycle */
int notification_center_init(notification_center_t *center);
int notification_center_show(notification_center_t *center);
int notification_center_hide(notification_center_t *center);
int notification_center_toggle(notification_center_t *center);
int notification_center_shutdown(notification_center_t *center);

/* Notification management */
notification_t *notification_center_add(notification_center_t *center, 
                                         const char *app_name,
                                         const char *title,
                                         const char *message,
                                         const char *icon,
                                         u32 urgency,
                                         u32 timeout);
int notification_center_dismiss(notification_center_t *center, u32 notify_id);
int notification_center_dismiss_all(notification_center_t *center);
int notification_center_dismiss_app(notification_center_t *center, const char *app_name);
int notification_center_mark_read(notification_center_t *center, u32 notify_id);
int notification_center_mark_all_read(notification_center_t *center);
notification_t *notification_center_get(notification_center_t *center, u32 notify_id);

/* Notification actions */
int notification_center_add_action(notification_t *notify, const char *label, 
                                    void (*callback)(void *), void *user_data);
int notification_center_trigger_action(notification_center_t *center, 
                                        u32 notify_id, u32 action_index);

/* Settings */
int notification_center_set_dnd(notification_center_t *center, bool enabled);
int notification_center_set_preview(notification_center_t *center, bool enabled);
int notification_center_set_grouping(notification_center_t *center, bool enabled);
int notification_center_set_max_visible(notification_center_t *center, u32 max);
int notification_center_set_timeout(notification_center_t *center, u32 timeout);

/* History */
int notification_center_show_history(notification_center_t *center);
int notification_center_clear_history(notification_center_t *center);
int notification_center_get_history(notification_center_t *center, 
                                     notification_t *notifications, 
                                     u32 *count);

/* Utility functions */
const char *notification_get_urgency_name(u32 urgency);
notification_center_t *notification_center_get_instance(void);

#endif /* _NOTIFICATION_CENTER_H */
