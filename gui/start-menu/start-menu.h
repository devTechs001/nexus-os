/*
 * NEXUS OS - Start Menu Application
 * gui/start-menu/start-menu.h
 *
 * Modern start menu with search, pinned apps, and system actions
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _START_MENU_H
#define _START_MENU_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         START MENU CONFIGURATION                          */
/*===========================================================================*/

#define START_MENU_VERSION          "1.0.0"
#define START_MENU_WIDTH            400
#define START_MENU_HEIGHT           600
#define START_MENU_MAX_APPS         128
#define START_MENU_MAX_PINNED       32
#define START_MENU_MAX_RECENT       16
#define START_MENU_MAX_SEARCH       50

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Application Entry
 */
typedef struct {
    u32 app_id;                     /* Application ID */
    char name[128];                 /* Application Name */
    char description[256];          /* Description */
    char icon_path[512];            /* Icon Path */
    char executable[512];           /* Executable Path */
    char category[64];              /* Category */
    bool is_pinned;                 /* Is Pinned */
    bool is_system;                 /* Is System App */
    u64 last_used;                  /* Last Used Time */
    u32 launch_count;               /* Launch Count */
} app_entry_t;

/**
 * Start Menu Section
 */
typedef struct {
    char name[64];                  /* Section Name */
    u32 section_id;                 /* Section ID */
    bool is_expanded;               /* Is Expanded */
    app_entry_t *apps;              /* Apps */
    u32 app_count;                  /* App Count */
} start_menu_section_t;

/**
 * Start Menu State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *main_window;          /* Main Window */
    widget_t *main_widget;          /* Main Widget */
    
    /* Search */
    widget_t *search_box;           /* Search Box */
    char search_query[256];         /* Search Query */
    app_entry_t search_results[START_MENU_MAX_SEARCH]; /* Search Results */
    u32 search_result_count;        /* Result Count */
    
    /* Sections */
    start_menu_section_t sections[8]; /* Sections */
    u32 section_count;              /* Section Count */
    
    /* Pinned Apps */
    app_entry_t pinned_apps[START_MENU_MAX_PINNED]; /* Pinned Apps */
    u32 pinned_count;               /* Pinned Count */
    
    /* Recent Apps */
    app_entry_t recent_apps[START_MENU_MAX_RECENT]; /* Recent Apps */
    u32 recent_count;               /* Recent Count */
    
    /* User Info */
    char username[64];              /* Username */
    char user_icon[512];            /* User Icon Path */
    
    /* System Actions */
    widget_t *lock_button;          /* Lock Button */
    widget_t *logout_button;        /* Logout Button */
    widget_t *shutdown_button;      /* Shutdown Button */
    widget_t *restart_button;       /* Restart Button */
    widget_t *settings_button;      /* Settings Button */
    
    /* Callbacks */
    void (*on_app_launched)(app_entry_t *);
    void (*on_settings_clicked)(void);
    void (*on_shutdown_clicked)(void);
    
    /* User Data */
    void *user_data;
    
} start_menu_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Start menu lifecycle */
int start_menu_init(start_menu_t *menu);
int start_menu_show(start_menu_t *menu);
int start_menu_hide(start_menu_t *menu);
int start_menu_toggle(start_menu_t *menu);
int start_menu_shutdown(start_menu_t *menu);

/* App management */
int start_menu_add_app(start_menu_t *menu, app_entry_t *app);
int start_menu_remove_app(start_menu_t *menu, u32 app_id);
int start_menu_pin_app(start_menu_t *menu, u32 app_id);
int start_menu_unpin_app(start_menu_t *menu, u32 app_id);
int start_menu_launch_app(start_menu_t *menu, u32 app_id);

/* Search */
int start_menu_search(start_menu_t *menu, const char *query);
int start_menu_clear_search(start_menu_t *menu);

/* Sections */
int start_menu_add_section(start_menu_t *menu, const char *name);
int start_menu_expand_section(start_menu_t *menu, u32 section_id);
int start_menu_collapse_section(start_menu_t *menu, u32 section_id);

/* User info */
int start_menu_set_user(start_menu_t *menu, const char *username, const char *icon);

/* System actions */
int start_menu_lock(start_menu_t *menu);
int start_menu_logout(start_menu_t *menu);
int start_menu_shutdown(start_menu_t *menu);
int start_menu_restart(start_menu_t *menu);
int start_menu_open_settings(start_menu_t *menu);

/* Utility functions */
start_menu_t *start_menu_get_instance(void);

#endif /* _START_MENU_H */
