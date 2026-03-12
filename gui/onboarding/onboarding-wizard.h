/*
 * NEXUS OS - First-Time Onboarding Wizard
 * gui/onboarding/onboarding-wizard.h
 *
 * Welcome wizard for new users after first login
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _ONBOARDING_WIZARD_H
#define _ONBOARDING_WIZARD_H

#include "../gui.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         ONBOARDING CONFIGURATION                          */
/*===========================================================================*/

#define ONBOARDING_VERSION          "1.0.0"
#define ONBOARDING_MAX_PAGES        16
#define ONBOARDING_MAX_TIPS         64
#define ONBOARDING_MAX_SHORTCUTS    128

/*===========================================================================*/
/*                         ONBOARDING STATES                                 */
/*===========================================================================*/

#define ONBOARDING_STATE_IDLE       0
#define ONBOARDING_STATE_RUNNING    1
#define ONBOARDING_STATE_PAUSED     2
#define ONBOARDING_STATE_COMPLETED  3
#define ONBOARDING_STATE_SKIPPED    4

/*===========================================================================*/
/*                         ONBOARDING THEMES                                 */
/*===========================================================================*/

#define ONBOARDING_THEME_DEFAULT    0
#define ONBOARDING_THEME_DARK       1
#define ONBOARDING_THEME_LIGHT      2
#define ONBOARDING_THEME_COLORFUL   3
#define ONBOARDING_THEME_MINIMAL    4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Onboarding Page
 */
typedef struct onboarding_page {
    u32 page_id;
    char title[128];
    char subtitle[256];
    char description[1024];
    char icon[64];
    char illustration[256];
    u32 theme;
    bool is_skipable;
    bool is_required;
    bool (*on_enter)(struct onboarding_page *);
    bool (*on_leave)(struct onboarding_page *);
    bool (*on_next)(struct onboarding_page *);
    bool (*on_back)(struct onboarding_page *);
    void (*on_complete)(struct onboarding_page *);
    void *page_data;
    widget_t *page_widget;
} onboarding_page_t;

/**
 * Quick Tip
 */
typedef struct quick_tip {
    u32 tip_id;
    char category[64];
    char title[128];
    char content[512];
    char icon[64];
    u32 priority;
    bool is_dismissed;
    bool is_interactive;
    void (*action)(struct quick_tip *);
} quick_tip_t;

/**
 * Keyboard Shortcut
 */
typedef struct keyboard_shortcut {
    u32 shortcut_id;
    char category[64];
    char description[128];
    char keys[64];
    u32 modifiers;
    u32 key_code;
    void (*action)(struct keyboard_shortcut *);
    bool is_customizable;
} keyboard_shortcut_t;

/**
 * Feature Introduction
 */
typedef struct feature_intro {
    u32 feature_id;
    char name[64];
    char description[512];
    char icon[64];
    char screenshot[256];
    bool is_enabled;
    bool is_new;
    u32 category;
} feature_intro_t;

/**
 * Onboarding Wizard State
 */
typedef struct onboarding_wizard {
    bool initialized;
    bool active;
    bool visible;
    bool completed;
    bool skipped;
    u32 state;
    
    /* Window references */
    window_t *main_window;
    widget_t *main_widget;
    widget_t *sidebar;
    widget_t *content_area;
    widget_t *header_widget;
    widget_t *title_label;
    widget_t *subtitle_label;
    widget_t *description_label;
    widget_t *illustration_widget;
    widget_t *progress_indicator;
    widget_t *progress_label;
    widget_t *back_button;
    widget_t *next_button;
    widget_t *skip_button;
    widget_t *finish_button;
    widget_t *close_button;
    
    /* State */
    u32 current_page;
    u32 total_pages;
    onboarding_page_t pages[ONBOARDING_MAX_PAGES];
    
    /* Progress */
    u32 completed_pages;
    u32 skipped_pages;
    u64 start_time;
    u64 total_time;
    u64 page_times[ONBOARDING_MAX_PAGES];
    
    /* User preferences */
    u32 selected_theme;
    bool enable_animations;
    bool enable_sounds;
    bool enable_tips;
    bool show_tutorials;
    u32 difficulty_level;
    
    /* Tips and shortcuts */
    quick_tip_t tips[ONBOARDING_MAX_TIPS];
    u32 tip_count;
    u32 tips_shown;
    
    keyboard_shortcut_t shortcuts[ONBOARDING_MAX_SHORTCUTS];
    u32 shortcut_count;
    
    /* Features */
    feature_intro_t features[64];
    u32 feature_count;
    
    /* Animation */
    u32 animation_state;
    u64 animation_start;
    float animation_progress;
    
    /* Callbacks */
    void (*on_page_changed)(struct onboarding_wizard *, u32);
    void (*on_completed)(struct onboarding_wizard *);
    void (*on_skipped)(struct onboarding_wizard *);
    void (*on_cancelled)(struct onboarding_wizard *);
    
    /* User data */
    void *user_data;
    char username[64];
    u32 user_id;
    
} onboarding_wizard_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Wizard lifecycle */
int onboarding_wizard_init(onboarding_wizard_t *wizard);
int onboarding_wizard_run(onboarding_wizard_t *wizard);
int onboarding_wizard_shutdown(onboarding_wizard_t *wizard);
int onboarding_wizard_pause(onboarding_wizard_t *wizard);
int onboarding_wizard_resume(onboarding_wizard_t *wizard);
int onboarding_wizard_complete(onboarding_wizard_t *wizard);
int onboarding_wizard_skip(onboarding_wizard_t *wizard);
bool onboarding_wizard_is_active(onboarding_wizard_t *wizard);
bool onboarding_wizard_is_completed(onboarding_wizard_t *wizard);

/* Page navigation */
int onboarding_wizard_goto_page(onboarding_wizard_t *wizard, u32 page_id);
int onboarding_wizard_next_page(onboarding_wizard_t *wizard);
int onboarding_wizard_previous_page(onboarding_wizard_t *wizard);
int onboarding_wizard_add_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
onboarding_page_t *onboarding_wizard_get_current_page(onboarding_wizard_t *wizard);

/* Page content creation */
int onboarding_create_welcome_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_theme_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_desktop_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_taskbar_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_start_menu_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_windows_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_workspaces_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_files_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_apps_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_settings_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_terminal_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_shortcuts_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_gestures_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_notifications_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_search_page(onboarding_wizard_t *wizard, onboarding_page_t *page);
int onboarding_create_complete_page(onboarding_wizard_t *wizard, onboarding_page_t *page);

/* Tips and tutorials */
int onboarding_show_tip(onboarding_wizard_t *wizard, u32 tip_id);
int onboarding_dismiss_tip(onboarding_wizard_t *wizard, u32 tip_id);
int onboarding_show_all_tips(onboarding_wizard_t *wizard);
int onboarding_get_next_tip(onboarding_wizard_t *wizard, quick_tip_t *tip);

/* Shortcuts */
int onboarding_show_shortcut(onboarding_wizard_t *wizard, u32 shortcut_id);
int onboarding_list_shortcuts(onboarding_wizard_t *wizard, keyboard_shortcut_t *shortcuts, u32 *count);
int onboarding_add_shortcut(onboarding_wizard_t *wizard, keyboard_shortcut_t *shortcut);

/* Features */
int onboarding_introduce_feature(onboarding_wizard_t *wizard, u32 feature_id);
int onboarding_enable_feature(onboarding_wizard_t *wizard, u32 feature_id);
int onboarding_disable_feature(onboarding_wizard_t *wizard, u32 feature_id);

/* UI updates */
int onboarding_update_progress(onboarding_wizard_t *wizard);
int onboarding_update_buttons(onboarding_wizard_t *wizard);
int onboarding_play_animation(onboarding_wizard_t *wizard, u32 animation_id);

/* Preferences */
int onboarding_set_theme(onboarding_wizard_t *wizard, u32 theme);
int onboarding_set_animations(onboarding_wizard_t *wizard, bool enabled);
int onboarding_set_tips(onboarding_wizard_t *wizard, bool enabled);

/* Utility functions */
const char *onboarding_get_page_type_name(u32 type);
const char *onboarding_get_theme_name(u32 theme);
u32 onboarding_get_total_pages(onboarding_wizard_t *wizard);
u32 onboarding_get_progress_percent(onboarding_wizard_t *wizard);

/* Global instance */
onboarding_wizard_t *onboarding_wizard_get_instance(void);

#endif /* _ONBOARDING_WIZARD_H */
