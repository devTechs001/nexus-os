/*
 * NEXUS OS - Enhanced Login Screen Implementation
 * gui/login/login-screen.c
 *
 * Modern, secure login screen with multiple authentication methods
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "login-screen.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"
#include "../../security/auth/auth.h"

/*===========================================================================*/
/*                         GLOBAL LOGIN SCREEN INSTANCE                      */
/*===========================================================================*/

static login_screen_t g_login_screen;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* UI Creation */
static int create_main_window(login_screen_t *screen);
static int create_background(login_screen_t *screen);
static int create_clock_display(login_screen_t *screen);
static int create_user_panel(login_screen_t *screen);
static int create_user_list(login_screen_t *screen);
static int create_login_form(login_screen_t *screen);
static int create_session_selector(login_screen_t *screen);
static int create_power_menu(login_screen_t *screen);
static int create_accessibility_menu(login_screen_t *screen);
static int create_on_screen_keyboard(login_screen_t *screen);

/* Event handlers */
static void on_login_button_clicked(widget_t *button);
static void on_cancel_button_clicked(widget_t *button);
static void on_user_selected(widget_t *widget);
static void on_session_changed(widget_t *widget, const char *text);
static void on_power_clicked(widget_t *widget);
static void on_accessibility_clicked(widget_t *widget);
static void on_password_changed(widget_t *widget, const char *text);
static void on_password_enter_pressed(widget_t *widget);

/* Authentication */
static int perform_authentication(login_screen_t *screen);
static int handle_login_success(login_screen_t *screen);
static int handle_login_failure(login_screen_t *screen, s32 error);

/* Updates */
static void update_clock(login_screen_t *screen);
static void update_user_display(login_screen_t *screen);
static void update_status(login_screen_t *screen, const char *msg);
static void update_animation(login_screen_t *screen);

/* Utilities */
static void load_system_info(login_screen_t *screen);
static void load_user_preferences(login_screen_t *screen);
static int save_login_history(login_screen_t *screen, u32 user_id);

/* Cached screen dimensions from boot params */
static u32 g_screen_w = 0;
static u32 g_screen_h = 0;

static void screen_dim_init(void)
{
    if (g_screen_w == 0) {
        boot_params_t *bp = get_boot_params();
        g_screen_w = (bp && bp->gfx_width) ? bp->gfx_width : 1024;
        g_screen_h = (bp && bp->gfx_height) ? bp->gfx_height : 768;
    }
}

/*===========================================================================*/
/*                         LOGIN SCREEN INITIALIZATION                       */
/*===========================================================================*/

/**
 * login_screen_init - Initialize the login screen
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_init(login_screen_t *screen)
{
    if (!screen) {
        return -EINVAL;
    }
    
    printk("[LOGIN] ========================================\n");
    printk("[LOGIN] NEXUS OS Login Screen v%s\n", LOGIN_SCREEN_VERSION);
    printk("[LOGIN] ========================================\n");
    printk("[LOGIN] Initializing login screen...\n");
    
    /* Clear structure */
    memset(screen, 0, sizeof(login_screen_t));
    screen->initialized = true;
    screen->state = LOGIN_STATE_LOCKED;
    screen->auth_method = AUTH_METHOD_PASSWORD;
    screen->max_failed_attempts = 5;
    screen->idle_timeout = 300000; /* 5 minutes */
    screen->is_24hour = true;
    screen->show_seconds = false;
    screen->secure_mode = true;
    screen->encrypt_password = true;
    screen->show_failed_message = false;
    screen->log_attempts = true;
    
    /* Apply default theme */
    login_theme_default(&screen->theme);
    strcpy(screen->current_theme_name, "default");
    
    /* Load system information */
    load_system_info(screen);
    
    /* Load users from system */
    printk("[LOGIN] Loading users...\n");
    login_screen_load_users(screen);
    printk("[LOGIN] Loaded %d user(s)\n", screen->user_count);
    
    /* Load sessions */
    printk("[LOGIN] Loading sessions...\n");
    login_screen_load_sessions(screen);
    printk("[LOGIN] Loaded %d session(s)\n", screen->session_count);
    
    /* Load accessibility options */
    screen->accessibility.high_contrast = false;
    screen->accessibility.large_text = false;
    screen->accessibility.screen_reader = false;
    screen->accessibility.on_screen_keyboard = false;
    screen->accessibility.cursor_size = 24;
    screen->accessibility.text_scale = 100;
    
    /* Create UI components */
    printk("[LOGIN] Creating UI...\n");
    
    if (create_main_window(screen) != 0) {
        printk("[LOGIN] Failed to create main window\n");
        return -1;
    }
    
    if (create_background(screen) != 0) {
        printk("[LOGIN] Failed to create background\n");
        return -1;
    }
    
    if (create_clock_display(screen) != 0) {
        printk("[LOGIN] Failed to create clock display\n");
        return -1;
    }
    
    if (create_user_panel(screen) != 0) {
        printk("[LOGIN] Failed to create user panel\n");
        return -1;
    }
    
    if (create_login_form(screen) != 0) {
        printk("[LOGIN] Failed to create login form\n");
        return -1;
    }
    
    if (create_session_selector(screen) != 0) {
        printk("[LOGIN] Failed to create session selector\n");
        return -1;
    }
    
    if (create_power_menu(screen) != 0) {
        printk("[LOGIN] Failed to create power menu\n");
        return -1;
    }
    
    if (create_accessibility_menu(screen) != 0) {
        printk("[LOGIN] Failed to create accessibility menu\n");
        return -1;
    }
    
    if (create_on_screen_keyboard(screen) != 0) {
        printk("[LOGIN] Failed to create on-screen keyboard\n");
        return -1;
    }
    
    /* Update initial display */
    update_clock(screen);
    update_user_display(screen);
    update_status(screen, "Welcome");
    
    /* Select first user if available */
    if (screen->user_count > 0) {
        login_screen_select_user(screen, 0);
    }
    
    printk("[LOGIN] Login screen initialized successfully\n");
    printk("[LOGIN] ========================================\n");
    
    return 0;
}

/**
 * login_screen_run - Start the login screen
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_run(login_screen_t *screen)
{
    if (!screen || !screen->initialized) {
        return -EINVAL;
    }
    
    printk("[LOGIN] Starting login screen...\n");
    
    screen->active = true;
    screen->visible = true;
    screen->idle_time = get_time_ms();
    
    /* Show main window */
    if (screen->main_window) {
        window_show(screen->main_window);
    }
    
    /* Start clock update timer */
    update_clock(screen);
    
    /* Focus password entry if user selected */
    if (screen->current_user && screen->password_entry) {
        widget_focus(screen->password_entry);
    }

    /* Load preferences and start animations */
    load_user_preferences(screen);
    update_animation(screen);

    printk("[LOGIN] Login screen active\n");

    return 0;
    }
/**
 * login_screen_update - Update login screen state and render
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_update(login_screen_t *screen)
{
    if (!screen || !screen->initialized) {
        return -EINVAL;
    }

    if (!screen->active) {
        return 0;
    }

    /* Update clock display */
    update_clock(screen);

    /* Check for idle timeout */
    u64 current_time = get_time_ms();
    if (current_time - screen->idle_time > screen->idle_timeout) {
        /* Lock screen due to inactivity */
        screen->state = LOGIN_STATE_LOCKED;
        screen->is_authenticated = false;
        if (screen->password_entry) {
            widget_set_text(screen->password_entry, "");
        }
        screen->password_length = 0;
        update_status(screen, "Session locked due to inactivity");
    }

    /* Handle input events */
    /* In a real implementation, would read from input subsystem */
    /* For now, just update the display */

    /* Render the screen */
    extern struct compositor *compositor_get_instance(void);
    extern int compositor_present(struct compositor *comp);
    compositor_present(compositor_get_instance());

    return 0;
}

/**
 * login_screen_shutdown - Shutdown the login screen
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_shutdown(login_screen_t *screen)
{
    if (!screen || !screen->initialized) {
        return -EINVAL;
    }
    
    printk("[LOGIN] Shutting down login screen...\n");
    
    screen->active = false;
    screen->visible = false;
    
    /* Hide main window */
    if (screen->main_window) {
        window_hide(screen->main_window);
    }
    
    /* Clear sensitive data */
    memset(screen->password_buffer, 0, LOGIN_PASSWORD_MAX);
    screen->password_length = 0;
    
    printk("[LOGIN] Login screen shutdown complete\n");
    
    return 0;
}

/**
 * login_screen_lock - Lock the screen
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_lock(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->state = LOGIN_STATE_LOCKED;
    screen->is_authenticated = false;
    memset(screen->password_buffer, 0, LOGIN_PASSWORD_MAX);
    screen->password_length = 0;
    
    if (screen->password_entry) {
        widget_set_text(screen->password_entry, "");
    }
    
    update_status(screen, "Session locked");
    
    if (screen->on_state_changed) {
        screen->on_state_changed(screen, LOGIN_STATE_LOCKED);
    }
    
    return 0;
}

/**
 * login_screen_unlock - Unlock the screen
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_unlock(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    if (screen->is_authenticated) {
        screen->state = LOGIN_STATE_UNLOCKED;
        
        if (screen->on_state_changed) {
            screen->on_state_changed(screen, LOGIN_STATE_UNLOCKED);
        }
        
        return 0;
    }
    
    return -1;
}

/**
 * login_screen_is_active - Check if login screen is active
 * @screen: Pointer to login screen structure
 *
 * Returns: true if active, false otherwise
 */
bool login_screen_is_active(login_screen_t *screen)
{
    return screen && screen->active;
}

/*===========================================================================*/
/*                         USER MANAGEMENT                                   */
/*===========================================================================*/

/**
 * login_screen_load_users - Load users from system
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_load_users(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->user_count = 0;
    
    /* In real implementation, this would query the user database */
    /* For now, create default users */
    
    /* Default admin user */
    if (screen->user_count < LOGIN_MAX_USERS) {
        user_info_t *user = &screen->users[screen->user_count++];
        user->user_id = 1000;
        user->group_id = 1000;
        strcpy(user->username, "admin");
        strcpy(user->full_name, "System Administrator");
        strcpy(user->email, "admin@nexus-os.local");
        strcpy(user->icon_path, "/usr/share/icons/users/admin.png");
        strcpy(user->home_dir, "/home/admin");
        strcpy(user->shell, "/bin/bash");
        user->is_admin = true;
        user->is_locked = false;
        user->is_guest = false;
        user->auto_login = false;
        user->require_password = true;
        user->last_login = 0;
        user->login_count = 0;
        strcpy(user->language, "en_US");
        strcpy(user->timezone, "UTC");
        strcpy(user->theme, "dark");
    }
    
    /* Default regular user */
    if (screen->user_count < LOGIN_MAX_USERS) {
        user_info_t *user = &screen->users[screen->user_count++];
        user->user_id = 1001;
        user->group_id = 1001;
        strcpy(user->username, "user");
        strcpy(user->full_name, "Regular User");
        strcpy(user->email, "user@nexus-os.local");
        strcpy(user->icon_path, "/usr/share/icons/users/user.png");
        strcpy(user->home_dir, "/home/user");
        strcpy(user->shell, "/bin/bash");
        user->is_admin = false;
        user->is_locked = false;
        user->is_guest = false;
        user->auto_login = false;
        user->require_password = true;
        user->last_login = 0;
        user->login_count = 0;
        strcpy(user->language, "en_US");
        strcpy(user->timezone, "UTC");
        strcpy(user->theme, "dark");
    }
    
    /* Guest user */
    if (screen->user_count < LOGIN_MAX_USERS) {
        user_info_t *user = &screen->users[screen->user_count++];
        user->user_id = 65534;
        user->group_id = 65534;
        strcpy(user->username, "guest");
        strcpy(user->full_name, "Guest Session");
        strcpy(user->email, "");
        strcpy(user->icon_path, "/usr/share/icons/users/guest.png");
        strcpy(user->home_dir, "/tmp/guest");
        strcpy(user->shell, "/bin/bash");
        user->is_admin = false;
        user->is_locked = false;
        user->is_guest = true;
        user->auto_login = true;
        user->require_password = false;
        user->last_login = 0;
        user->login_count = 0;
        strcpy(user->language, "en_US");
        strcpy(user->timezone, "UTC");
        strcpy(user->theme, "dark");
    }
    
    return 0;
}

/**
 * login_screen_add_user - Add a user to the login screen
 * @screen: Pointer to login screen structure
 * @user: Pointer to user info
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_add_user(login_screen_t *screen, user_info_t *user)
{
    if (!screen || !user) return -EINVAL;
    
    if (screen->user_count >= LOGIN_MAX_USERS) {
        return -1;
    }
    
    memcpy(&screen->users[screen->user_count], user, sizeof(user_info_t));
    screen->user_count++;
    
    /* Update user list UI */
    if (screen->user_list) {
        /* Would update the UI list */
    }
    
    return 0;
}

/**
 * login_screen_select_user - Select a user for login
 * @screen: Pointer to login screen structure
 * @user_index: Index of user to select
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_select_user(login_screen_t *screen, u32 user_index)
{
    if (!screen || user_index >= screen->user_count) {
        return -EINVAL;
    }
    
    screen->selected_user = user_index;
    screen->current_user = &screen->users[user_index];
    
    /* Clear password field */
    memset(screen->password_buffer, 0, LOGIN_PASSWORD_MAX);
    screen->password_length = 0;
    
    if (screen->password_entry) {
        widget_set_text(screen->password_entry, "");
    }
    
    /* Update display */
    update_user_display(screen);
    
    /* Focus password entry if user requires password */
    if (screen->current_user->require_password && screen->password_entry) {
        widget_focus(screen->password_entry);
    }
    
    /* Call callback */
    if (screen->on_user_selected) {
        screen->on_user_selected(screen, screen->current_user);
    }
    
    update_status(screen, "Enter password");
    
    return 0;
}

/**
 * login_screen_get_current_user - Get current selected user
 * @screen: Pointer to login screen structure
 * @user: Pointer to store user info
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_get_current_user(login_screen_t *screen, user_info_t *user)
{
    if (!screen || !user) return -EINVAL;
    
    if (!screen->current_user) {
        return -ENOENT;
    }
    
    memcpy(user, screen->current_user, sizeof(user_info_t));
    return 0;
}

/*===========================================================================*/
/*                         SESSION MANAGEMENT                                */
/*===========================================================================*/

/**
 * login_screen_load_sessions - Load available sessions
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_load_sessions(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->session_count = 0;
    
    /* NEXUS Desktop (default) */
    if (screen->session_count < LOGIN_MAX_SESSIONS) {
        session_info_t *session = &screen->sessions[screen->session_count++];
        session->session_id = 1;
        session->session_type = SESSION_TYPE_DESKTOP;
        strcpy(session->session_name, "NEXUS Desktop");
        strcpy(session->desktop_environment, "nexus");
        strcpy(session->session_command, "/usr/bin/nexus-desktop");
        session->is_active = true;
        session->is_default = true;
    }
    
    /* Wayland session */
    if (screen->session_count < LOGIN_MAX_SESSIONS) {
        session_info_t *session = &screen->sessions[screen->session_count++];
        session->session_id = 2;
        session->session_type = SESSION_TYPE_WAYLAND;
        strcpy(session->session_name, "Wayland");
        strcpy(session->desktop_environment, "wayland");
        strcpy(session->session_command, "/usr/bin/weston");
        session->is_active = true;
        session->is_default = false;
    }
    
    /* X11 session */
    if (screen->session_count < LOGIN_MAX_SESSIONS) {
        session_info_t *session = &screen->sessions[screen->session_count++];
        session->session_id = 3;
        session->session_type = SESSION_TYPE_X11;
        strcpy(session->session_name, "X11");
        strcpy(session->desktop_environment, "x11");
        strcpy(session->session_command, "/usr/bin/startx");
        session->is_active = true;
        session->is_default = false;
    }
    
    /* TTY session */
    if (screen->session_count < LOGIN_MAX_SESSIONS) {
        session_info_t *session = &screen->sessions[screen->session_count++];
        session->session_id = 4;
        session->session_type = SESSION_TYPE_TTY;
        strcpy(session->session_name, "Terminal");
        strcpy(session->desktop_environment, "tty");
        strcpy(session->session_command, "/bin/bash");
        session->is_active = true;
        session->is_default = false;
    }
    
    /* Select default session */
    for (u32 i = 0; i < screen->session_count; i++) {
        if (screen->sessions[i].is_default) {
            screen->selected_session = i;
            screen->current_session = &screen->sessions[i];
            break;
        }
    }
    
    return 0;
}

/**
 * login_screen_select_session - Select a session type
 * @screen: Pointer to login screen structure
 * @session_index: Index of session to select
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_select_session(login_screen_t *screen, u32 session_index)
{
    if (!screen || session_index >= screen->session_count) {
        return -EINVAL;
    }
    
    screen->selected_session = session_index;
    screen->current_session = &screen->sessions[session_index];
    
    /* Update UI */
    if (screen->session_selector) {
        /* Would update session selector UI */
    }
    
    /* Call callback */
    if (screen->on_session_changed) {
        screen->on_session_changed(screen, screen->current_session);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         AUTHENTICATION                                    */
/*===========================================================================*/

/**
 * login_screen_authenticate - Authenticate user with password
 * @screen: Pointer to login screen structure
 * @password: Password to authenticate
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_authenticate(login_screen_t *screen, const char *password)
{
    if (!screen || !password || !screen->current_user) {
        return -EINVAL;
    }
    
    screen->state = LOGIN_STATE_AUTHENTICATING;
    update_status(screen, "Authenticating...");
    
    /* Initialize auth context if needed */
    if (!screen->auth_context) {
        screen->auth_context = kmalloc(sizeof(auth_context_t));
        if (!screen->auth_context) {
            return -ENOMEM;
        }
        memset(screen->auth_context, 0, sizeof(auth_context_t));
    }
    
    /* Perform authentication */
    int result = auth_login(screen->auth_context, 
                           screen->current_user->username,
                           password);
    
    if (result == 0) {
        /* Authentication successful */
        return handle_login_success(screen);
    } else {
        /* Authentication failed */
        screen->failed_attempts++;
        
        if (screen->failed_attempts >= screen->max_failed_attempts) {
            screen->lockout_time = get_time_ms() + 60000; /* 1 minute lockout */
            update_status(screen, "Too many failed attempts. Please wait.");
        } else {
            update_status(screen, "Authentication failed");
        }
        
        return handle_login_failure(screen, result);
    }
}

/**
 * perform_authentication - Perform authentication with stored password
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int perform_authentication(login_screen_t *screen)
{
    if (!screen || !screen->current_user) {
        return -EINVAL;
    }
    
    /* Guest users don't need password */
    if (screen->current_user->is_guest || !screen->current_user->require_password) {
        return handle_login_success(screen);
    }
    
    /* Check password buffer */
    if (screen->password_length == 0) {
        update_status(screen, "Please enter password");
        return -1;
    }
    
    return login_screen_authenticate(screen, screen->password_buffer);
}

/**
 * handle_login_success - Handle successful authentication
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
static int handle_login_success(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->is_authenticated = true;
    screen->state = LOGIN_STATE_LOGGING_IN;
    screen->failed_attempts = 0;
    
    /* Update user login info */
    if (screen->current_user) {
        screen->current_user->last_login = login_get_current_time();
        screen->current_user->login_count++;
    }
    
    /* Save login history */
    save_login_history(screen, screen->current_user->user_id);
    
    update_status(screen, "Login successful");
    
    /* Call success callback */
    if (screen->on_login_success) {
        screen->on_login_success(screen, screen->current_user);
    }
    
    /* Start session transition animation */
    screen->animation_state = 1;
    screen->animation_start = get_time_ms();
    screen->animation_duration = 500;
    
    printk("[LOGIN] User '%s' logged in successfully\n", screen->current_user->username);
    
    return 0;
}

/**
 * handle_login_failure - Handle failed authentication
 * @screen: Pointer to login screen structure
 * @error: Error code
 *
 * Returns: 0
 */
static int handle_login_failure(login_screen_t *screen, s32 error)
{
    if (!screen) return -EINVAL;
    
    screen->state = LOGIN_STATE_LOCKED;
    memset(screen->password_buffer, 0, LOGIN_PASSWORD_MAX);
    screen->password_length = 0;
    
    if (screen->password_entry) {
        widget_set_text(screen->password_entry, "");
    }
    
    /* Log failed attempt */
    if (screen->log_attempts) {
        printk("[LOGIN] Failed login attempt for user '%s' (error: %d)\n",
               screen->current_user ? screen->current_user->username : "unknown",
               error);
    }
    
    /* Call failure callback */
    if (screen->on_login_failure) {
        screen->on_login_failure(screen, error);
    }
    
    /* Shake animation for error */
    screen->animation_state = 2;
    screen->animation_start = get_time_ms();
    screen->animation_duration = 300;
    
    return 0;
}

/**
 * login_screen_logout - Logout current user
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_logout(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->state = LOGIN_STATE_LOGGING_OUT;
    screen->is_authenticated = false;
    screen->current_user = NULL;
    
    memset(screen->password_buffer, 0, LOGIN_PASSWORD_MAX);
    screen->password_length = 0;
    
    update_status(screen, "Logged out");
    
    return 0;
}

/**
 * login_screen_is_authenticated - Check if authenticated
 * @screen: Pointer to login screen structure
 *
 * Returns: true if authenticated, false otherwise
 */
bool login_screen_is_authenticated(login_screen_t *screen)
{
    return screen && screen->is_authenticated;
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_main_window - Create the main login window
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_main_window(login_screen_t *screen)
{
    screen_dim_init();

    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "NEXUS OS Login");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_FULLSCREEN;
    props.bounds.x = 0;
    props.bounds.y = 0;
    props.bounds.width = g_screen_w;
    props.bounds.height = g_screen_h;
    props.background = color_from_rgba(26, 26, 46, 255);
    
    screen->main_window = window_create(&props);
    if (!screen->main_window) {
        return -1;
    }
    
    /* Create main widget container */
    screen->main_widget = panel_create(NULL, 0, 0, g_screen_w, g_screen_h);
    if (!screen->main_widget) {
        return -1;
    }
    
    return 0;
}

/**
 * create_background - Create the background with blur effect
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_background(login_screen_t *screen)
{
    screen_dim_init();
    /* Create background widget */
    screen->background_widget = panel_create(screen->main_widget, 0, 0, g_screen_w, g_screen_h);
    if (!screen->background_widget) {
        return -1;
    }
    
    /* Apply theme background */
    widget_set_colors(screen->background_widget,
                      screen->theme.text_primary,
                      screen->theme.background,
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_clock_display - Create the clock display
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_clock_display(login_screen_t *screen)
{
    screen_dim_init();
    s32 center_x = g_screen_w / 2;
    s32 top_y = 80;
    
    /* Create clock label */
    screen->clock_widget = label_create(screen->main_widget, "00:00", 
                                         center_x - 100, top_y, 200, 80);
    if (!screen->clock_widget) {
        return -1;
    }
    
    widget_set_font(screen->clock_widget, 0, 72);
    widget_set_colors(screen->clock_widget,
                      screen->theme.text_primary,
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create date label */
    screen->date_widget = label_create(screen->main_widget, "Monday, January 1, 2026",
                                        center_x - 200, top_y + 85, 400, 30);
    if (!screen->date_widget) {
        return -1;
    }
    
    widget_set_font(screen->date_widget, 0, 18);
    widget_set_colors(screen->date_widget,
                      screen->theme.text_secondary,
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create hostname label */
    screen->hostname_widget = label_create(screen->main_widget, screen->hostname,
                                            center_x - 100, top_y + 120, 200, 24);
    if (!screen->hostname_widget) {
        return -1;
    }
    
    widget_set_font(screen->hostname_widget, 0, 14);
    widget_set_colors(screen->hostname_widget,
                      screen->theme.text_hint,
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_user_panel - Create the user login panel
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_user_panel(login_screen_t *screen)
{
    s32 panel_width = screen->theme.panel_width;
    s32 panel_height = screen->theme.panel_height;
    screen_dim_init();
    s32 center_x = (g_screen_w - panel_width) / 2;
    s32 center_y = (g_screen_h - panel_height) / 2 + 50;
    
    /* Create user panel */
    screen->user_panel = panel_create(screen->main_widget, 
                                       center_x, center_y, 
                                       panel_width, panel_height);
    if (!screen->user_panel) {
        return -1;
    }
    
    widget_set_colors(screen->user_panel,
                      screen->theme.panel_fg,
                      screen->theme.panel_bg,
                      color_from_rgba(80, 80, 100, 100));
    
    /* Add user list to the panel */
    create_user_list(screen);
    
    return 0;
}

static int create_user_list(login_screen_t *screen)
{
    /* 
     * Implement user list widget for selecting different users.
     */
    if (!screen || !screen->user_panel) {
        return -1;
    }

    pr_info("  [Login] Creating user list...\n");
    
    /* Simulate adding users to the list with callbacks */
    for (u32 i = 0; i < screen->user_count && i < 5; i++) {
        widget_t *user_btn = button_create(screen->user_panel, "User",
                                          10 + (i * 120), 10, 
                                          100, 100);
        if (user_btn) {
            /* Set callback for user selection */
            user_btn->on_click = on_user_selected;
            user_btn->user_data = screen;
            user_btn->tooltip = (char*)(uintptr_t)i; /* Store index */
        }
    }

    return 0;
}

/**
 * create_login_form - Create the login form with password entry
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_login_form(login_screen_t *screen)
{
    s32 center_x = screen->theme.panel_width / 2;
    s32 y_offset = 150;
    
    /* Create user icon */
    screen->user_icon = panel_create(screen->user_panel,
                                      center_x - 50, y_offset,
                                      100, 100);
    if (!screen->user_icon) {
        return -1;
    }
    
    widget_set_colors(screen->user_icon,
                      screen->theme.text_primary,
                      screen->theme.accent,
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create username label */
    screen->username_label = label_create(screen->user_panel, "Username",
                                           30, y_offset + 120,
                                           screen->theme.panel_width - 60, 30);
    if (!screen->username_label) {
        return -1;
    }
    
    widget_set_font(screen->username_label, 0, 18);
    widget_set_colors(screen->username_label,
                      screen->theme.text_primary,
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create password entry */
    screen->password_entry = edit_create(screen->user_panel, "",
                                          30, y_offset + 160,
                                          screen->theme.panel_width - 110, 45);
    if (!screen->password_entry) {
        return -1;
    }
    
    widget_set_font(screen->password_entry, 0, 16);
    widget_set_colors(screen->password_entry,
                      screen->theme.text_primary,
                      color_from_rgba(50, 50, 70, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    screen->password_entry->on_text_changed = on_password_changed;
    screen->password_entry->user_data = screen;
    
    /* Create password show/hide button */
    widget_t *password_toggle = button_create(screen->user_panel, "👁",
                                               screen->theme.panel_width - 70, 
                                               y_offset + 160, 40, 45);
    if (!password_toggle) {
        return -1;
    }
    
    password_toggle->on_click = on_password_enter_pressed;
    password_toggle->user_data = screen;
    
    /* Create login button */
    screen->login_button = button_create(screen->user_panel, "Login",
                                          30, y_offset + 220,
                                          screen->theme.panel_width - 60, 50);
    if (!screen->login_button) {
        return -1;
    }
    
    widget_set_colors(screen->login_button,
                      screen->theme.text_primary,
                      screen->theme.accent,
                      screen->theme.accent);
    
    screen->login_button->on_click = on_login_button_clicked;
    screen->login_button->user_data = screen;
    
    /* Create cancel button */
    screen->cancel_button = button_create(screen->user_panel, "Cancel",
                                           30, y_offset + 280,
                                           screen->theme.panel_width - 60, 40);
    if (!screen->cancel_button) {
        return -1;
    }
    
    widget_set_colors(screen->cancel_button,
                      screen->theme.text_primary,
                      color_from_rgba(60, 60, 80, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    screen->cancel_button->on_click = on_cancel_button_clicked;
    screen->cancel_button->user_data = screen;
    
    /* Create status label */
    screen->status_label = label_create(screen->user_panel, "Welcome",
                                         30, y_offset + 330,
                                         screen->theme.panel_width - 60, 24);
    if (!screen->status_label) {
        return -1;
    }
    
    widget_set_font(screen->status_label, 0, 12);
    widget_set_colors(screen->status_label,
                      screen->theme.text_secondary,
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_session_selector - Create session type selector
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_session_selector(login_screen_t *screen)
{
    screen_dim_init();
    s32 center_x = (g_screen_w - screen->theme.panel_width) / 2;
    s32 y = g_screen_h - 100;
    
    screen->session_selector = combobox_create(screen->main_widget,
                                                center_x, y, 200, 35);
    if (!screen->session_selector) {
        return -1;
    }
    
    /* Add session options */
    for (u32 i = 0; i < screen->session_count; i++) {
        /* Would add items to combobox */
    }
    
    screen->session_selector->on_text_changed = on_session_changed;
    screen->session_selector->user_data = screen;
    
    return 0;
}

/**
 * create_power_menu - Create power menu (shutdown, restart, etc.)
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_power_menu(login_screen_t *screen)
{
    s32 button_size = 40;
    s32 spacing = 10;
    s32 total_width = (button_size + spacing) * 3;
    screen_dim_init();
    s32 x = g_screen_w - total_width - 20;
    s32 y = g_screen_h - button_size - 20;

    /* Shutdown button */
    widget_t *shutdown_btn = button_create(screen->main_widget, "⏻",
                                            x, y, button_size, button_size);
    if (!shutdown_btn) return -1;

    shutdown_btn->on_click = on_power_clicked;
    shutdown_btn->user_data = (void*)(uintptr_t)1; /* Power action ID */
    shutdown_btn->tooltip = "Shutdown";

    /* Restart button */
    widget_t *restart_btn = button_create(screen->main_widget, "⟳",
                                           x + button_size + spacing, y,
                                           button_size, button_size);
    if (!restart_btn) return -1;

    restart_btn->on_click = on_power_clicked;
    restart_btn->user_data = (void*)(uintptr_t)2;
    restart_btn->tooltip = "Restart";

    /* Sleep button */
    widget_t *sleep_btn = button_create(screen->main_widget, "⏾",
                                         x + (button_size + spacing) * 2, y,
                                         button_size, button_size);
    if (!sleep_btn) return -1;

    sleep_btn->on_click = on_power_clicked;
    sleep_btn->user_data = (void*)(uintptr_t)3;
    sleep_btn->tooltip = "Sleep";

    screen->power_menu = shutdown_btn; /* Reference to power menu */
    
    return 0;
}

/**
 * create_accessibility_menu - Create accessibility options button
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_accessibility_menu(login_screen_t *screen)
{
    screen_dim_init();
    s32 button_size = 40;
    s32 x = 20;
    s32 y = g_screen_h - button_size - 20;
    
    screen->accessibility_button = button_create(screen->main_widget, "♿",
                                                  x, y, button_size, button_size);
    if (!screen->accessibility_button) {
        return -1;
    }
    
    screen->accessibility_button->on_click = on_accessibility_clicked;
    screen->accessibility_button->user_data = screen;
    screen->accessibility_button->tooltip = "Accessibility Options";
    
    return 0;
}

/**
 * create_on_screen_keyboard - Create on-screen keyboard
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_on_screen_keyboard(login_screen_t *screen)
{
    screen_dim_init();
    s32 kb_width = 800;
    s32 kb_height = 300;
    s32 x = (g_screen_w - kb_width) / 2;
    s32 y = g_screen_h - kb_height - 20;
    
    screen->osk_widget = panel_create(screen->main_widget, x, y, kb_width, kb_height);
    if (!screen->osk_widget) {
        return -1;
    }
    
    widget_set_colors(screen->osk_widget,
                      screen->theme.text_primary,
                      color_from_rgba(30, 30, 45, 240),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Initially hidden */
    widget_hide(screen->osk_widget);
    screen->osk_visible = false;
    
    /* Create keyboard keys (simplified - would create full keyboard) */
    const char *rows[] = {
        "1234567890-=",
        "QWERTYUIOP[]",
        "ASDFGHJKL;'",
        "ZXCVBNM,./",
        "SPACE"
    };
    
    s32 key_width = 60;
    s32 key_height = 50;
    s32 key_spacing = 5;
    s32 start_y = 10;
    
    for (u32 row = 0; row < 5; row++) {
        s32 row_width = strlen(rows[row]) * (key_width + key_spacing);
        s32 start_x = (kb_width - row_width) / 2;
        
        for (u32 i = 0; rows[row][i] != '\0'; i++) {
            char key_label[8];
            if (rows[row][i] == ' ') {
                strcpy(key_label, "SPACE");
            } else {
                key_label[0] = rows[row][i];
                key_label[1] = '\0';
            }
            
            widget_t *key = button_create(screen->osk_widget, key_label,
                                          start_x + i * (key_width + key_spacing),
                                          start_y + row * (key_height + key_spacing),
                                          key_width, key_height);
            if (key) {
                key->user_data = screen;
                /* Would add key click handler */
            }
        }
    }
    
    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

/**
 * on_login_button_clicked - Handle login button click
 * @button: Button widget
 */
static void on_login_button_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    login_screen_t *screen = (login_screen_t *)button->user_data;
    perform_authentication(screen);
}

/**
 * on_cancel_button_clicked - Handle cancel button click
 * @button: Button widget
 */
static void on_cancel_button_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    login_screen_t *screen = (login_screen_t *)button->user_data;
    
    /* Clear password field */
    memset(screen->password_buffer, 0, LOGIN_PASSWORD_MAX);
    screen->password_length = 0;
    
    if (screen->password_entry) {
        widget_set_text(screen->password_entry, "");
    }
    
    update_status(screen, "Login cancelled");
}

/**
 * on_password_changed - Handle password entry change
 * @widget: Edit widget
 * @text: New text
 */
static void on_password_changed(widget_t *widget, const char *text)
{
    if (!widget || !widget->user_data || !text) return;
    
    login_screen_t *screen = (login_screen_t *)widget->user_data;
    
    /* Update password buffer */
    strncpy(screen->password_buffer, text, LOGIN_PASSWORD_MAX - 1);
    screen->password_buffer[LOGIN_PASSWORD_MAX - 1] = '\0';
    screen->password_length = strlen(screen->password_buffer);
}

/**
 * on_password_enter_pressed - Handle enter key in password field
 * @widget: Edit widget
 */
static void on_password_enter_pressed(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    login_screen_t *screen = (login_screen_t *)widget->user_data;
    
    /* Toggle password visibility */
    screen->password_visible = !screen->password_visible;
    
    /* In real implementation, would toggle password echo mode */
}

/**
 * on_user_selected - Handle user selection
 * @widget: Widget that triggered selection
 */
static void on_user_selected(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    login_screen_t *screen = (login_screen_t *)widget->user_data;
    u32 user_index = (u32)(uintptr_t)widget->tooltip; /* Using tooltip as temp storage */
    
    login_screen_select_user(screen, user_index);
}

/**
 * on_session_changed - Handle session type change
 * @widget: Combobox widget
 * @text: Selected session name
 */
static void on_session_changed(widget_t *widget, const char *text)
{
    if (!widget || !widget->user_data) return;
    
    login_screen_t *screen = (login_screen_t *)widget->user_data;
    /* Would get selected index from combobox based on text */
    u32 session_index = 0;
    
    login_screen_select_session(screen, session_index);
}

/**
 * on_power_clicked - Handle power button click
 * @widget: Button widget
 */
static void on_power_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    login_screen_t *screen = widget->user_data;
    u32 action = (u32)(uintptr_t)widget->user_data;
    
    switch (action) {
        case 1: /* Shutdown */
            login_screen_shutdown_system(screen);
            break;
        case 2: /* Restart */
            login_screen_restart_system(screen);
            break;
        case 3: /* Sleep */
            login_screen_sleep_system(screen);
            break;
    }
    
    if (screen->on_power_action) {
        screen->on_power_action(screen, action);
    }
}

/**
 * on_accessibility_clicked - Handle accessibility button click
 * @widget: Button widget
 */
static void on_accessibility_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    login_screen_t *screen = (login_screen_t *)widget->user_data;
    login_screen_toggle_osk(screen);
}

/*===========================================================================*/
/*                         UPDATES                                           */
/*===========================================================================*/

/**
 * update_clock - Update the clock display
 * @screen: Pointer to login screen structure
 */
static void update_clock(login_screen_t *screen)
{
    if (!screen) return;
    
    u64 current_time = login_get_current_time();
    
    /* Extract time components */
    u32 total_seconds = (u32)(current_time / 1000);
    screen->second = total_seconds % 60;
    screen->minute = (total_seconds / 60) % 60;
    screen->hour = (total_seconds / 3600) % 24;
    screen->weekday = (u32)(current_time / 86400000) % 7;
    
    /* Format time string */
    char time_str[32];
    if (screen->is_24hour) {
        if (screen->show_seconds) {
            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                     screen->hour, screen->minute, screen->second);
        } else {
            snprintf(time_str, sizeof(time_str), "%02d:%02d",
                     screen->hour, screen->minute);
        }
    } else {
        const char *ampm = screen->hour >= 12 ? "PM" : "AM";
        u32 hour_12 = screen->hour % 12;
        if (hour_12 == 0) hour_12 = 12;
        if (screen->show_seconds) {
            snprintf(time_str, sizeof(time_str), "%d:%02d:%02d %s",
                     hour_12, screen->minute, screen->second, ampm);
        } else {
            snprintf(time_str, sizeof(time_str), "%d:%02d %s",
                     hour_12, screen->minute, ampm);
        }
    }
    
    /* Update clock widget */
    if (screen->clock_widget) {
        widget_set_text(screen->clock_widget, time_str);
    }
    
    /* Format date string */
    const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", 
                          "Thursday", "Friday", "Saturday"};
    const char *months[] = {"January", "February", "March", "April", "May", "June",
                            "July", "August", "September", "October", "November", "December"};
    
    char date_str[64];
    snprintf(date_str, sizeof(date_str), "%s, %s %d, 2026",
             days[screen->weekday], months[screen->month], screen->day);
    
    if (screen->date_widget) {
        widget_set_text(screen->date_widget, date_str);
    }
}

/**
 * update_user_display - Update user display in login panel
 * @screen: Pointer to login screen structure
 */
static void update_user_display(login_screen_t *screen)
{
    if (!screen || !screen->current_user) return;
    
    /* Update username label */
    if (screen->username_label) {
        widget_set_text(screen->username_label, screen->current_user->full_name);
    }
    
    /* Update user icon (would load actual icon image) */
    if (screen->user_icon) {
        /* Would set icon image */
    }
}

/**
 * update_status - Update status message
 * @screen: Pointer to login screen structure
 * @msg: Status message
 */
static void update_status(login_screen_t *screen, const char *msg)
{
    if (!screen || !msg) return;
    
    strncpy(screen->status_message, msg, sizeof(screen->status_message) - 1);
    screen->status_message[sizeof(screen->status_message) - 1] = '\0';
    
    if (screen->status_label) {
        widget_set_text(screen->status_label, screen->status_message);
    }
}

/**
 * update_animation - Update animation state
 * @screen: Pointer to login screen structure
 */
static void update_animation(login_screen_t *screen)
{
    if (!screen || screen->animation_state == 0) return;
    
    u64 elapsed = get_time_ms() - screen->animation_start;
    screen->animation_progress = (float)elapsed / (float)screen->animation_duration;
    
    if (screen->animation_progress >= 1.0f) {
        screen->animation_progress = 1.0f;
        screen->animation_state = 0;
        
        /* Animation complete */
        if (screen->state == LOGIN_STATE_LOGGING_IN && screen->is_authenticated) {
            /* Would transition to desktop */
            printk("[LOGIN] Transitioning to desktop...\n");
        }
    }
}

/*===========================================================================*/
/*                         POWER MANAGEMENT                                  */
/*===========================================================================*/

/**
 * login_screen_shutdown_system - Shutdown the system
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_shutdown_system(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->state = LOGIN_STATE_SHUTDOWN;
    update_status(screen, "Shutting down...");
    
    printk("[LOGIN] System shutdown initiated\n");
    
    /* Would call system shutdown */
    
    return 0;
}

/**
 * login_screen_restart_system - Restart the system
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_restart_system(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->state = LOGIN_STATE_RESTART;
    update_status(screen, "Restarting...");
    
    printk("[LOGIN] System restart initiated\n");
    
    /* Would call system reboot */
    
    return 0;
}

/**
 * login_screen_sleep_system - Put system to sleep
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_sleep_system(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->state = LOGIN_STATE_SLEEP;
    update_status(screen, "Sleeping...");
    
    printk("[LOGIN] System sleep initiated\n");
    
    /* Would call system sleep */
    
    return 0;
}

/*===========================================================================*/
/*                         ACCESSIBILITY                                     */
/*===========================================================================*/

/**
 * login_screen_toggle_osk - Toggle on-screen keyboard
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_toggle_osk(login_screen_t *screen)
{
    if (!screen) return -EINVAL;
    
    screen->osk_visible = !screen->osk_visible;
    
    if (screen->osk_visible) {
        return login_screen_show_osk(screen);
    } else {
        return login_screen_hide_osk(screen);
    }
}

/**
 * login_screen_show_osk - Show on-screen keyboard
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_show_osk(login_screen_t *screen)
{
    if (!screen || !screen->osk_widget) return -EINVAL;
    
    widget_show(screen->osk_widget);
    screen->osk_visible = true;
    screen->accessibility.on_screen_keyboard = true;
    
    return 0;
}

/**
 * login_screen_hide_osk - Hide on-screen keyboard
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success
 */
int login_screen_hide_osk(login_screen_t *screen)
{
    if (!screen || !screen->osk_widget) return -EINVAL;
    
    widget_hide(screen->osk_widget);
    screen->osk_visible = false;
    screen->accessibility.on_screen_keyboard = false;
    
    return 0;
}

/*===========================================================================*/
/*                         THEME PRESETS                                     */
/*===========================================================================*/

/**
 * login_theme_default - Apply default theme
 * @theme: Pointer to theme structure
 *
 * Returns: 0 on success
 */
int login_theme_default(login_theme_t *theme)
{
    if (!theme) return -EINVAL;
    
    strcpy(theme->name, "default");
    strcpy(theme->background_color, "#1A1A2E");
    theme->blur_effect = 1;
    theme->blur_radius = 20;
    
    theme->background = color_from_rgba(26, 26, 46, 255);
    theme->overlay = color_from_rgba(0, 0, 0, 100);
    theme->panel_bg = color_from_rgba(30, 30, 50, 200);
    theme->panel_fg = color_from_rgba(255, 255, 255, 255);
    theme->text_primary = color_from_rgba(255, 255, 255, 255);
    theme->text_secondary = color_from_rgba(200, 200, 200, 255);
    theme->text_hint = color_from_rgba(150, 150, 150, 255);
    theme->accent = color_from_rgba(233, 69, 96, 255);
    theme->error = color_from_rgba(255, 80, 80, 255);
    theme->success = color_from_rgba(80, 255, 120, 255);
    theme->warning = color_from_rgba(255, 200, 80, 255);
    
    theme->font_size_title = 72;
    theme->font_size_subtitle = 18;
    theme->font_size_body = 14;
    theme->font_size_small = 12;
    
    theme->user_icon_size = 100;
    theme->panel_width = 400;
    theme->panel_height = 450;
    theme->panel_corner_radius = 20;
    theme->animation_duration = 300;
    
    theme->show_clock = true;
    theme->show_date = true;
    theme->show_hostname = true;
    theme->show_session_selector = true;
    theme->show_power_menu = true;
    theme->show_accessibility = true;
    
    return 0;
}

/**
 * login_theme_dark - Apply dark theme
 * @theme: Pointer to theme structure
 *
 * Returns: 0 on success
 */
int login_theme_dark(login_theme_t *theme)
{
    if (!theme) return -EINVAL;
    
    strcpy(theme->name, "dark");
    
    theme->background = color_from_rgba(10, 10, 15, 255);
    theme->panel_bg = color_from_rgba(25, 25, 35, 200);
    theme->text_primary = color_from_rgba(255, 255, 255, 255);
    theme->text_secondary = color_from_rgba(180, 180, 180, 255);
    theme->accent = color_from_rgba(0, 150, 255, 255);
    
    return login_theme_default(theme); /* Copy rest from default */
}

/**
 * login_theme_light - Apply light theme
 * @theme: Pointer to theme structure
 *
 * Returns: 0 on success
 */
int login_theme_light(login_theme_t *theme)
{
    if (!theme) return -EINVAL;
    
    strcpy(theme->name, "light");
    
    theme->background = color_from_rgba(240, 240, 245, 255);
    theme->panel_bg = color_from_rgba(255, 255, 255, 200);
    theme->text_primary = color_from_rgba(30, 30, 40, 255);
    theme->text_secondary = color_from_rgba(80, 80, 90, 255);
    theme->accent = color_from_rgba(0, 120, 215, 255);
    
    return login_theme_default(theme);
}

/**
 * login_theme_minimal - Apply minimal theme
 * @theme: Pointer to theme structure
 *
 * Returns: 0 on success
 */
int login_theme_minimal(login_theme_t *theme)
{
    if (!theme) return -EINVAL;
    
    strcpy(theme->name, "minimal");
    
    theme->panel_width = 350;
    theme->panel_height = 380;
    theme->panel_corner_radius = 0;
    theme->show_clock = false;
    theme->show_date = false;
    
    return login_theme_default(theme);
}

/**
 * login_theme_elegant - Apply elegant theme
 * @theme: Pointer to theme structure
 *
 * Returns: 0 on success
 */
int login_theme_elegant(login_theme_t *theme)
{
    if (!theme) return -EINVAL;
    
    strcpy(theme->name, "elegant");
    
    theme->background = color_from_rgba(20, 20, 30, 255);
    theme->panel_bg = color_from_rgba(40, 40, 55, 220);
    theme->accent = color_from_rgba(255, 180, 80, 255);
    theme->panel_corner_radius = 30;
    
    return login_theme_default(theme);
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * load_system_info - Load system information
 * @screen: Pointer to login screen structure
 */
static void load_system_info(login_screen_t *screen)
{
    if (!screen) return;
    
    /* Get hostname */
    strcpy(screen->hostname, "NEXUS-OS");
    
    /* Get welcome message */
    strcpy(screen->welcome_message, "Welcome to NEXUS OS");
}

/**
 * load_user_preferences - Load user preferences
 * @screen: Pointer to login screen structure
 */
static void load_user_preferences(login_screen_t *screen)
{
    if (!screen || !screen->current_user) return;
    
    /* Would load user preferences from config */
}

/**
 * save_login_history - Save login history
 * @screen: Pointer to login screen structure
 * @user_id: User ID
 *
 * Returns: 0 on success
 */
static int save_login_history(login_screen_t *screen, u32 user_id)
{
    if (!screen) return -EINVAL;
    
    /* Would save to /var/log/nexus/login.log */
    printk("[LOGIN] User %d logged in at %d\n", 
           user_id, (u32)login_get_current_time());
    
    return 0;
}

/**
 * login_get_current_time - Get current time in milliseconds
 *
 * Returns: Current time
 */
u64 login_get_current_time(void)
{
    return get_time_ms();
}

/**
 * login_get_session_type_name - Get session type name
 * @type: Session type
 *
 * Returns: Session type name string
 */
const char *login_get_session_type_name(u32 type)
{
    switch (type) {
        case SESSION_TYPE_DESKTOP:  return "Desktop";
        case SESSION_TYPE_WAYLAND:  return "Wayland";
        case SESSION_TYPE_X11:      return "X11";
        case SESSION_TYPE_TTY:      return "Terminal";
        case SESSION_TYPE_REMOTE:   return "Remote";
        case SESSION_TYPE_CUSTOM:   return "Custom";
        default:                    return "Unknown";
    }
}

/**
 * login_get_auth_method_name - Get authentication method name
 * @method: Authentication method
 *
 * Returns: Auth method name string
 */
const char *login_get_auth_method_name(u32 method)
{
    switch (method) {
        case AUTH_METHOD_PASSWORD:  return "Password";
        case AUTH_METHOD_PIN:       return "PIN";
        case AUTH_METHOD_FINGERPRINT: return "Fingerprint";
        case AUTH_METHOD_FACE:      return "Face Recognition";
        case AUTH_METHOD_SMARTCARD: return "Smart Card";
        case AUTH_METHOD_TOKEN:     return "Security Token";
        case AUTH_METHOD_PATTERN:   return "Pattern";
        default:                    return "Unknown";
    }
}

/**
 * login_get_state_name - Get login state name
 * @state: Login state
 *
 * Returns: State name string
 */
const char *login_get_state_name(u32 state)
{
    switch (state) {
        case LOGIN_STATE_LOCKED:        return "Locked";
        case LOGIN_STATE_UNLOCKED:      return "Unlocked";
        case LOGIN_STATE_AUTHENTICATING: return "Authenticating";
        case LOGIN_STATE_LOGGING_IN:    return "Logging In";
        case LOGIN_STATE_LOGGING_OUT:   return "Logging Out";
        case LOGIN_STATE_SHUTDOWN:      return "Shutdown";
        case LOGIN_STATE_RESTART:       return "Restart";
        case LOGIN_STATE_SLEEP:         return "Sleep";
        case LOGIN_STATE_HIBERNATE:     return "Hibernate";
        default:                        return "Unknown";
    }
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * login_screen_get_instance - Get the global login screen instance
 *
 * Returns: Pointer to global login screen instance
 */
login_screen_t *login_screen_get_instance(void)
{
    return &g_login_screen;
}

/*===========================================================================*/
/*        LOGIN SCREEN API THAT WAS DECLARED BUT NEVER IMPLEMENTED           */
/*===========================================================================*/
/*
 * login-screen.h advertised all of the functions below, and login-screen.c
 * used some of them internally, but none had a body anywhere in the tree.
 * That left the screen only half-wired: there was no way to lock it, show an
 * error, switch users, or inspect the session list from outside.
 */

/**
 * login_screen_update_clock - Refresh the on-screen time from the RTC
 * @screen: Pointer to login screen structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_update_clock(login_screen_t *screen)
{
    u64 now;
    u32 secs;
    u32 days;

    if (!screen) {
        return -EINVAL;
    }

    now = login_get_current_time();
    secs = (u32)(now / 1000);

    screen->hour   = (secs / 3600) % 24;
    screen->minute = (secs / 60) % 60;
    screen->second = secs % 60;

    /*
     * Civil date from days since epoch (a compact version of Howard Hinnant's
     * civil_from_days, which is exact for the whole range we care about).
     */
    days = secs / 86400;
    days += 719468;
    u32 era = days / 146097;
    u32 doe = days - era * 146097;
    u32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    u32 y   = yoe + era * 400;
    u32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    u32 mp  = (5 * doy + 2) / 153;
    u32 d   = doy - (153 * mp + 2) / 5 + 1;
    u32 m   = (mp < 10) ? mp + 3 : mp - 9;

    if (m <= 2) {
        y++;
    }

    screen->day   = d;
    screen->month = m;
    screen->year  = y;
    screen->weekday = (u32)((days + 4) % 7);    /* 0 = Sunday */

    return 0;
}

/**
 * login_screen_update_status - Set the status line
 */
int login_screen_update_status(login_screen_t *screen, const char *status)
{
    if (!screen) {
        return -EINVAL;
    }

    update_status(screen, status ? status : "");
    return 0;
}

/**
 * login_screen_update_message - Set the welcome message
 */
int login_screen_update_message(login_screen_t *screen, const char *message)
{
    if (!screen) {
        return -EINVAL;
    }

    if (message) {
        strncpy(screen->welcome_message, message, sizeof(screen->welcome_message) - 1);
        screen->welcome_message[sizeof(screen->welcome_message) - 1] = '\0';
    } else {
        screen->welcome_message[0] = '\0';
    }

    return 0;
}

/**
 * login_screen_show_error - Display an error, clearing any status line
 */
int login_screen_show_error(login_screen_t *screen, const char *error)
{
    if (!screen) {
        return -EINVAL;
    }

    if (error) {
        strncpy(screen->error_message, error, sizeof(screen->error_message) - 1);
        screen->error_message[sizeof(screen->error_message) - 1] = '\0';
    } else {
        screen->error_message[0] = '\0';
    }

    /* An error replaces the status line: showing both is contradictory. */
    update_status(screen, screen->error_message);

    return 0;
}

/**
 * login_screen_clear_error - Clear the error and restore a neutral status
 */
int login_screen_clear_error(login_screen_t *screen)
{
    if (!screen) {
        return -EINVAL;
    }

    memset(screen->error_message, 0, sizeof(screen->error_message));
    update_status(screen, "");

    return 0;
}

/**
 * login_screen_set_theme - Select a theme preset by name
 */
int login_screen_set_theme(login_screen_t *screen, const char *theme_name)
{
    int rc;

    if (!screen || !theme_name) {
        return -EINVAL;
    }

    if (strcmp(theme_name, "dark") == 0) {
        rc = login_theme_dark(&screen->theme);
    } else if (strcmp(theme_name, "light") == 0) {
        rc = login_theme_light(&screen->theme);
    } else if (strcmp(theme_name, "minimal") == 0) {
        rc = login_theme_minimal(&screen->theme);
    } else if (strcmp(theme_name, "elegant") == 0) {
        rc = login_theme_elegant(&screen->theme);
    } else {
        rc = login_theme_default(&screen->theme);
    }

    if (rc != 0) {
        return rc;
    }

    strncpy(screen->current_theme_name, theme_name, sizeof(screen->current_theme_name) - 1);
    screen->current_theme_name[sizeof(screen->current_theme_name) - 1] = '\0';

    return 0;
}

/**
 * login_screen_apply_theme - Apply a caller-supplied theme structure
 */
int login_screen_apply_theme(login_screen_t *screen, login_theme_t *theme)
{
    if (!screen || !theme) {
        return -EINVAL;
    }

    memcpy(&screen->theme, theme, sizeof(login_theme_t));

    return 0;
}

/**
 * login_screen_set_accessibility - Update accessibility options
 */
int login_screen_set_accessibility(login_screen_t *screen,
                                   accessibility_options_t *options)
{
    if (!screen || !options) {
        return -EINVAL;
    }

    memcpy(&screen->accessibility, options, sizeof(accessibility_options_t));

    return 0;
}

/**
 * login_screen_list_users - Enumerate users shown on the login screen
 * @screen: Pointer to login screen structure
 * @users: Output array
 * @count: In: capacity of @users. Out: number written.
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_screen_list_users(login_screen_t *screen, user_info_t *users, u32 *count)
{
    u32 n;

    if (!screen || !users || !count) {
        return -EINVAL;
    }

    n = screen->user_count;
    if (n > *count) {
        n = *count;
    }

    for (u32 i = 0; i < n; i++) {
        memcpy(&users[i], &screen->users[i], sizeof(user_info_t));
    }

    *count = n;

    return 0;
}

/**
 * login_screen_list_sessions - Enumerate open sessions
 */
int login_screen_list_sessions(login_screen_t *screen, session_info_t *sessions,
                               u32 *count)
{
    u32 n;

    if (!screen || !sessions || !count) {
        return -EINVAL;
    }

    n = screen->session_count;
    if (n > *count) {
        n = *count;
    }

    for (u32 i = 0; i < n; i++) {
        memcpy(&sessions[i], &screen->sessions[i], sizeof(session_info_t));
    }

    *count = n;

    return 0;
}

/**
 * login_screen_get_current_session - Return the active session, if any
 */
int login_screen_get_current_session(login_screen_t *screen, session_info_t *session)
{
    if (!screen || !session) {
        return -EINVAL;
    }

    for (u32 i = 0; i < screen->session_count; i++) {
        if (screen->sessions[i].is_active) {
            memcpy(session, &screen->sessions[i], sizeof(session_info_t));
            return 0;
        }
    }

    return -ENOENT;
}

/**
 * login_screen_add_session - Register a new session for the current user
 */
int login_screen_add_session(login_screen_t *screen, session_info_t *session)
{
    session_info_t *s;

    if (!screen || !session) {
        return -EINVAL;
    }

    if (screen->session_count >= LOGIN_MAX_SESSIONS) {
        return -ENOMEM;
    }

    s = &screen->sessions[screen->session_count++];
    memcpy(s, session, sizeof(session_info_t));

    /* Fill in whatever the caller left blank. */
    if (s->session_id == 0) {
        s->session_id = 1000 + screen->session_count;
    }
    if (s->user_id == 0 && screen->current_user) {
        s->user_id = screen->current_user->user_id;
    }
    if (s->session_name[0] == '\0') {
        snprintf(s->session_name, sizeof(s->session_name),
                 "session%u", s->session_id);
    }
    if (s->display[0] == '\0') {
        snprintf(s->display, sizeof(s->display), ":0");
    }
    if (s->start_time == 0) {
        s->start_time = login_get_current_time();
    }
    s->last_activity = s->start_time;
    s->is_active = true;
    s->is_default = (screen->session_count == 1);

    /* Only one session may be active at a time. */
    for (u32 i = 0; i + 1 < screen->session_count; i++) {
        screen->sessions[i].is_active = false;
    }

    return 0;
}

/**
 * login_screen_switch_user - Move focus to a different user account
 */
int login_screen_switch_user(login_screen_t *screen, u32 user_id)
{
    if (!screen) {
        return -EINVAL;
    }

    for (u32 i = 0; i < screen->user_count; i++) {
        if (screen->users[i].user_id == user_id) {
            screen->current_user = &screen->users[i];
            screen->is_authenticated = false;
            screen->password_length = 0;
            memset(screen->password_buffer, 0, LOGIN_PASSWORD_MAX);

            if (screen->password_entry) {
                widget_set_text(screen->password_entry, "");
                widget_focus(screen->password_entry);
            }

            update_status(screen, "Enter password");
            return 0;
        }
    }

    return -ENOENT;
}

/**
 * login_screen_remove_user - Delete a user account from the login screen
 *
 * The account list is packed, so the tail has to be shifted down. Any
 * session belonging to the removed user is dropped as well.
 */
int login_screen_remove_user(login_screen_t *screen, u32 user_id)
{
    u32 i;

    if (!screen) {
        return -EINVAL;
    }

    for (i = 0; i < screen->user_count; i++) {
        if (screen->users[i].user_id == user_id) {
            /* Refuse to remove the account currently selected. */
            if (screen->current_user == &screen->users[i]) {
                return -EBUSY;
            }

            for (u32 j = i; j + 1 < screen->user_count; j++) {
                screen->users[j] = screen->users[j + 1];
            }
            screen->user_count--;

            /* Pack sessions too. */
            for (u32 j = 0; j < screen->session_count; ) {
                if (screen->sessions[j].user_id == user_id) {
                    for (u32 k = j; k + 1 < screen->session_count; k++) {
                        screen->sessions[k] = screen->sessions[k + 1];
                    }
                    screen->session_count--;
                } else {
                    j++;
                }
            }

            return 0;
        }
    }

    return -ENOENT;
}

/*===========================================================================*/
/*                       ALTERNATE AUTHENTICATION                            */
/*===========================================================================*/

/**
 * login_screen_authenticate_pin - Authenticate with a numeric PIN
 *
 * A PIN is a credential like any other, so it goes through the exact same
 * auth path as a password - including the attempt counter and lockout. It is
 * compared against the account's password field rather than a separate store,
 * because NEXUS has no dedicated PIN backend.
 */
int login_screen_authenticate_pin(login_screen_t *screen, const char *pin)
{
    if (!screen || !pin) {
        return -EINVAL;
    }

    if (!*pin) {
        login_screen_show_error(screen, "PIN required");
        return -EINVAL;
    }

    return login_screen_authenticate(screen, pin);
}

/**
 * login_screen_authenticate_face - Face unlock
 *
 * Returns -ENOSYS deliberately: there is no camera pipeline feeding a face
 * matcher in this tree, and pretending otherwise would let anyone in.
 */
int login_screen_authenticate_face(login_screen_t *screen)
{
    if (!screen) {
        return -EINVAL;
    }

    login_screen_show_error(screen,
        "Face unlock is not available (no biometric pipeline)");
    return -ENOSYS;
}

/**
 * login_screen_authenticate_fingerprint - Fingerprint unlock
 *
 * Same reasoning as face unlock: honestly unavailable rather than fake.
 */
int login_screen_authenticate_fingerprint(login_screen_t *screen)
{
    if (!screen) {
        return -EINVAL;
    }

    login_screen_show_error(screen,
        "Fingerprint unlock is not available (no sensor pipeline)");
    return -ENOSYS;
}

/*===========================================================================*/
/*                          POWER MANAGEMENT                                 */
/*===========================================================================*/

/**
 * login_screen_hibernate_system - Hibernate the machine
 *
 * Hibernation needs swap-space writeout and a resume image; neither exists
 * yet. Report that instead of silently doing nothing after the UI says it
 * worked.
 */
int login_screen_hibernate_system(login_screen_t *screen)
{
    if (!screen) {
        return -EINVAL;
    }

    login_screen_show_error(screen,
        "Hibernation is not supported (no resume image support)");
    return -ENOSYS;
}

/**
 * login_screen_lock_system - Lock the session from the login screen
 *
 * Distinct from login_screen_lock(): that locks the *screen object*, while
 * this is the power-menu "Lock" action, which also tears the session down to
 * the lock state so the desktop has to be re-authenticated.
 */
int login_screen_lock_system(login_screen_t *screen)
{
    int rc;

    if (!screen) {
        return -EINVAL;
    }

    rc = login_screen_lock(screen);
    if (rc != 0) {
        return rc;
    }

    /* Suspending the session means dropping the authentication context. */
    screen->state = LOGIN_STATE_LOCKED;
    screen->is_authenticated = false;
    screen->failed_attempts = 0;

    update_status(screen, "Session locked");
    pr_info("[LOGIN] Session locked by user\n");

    return 0;
}

/*===========================================================================*/
/*                          UTILITIES                                        */
/*===========================================================================*/

/**
 * login_format_time - Format a millisecond timestamp as HH:MM[:SS]
 *
 * @time: Timestamp in milliseconds since boot/epoch
 * @buffer: Destination
 * @size: Size of @buffer
 *
 * Returns: 0 on success, negative error code on failure
 */
int login_format_time(u64 time, char *buffer, u32 size)
{
    u32 secs;
    u32 h, m, s;

    if (!buffer || size < 6) {
        return -EINVAL;
    }

    secs = (u32)(time / 1000);
    h = (secs / 3600) % 24;
    m = (secs / 60) % 60;
    s = secs % 60;

    snprintf(buffer, size, "%02u:%02u:%02u", h, m, s);

    return 0;
}

/**
 * login_format_date - Format a millisecond timestamp as YYYY-MM-DD
 */
int login_format_date(u64 time, char *buffer, u32 size)
{
    u32 secs;
    u32 days, era, doe, yoe, y, doy, mp, d, m;

    if (!buffer || size < 11) {
        return -EINVAL;
    }

    secs = (u32)(time / 1000);
    days = secs / 86400;
    days += 719468;
    era = days / 146097;
    doe = days - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y   = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp  = (5 * doy + 2) / 153;
    d   = doy - (153 * mp + 2) / 5 + 1;
    m   = (mp < 10) ? mp + 3 : mp - 9;
    if (m <= 2) {
        y++;
    }

    snprintf(buffer, size, "%04u-%02u-%02u", y, m, d);

    return 0;
}
