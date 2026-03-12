/*
 * NEXUS OS - Enhanced Login Screen
 * gui/login/login-screen.h
 *
 * Modern, secure login screen with multiple authentication methods
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _LOGIN_SCREEN_H
#define _LOGIN_SCREEN_H

#include "../gui.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"
#include "../../security/auth/auth.h"

/*===========================================================================*/
/*                         LOGIN SCREEN CONFIGURATION                        */
/*===========================================================================*/

#define LOGIN_SCREEN_VERSION        "1.0.0"
#define LOGIN_MAX_USERS             32
#define LOGIN_MAX_SESSIONS          16
#define LOGIN_MAX_RECENT_USERS      8
#define LOGIN_USERNAME_MAX          64
#define LOGIN_PASSWORD_MAX          128
#define LOGIN_HOSTNAME_MAX          64
#define LOGIN_MESSAGE_MAX           512

/*===========================================================================*/
/*                         AUTHENTICATION METHODS                            */
/*===========================================================================*/

#define AUTH_METHOD_PASSWORD        0
#define AUTH_METHOD_PIN             1
#define AUTH_METHOD_FINGERPRINT     2
#define AUTH_METHOD_FACE            3
#define AUTH_METHOD_SMARTCARD       4
#define AUTH_METHOD_TOKEN           5
#define AUTH_METHOD_PATTERN         6

/*===========================================================================*/
/*                         SESSION TYPES                                     */
/*===========================================================================*/

#define SESSION_TYPE_DESKTOP        0
#define SESSION_TYPE_WAYLAND        1
#define SESSION_TYPE_X11            2
#define SESSION_TYPE_TTY            3
#define SESSION_TYPE_REMOTE         4
#define SESSION_TYPE_CUSTOM         5

/*===========================================================================*/
/*                         LOGIN STATES                                      */
/*===========================================================================*/

#define LOGIN_STATE_LOCKED          0
#define LOGIN_STATE_UNLOCKED        1
#define LOGIN_STATE_AUTHENTICATING  2
#define LOGIN_STATE_LOGGING_IN      3
#define LOGIN_STATE_LOGGING_OUT     4
#define LOGIN_STATE_SHUTDOWN        5
#define LOGIN_STATE_RESTART         6
#define LOGIN_STATE_SLEEP           7
#define LOGIN_STATE_HIBERNATE       8

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * User Information
 */
typedef struct user_info {
    u32 user_id;
    u32 group_id;
    char username[LOGIN_USERNAME_MAX];
    char full_name[128];
    char email[128];
    char icon_path[256];
    char home_dir[256];
    char shell[64];
    bool is_admin;
    bool is_locked;
    bool is_guest;
    bool auto_login;
    bool require_password;
    u64 last_login;
    u32 login_count;
    char language[32];
    char timezone[64];
    char theme[64];
    void *user_data;
} user_info_t;

/**
 * Session Information
 */
typedef struct session_info {
    u32 session_id;
    u32 user_id;
    u32 session_type;
    char session_name[64];
    char session_command[512];
    char desktop_environment[64];
    bool is_active;
    bool is_default;
    u64 start_time;
    u64 last_activity;
    char display[32];
    void *session_data;
} session_info_t;

/**
 * Login Screen Theme
 */
typedef struct login_theme {
    char name[64];
    char background_image[512];
    char background_color[32];
    char blur_effect;
    u32 blur_radius;
    
    /* Colors */
    color_t background;
    color_t overlay;
    color_t panel_bg;
    color_t panel_fg;
    color_t text_primary;
    color_t text_secondary;
    color_t text_hint;
    color_t accent;
    color_t error;
    color_t success;
    color_t warning;
    
    /* Fonts */
    u32 font_family;
    u32 font_size_title;
    u32 font_size_subtitle;
    u32 font_size_body;
    u32 font_size_small;
    
    /* Layout */
    u32 user_icon_size;
    u32 panel_width;
    u32 panel_height;
    u32 panel_corner_radius;
    u32 animation_duration;
    
    /* Effects */
    bool show_clock;
    bool show_date;
    bool show_hostname;
    bool show_session_selector;
    bool show_power_menu;
    bool show_accessibility;
} login_theme_t;

/**
 * Power Menu Item
 */
typedef struct power_menu_item {
    u32 id;
    char label[64];
    char icon[64];
    char tooltip[128];
    void (*action)(struct power_menu_item *);
    bool enabled;
    bool visible;
} power_menu_item_t;

/**
 * Accessibility Options
 */
typedef struct accessibility_options {
    bool high_contrast;
    bool large_text;
    bool screen_reader;
    bool on_screen_keyboard;
    bool sticky_keys;
    bool slow_keys;
    bool mouse_keys;
    u32 cursor_size;
    u32 text_scale;
    u32 repeat_delay;
    u32 repeat_rate;
} accessibility_options_t;

/**
 * Login Screen State
 */
typedef struct login_screen {
    bool initialized;
    bool active;
    bool visible;
    u32 state;
    
    /* Window references */
    window_t *main_window;
    widget_t *main_widget;
    widget_t *background_widget;
    widget_t *clock_widget;
    widget_t *date_widget;
    widget_t *hostname_widget;
    widget_t *user_panel;
    widget_t *user_list;
    widget_t *user_icon;
    widget_t *username_label;
    widget_t *password_entry;
    widget_t *password_hint;
    widget_t *login_button;
    widget_t *cancel_button;
    widget_t *session_selector;
    widget_t *power_menu;
    widget_t *accessibility_button;
    widget_t *status_label;
    widget_t *message_label;
    widget_t *progress_bar;
    widget_t *keyboard_button;
    
    /* User management */
    user_info_t users[LOGIN_MAX_USERS];
    u32 user_count;
    u32 selected_user;
    u32 recent_users[LOGIN_MAX_RECENT_USERS];
    u32 recent_count;
    user_info_t *current_user;
    
    /* Session management */
    session_info_t sessions[LOGIN_MAX_SESSIONS];
    u32 session_count;
    u32 selected_session;
    session_info_t *current_session;
    
    /* Authentication */
    u32 auth_method;
    char password_buffer[LOGIN_PASSWORD_MAX];
    u32 password_length;
    bool password_visible;
    u32 failed_attempts;
    u32 max_failed_attempts;
    u64 lockout_time;
    bool is_authenticated;
    auth_context_t *auth_context;
    
    /* Theme */
    login_theme_t theme;
    char current_theme_name[64];
    
    /* State */
    char hostname[LOGIN_HOSTNAME_MAX];
    char welcome_message[LOGIN_MESSAGE_MAX];
    char error_message[LOGIN_MESSAGE_MAX];
    char status_message[LOGIN_MESSAGE_MAX];
    
    /* Clock */
    u32 hour;
    u32 minute;
    u32 second;
    u32 day;
    u32 month;
    u32 year;
    u32 weekday;
    bool is_24hour;
    bool show_seconds;
    
    /* Accessibility */
    accessibility_options_t accessibility;
    bool osk_visible;
    widget_t *osk_widget;
    
    /* Animation */
    u32 animation_state;
    u64 animation_start;
    u64 animation_duration;
    float animation_progress;
    
    /* Input */
    bool keyboard_active;
    u32 last_key_time;
    u64 idle_time;
    u64 idle_timeout;
    
    /* Security */
    bool secure_mode;
    bool encrypt_password;
    bool show_failed_message;
    bool log_attempts;
    
    /* Callbacks */
    void (*on_login_success)(struct login_screen *, user_info_t *);
    void (*on_login_failure)(struct login_screen *, s32);
    void (*on_user_selected)(struct login_screen *, user_info_t *);
    void (*on_session_changed)(struct login_screen *, session_info_t *);
    void (*on_power_action)(struct login_screen *, u32);
    void (*on_state_changed)(struct login_screen *, u32);
    
    /* User data */
    void *user_data;
    
} login_screen_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Login screen lifecycle */
int login_screen_init(login_screen_t *screen);
int login_screen_run(login_screen_t *screen);
int login_screen_shutdown(login_screen_t *screen);
int login_screen_lock(login_screen_t *screen);
int login_screen_unlock(login_screen_t *screen);
bool login_screen_is_active(login_screen_t *screen);

/* User management */
int login_screen_load_users(login_screen_t *screen);
int login_screen_add_user(login_screen_t *screen, user_info_t *user);
int login_screen_remove_user(login_screen_t *screen, u32 user_id);
int login_screen_select_user(login_screen_t *screen, u32 user_index);
int login_screen_get_current_user(login_screen_t *screen, user_info_t *user);
int login_screen_list_users(login_screen_t *screen, user_info_t *users, u32 *count);

/* Session management */
int login_screen_load_sessions(login_screen_t *screen);
int login_screen_add_session(login_screen_t *screen, session_info_t *session);
int login_screen_select_session(login_screen_t *screen, u32 session_index);
int login_screen_get_current_session(login_screen_t *screen, session_info_t *session);
int login_screen_list_sessions(login_screen_t *screen, session_info_t *sessions, u32 *count);

/* Authentication */
int login_screen_authenticate(login_screen_t *screen, const char *password);
int login_screen_authenticate_pin(login_screen_t *screen, const char *pin);
int login_screen_authenticate_fingerprint(login_screen_t *screen);
int login_screen_authenticate_face(login_screen_t *screen);
int login_screen_logout(login_screen_t *screen);
int login_screen_switch_user(login_screen_t *screen, u32 user_id);
bool login_screen_is_authenticated(login_screen_t *screen);

/* UI updates */
int login_screen_update_clock(login_screen_t *screen);
int login_screen_update_status(login_screen_t *screen, const char *status);
int login_screen_update_message(login_screen_t *screen, const char *message);
int login_screen_show_error(login_screen_t *screen, const char *error);
int login_screen_clear_error(login_screen_t *screen);
int login_screen_set_theme(login_screen_t *screen, const char *theme_name);
int login_screen_apply_theme(login_screen_t *screen, login_theme_t *theme);

/* Power management */
int login_screen_shutdown_system(login_screen_t *screen);
int login_screen_restart_system(login_screen_t *screen);
int login_screen_sleep_system(login_screen_t *screen);
int login_screen_hibernate_system(login_screen_t *screen);
int login_screen_lock_system(login_screen_t *screen);

/* Accessibility */
int login_screen_toggle_osk(login_screen_t *screen);
int login_screen_show_osk(login_screen_t *screen);
int login_screen_hide_osk(login_screen_t *screen);
int login_screen_set_accessibility(login_screen_t *screen, accessibility_options_t *options);

/* Theme presets */
int login_theme_default(login_theme_t *theme);
int login_theme_dark(login_theme_t *theme);
int login_theme_light(login_theme_t *theme);
int login_theme_minimal(login_theme_t *theme);
int login_theme_elegant(login_theme_t *theme);

/* Utility functions */
const char *login_get_session_type_name(u32 type);
const char *login_get_auth_method_name(u32 method);
const char *login_get_state_name(u32 state);
u64 login_get_current_time(void);
int login_format_time(u64 time, char *buffer, u32 size);
int login_format_date(u64 time, char *buffer, u32 size);

/* Global instance */
login_screen_t *login_screen_get_instance(void);

#endif /* _LOGIN_SCREEN_H */
