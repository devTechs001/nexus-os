/*
 * NEXUS OS - User Session Manager
 * gui/session/session-manager.h
 *
 * Manages user sessions, login state, and session lifecycle
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _SESSION_MANAGER_H
#define _SESSION_MANAGER_H

#include "../gui.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         SESSION MANAGER CONFIGURATION                     */
/*===========================================================================*/

#define SESSION_MANAGER_VERSION     "1.0.0"
#define SESSION_MAX_USERS           32
#define SESSION_MAX_ACTIVE          16
#define SESSION_MAX_SERVICES        64
#define SESSION_NAME_MAX            64
#define SESSION_PATH_MAX            512

/*===========================================================================*/
/*                         SESSION STATES                                    */
/*===========================================================================*/

#define SESSION_STATE_NONE          0
#define SESSION_STATE_STARTING      1
#define SESSION_STATE_ACTIVE        2
#define SESSION_STATE_IDLE          3
#define SESSION_STATE_LOCKED        4
#define SESSION_STATE_SWITCHING     5
#define SESSION_STATE_TERMINATING   6
#define SESSION_STATE_ENDED         7

/*===========================================================================*/
/*                         SESSION TYPES                                     */
/*===========================================================================*/

#define SESSION_TYPE_USER           0
#define SESSION_TYPE_SYSTEM         1
#define SESSION_TYPE_DISPLAY        2
#define SESSION_TYPE_TTY            3
#define SESSION_TYPE_REMOTE         4
#define SESSION_TYPE_GUEST          5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Session Information
 */
typedef struct session_info {
    u32 session_id;
    u32 user_id;
    u32 session_type;
    u32 state;
    char session_name[SESSION_NAME_MAX];
    char display[32];
    char tty[16];
    char seat[32];
    u64 start_time;
    u64 last_activity;
    u64 idle_time;
    pid_t leader_pid;
    pid_t session_pid;
    char working_dir[SESSION_PATH_MAX];
    char desktop_environment[64];
    char language[32];
    char timezone[64];
    bool is_active;
    bool is_local;
    bool is_remote;
    bool allows_remote;
    u32 width;
    u32 height;
    u32 color_depth;
    void *session_data;
} session_info_t;

/**
 * User Session
 */
typedef struct user_session {
    u32 session_id;
    u32 user_id;
    u32 group_id;
    char username[64];
    char full_name[128];
    char home_dir[SESSION_PATH_MAX];
    char shell[64];
    char email[128];
    char icon_path[SESSION_PATH_MAX];
    
    /* Session state */
    u32 state;
    u64 login_time;
    u64 last_activity;
    u64 idle_time;
    
    /* Environment */
    char **environment;
    u32 env_count;
    char current_dir[SESSION_PATH_MAX];
    char locale[32];
    char timezone[64];
    char keymap[32];
    
    /* Preferences */
    char theme[64];
    char wallpaper[SESSION_PATH_MAX];
    u32 font_size;
    u32 icon_size;
    bool animations_enabled;
    bool sounds_enabled;
    bool notifications_enabled;
    
    /* Session services */
    u32 services_mask;
    
    /* Statistics */
    u32 total_logins;
    u64 total_time;
    u64 session_time;
    
    /* Flags */
    bool is_admin;
    bool is_guest;
    bool is_locked;
    bool auto_login;
    bool remember_session;
    
    void *user_data;
} user_session_t;

/**
 * Session Service
 */
typedef struct session_service {
    u32 service_id;
    char name[64];
    char description[256];
    char command[SESSION_PATH_MAX];
    char pid_file[SESSION_PATH_MAX];
    pid_t pid;
    bool is_running;
    bool is_enabled;
    bool is_required;
    u32 restart_count;
    u32 max_restarts;
    u64 start_time;
    u64 stop_time;
    int exit_code;
    void (*on_start)(struct session_service *);
    void (*on_stop)(struct session_service *);
    void (*on_crash)(struct session_service *);
    void *service_data;
} session_service_t;

/**
 * Session Manager State
 */
typedef struct session_manager {
    bool initialized;
    bool running;
    u32 state;
    
    /* Current session */
    user_session_t *current_session;
    session_info_t *active_session;
    
    /* Session tracking */
    user_session_t sessions[SESSION_MAX_USERS];
    u32 session_count;
    session_info_t active_sessions[SESSION_MAX_ACTIVE];
    u32 active_count;
    
    /* Services */
    session_service_t services[SESSION_MAX_SERVICES];
    u32 service_count;
    
    /* Session management */
    u32 next_session_id;
    u32 next_service_id;
    pid_t manager_pid;
    char seat_id[32];
    char session_path[SESSION_PATH_MAX];
    
    /* Login state */
    bool login_screen_active;
    bool onboarding_completed;
    bool first_login;
    u32 failed_login_attempts;
    u64 lockout_until;
    
    /* Idle detection */
    u64 idle_threshold;
    u64 lock_threshold;
    u64 sleep_threshold;
    u64 last_input_time;
    
    /* Callbacks */
    int (*on_session_start)(struct session_manager *, user_session_t *);
    int (*on_session_end)(struct session_manager *, user_session_t *);
    int (*on_session_lock)(struct session_manager *, user_session_t *);
    int (*on_session_unlock)(struct session_manager *, user_session_t *);
    int (*on_user_switch)(struct session_manager *, user_session_t *, user_session_t *);
    int (*on_idle)(struct session_manager *, u64);
    int (*on_service_start)(struct session_manager *, session_service_t *);
    int (*on_service_stop)(struct session_manager *, session_service_t *);
    
    /* User data */
    void *user_data;
    
} session_manager_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Session manager lifecycle */
int session_manager_init(session_manager_t *manager);
int session_manager_run(session_manager_t *manager);
int session_manager_shutdown(session_manager_t *manager);
bool session_manager_is_running(session_manager_t *manager);

/* Session creation and management */
int session_manager_create_session(session_manager_t *manager, u32 user_id, user_session_t **session);
int session_manager_destroy_session(session_manager_t *manager, u32 session_id);
int session_manager_start_session(session_manager_t *manager, u32 session_id);
int session_manager_stop_session(session_manager_t *manager, u32 session_id);
int session_manager_switch_session(session_manager_t *manager, u32 session_id);
int session_manager_get_session(session_manager_t *manager, u32 session_id, user_session_t *session);
int session_manager_list_sessions(session_manager_t *manager, user_session_t *sessions, u32 *count);

/* Current session operations */
int session_manager_get_current_session(session_manager_t *manager, user_session_t *session);
int session_manager_set_current_session(session_manager_t *manager, u32 session_id);
bool session_manager_has_active_session(session_manager_t *manager);

/* User login/logout */
int session_manager_login(session_manager_t *manager, const char *username, const char *password, u32 *session_id);
int session_manager_logout(session_manager_t *manager, u32 session_id);
int session_manager_auto_login(session_manager_t *manager, u32 user_id);
int session_manager_guest_login(session_manager_t *manager, u32 *session_id);

/* Session locking */
int session_manager_lock(session_manager_t *manager);
int session_manager_unlock(session_manager_t *manager, const char *password);
bool session_manager_is_locked(session_manager_t *manager);

/* Service management */
int session_manager_register_service(session_manager_t *manager, session_service_t *service);
int session_manager_start_service(session_manager_t *manager, const char *name);
int session_manager_stop_service(session_manager_t *manager, const char *name);
int session_manager_restart_service(session_manager_t *manager, const char *name);
int session_manager_list_services(session_manager_t *manager, session_service_t *services, u32 *count);

/* Idle management */
int session_manager_set_idle_threshold(session_manager_t *manager, u64 milliseconds);
int session_manager_set_lock_threshold(session_manager_t *manager, u64 milliseconds);
int session_manager_set_sleep_threshold(session_manager_t *manager, u64 milliseconds);
int session_manager_reset_idle_timer(session_manager_t *manager);
u64 session_manager_get_idle_time(session_manager_t *manager);

/* Environment management */
int session_manager_set_environment(session_manager_t *manager, const char *key, const char *value);
int session_manager_get_environment(session_manager_t *manager, const char *key, char *value, u32 size);
int session_manager_unset_environment(session_manager_t *manager, const char *key);
char **session_manager_get_full_environment(session_manager_t *manager, u32 *count);

/* Session preferences */
int session_manager_load_preferences(session_manager_t *manager, user_session_t *session);
int session_manager_save_preferences(session_manager_t *manager, user_session_t *session);
int session_manager_set_preference(session_manager_t *manager, const char *key, const char *value);
int session_manager_get_preference(session_manager_t *manager, const char *key, char *value, u32 size);

/* Session statistics */
int session_manager_get_statistics(session_manager_t *manager, u32 session_id, void *stats);
int session_manager_reset_statistics(session_manager_t *manager, u32 session_id);

/* Utility functions */
const char *session_get_state_name(u32 state);
const char *session_get_type_name(u32 type);
u32 session_get_uid(void);
u32 session_get_gid(void);
char *session_get_username(char *buffer, u32 size);
char *session_get_home_dir(char *buffer, u32 size);
int session_set_current_directory(const char *path);
char *session_get_current_directory(char *buffer, u32 size);

/* Global instance */
session_manager_t *session_manager_get_instance(void);

#endif /* _SESSION_MANAGER_H */
