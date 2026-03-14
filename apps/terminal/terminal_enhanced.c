/*
 * NEXUS OS - Enhanced Terminal with Sudo Support
 * apps/terminal/terminal_enhanced.c
 *
 * Advanced terminal with sudo operations, improved UI, and enhanced functionality
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "terminal.h"
#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../gui/window-manager/window-manager.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         SUDO CONFIGURATION                                */
/*===========================================================================*/

#define SUDO_PASSWORD_MAX       128
#define SUDO_TIMEOUT_SECONDS    300
#define SUDO_MAX_ATTEMPTS       3
#define SUDO_HISTORY_MAX        100
#define SUDOERS_MAX             256

/* Sudo States */
#define SUDO_STATE_NONE         0
#define SUDO_STATE_PROMPTED     1
#define SUDO_STATE_AUTHENTICATED 2
#define SUDO_STATE_EXPIRED      3
#define SUDO_STATE_LOCKED       4

/* Sudo Permissions */
#define SUDO_PERM_READ          (1 << 0)
#define SUDO_PERM_WRITE         (1 << 1)
#define SUDO_PERM_EXECUTE       (1 << 2)
#define SUDO_PERM_ADMIN         (1 << 3)
#define SUDO_PERM_ROOT          (1 << 4)
#define SUDO_PERM_ALL           0xFFFFFFFF

/* Command Categories */
#define CMD_CAT_GENERAL         1
#define CMD_CAT_FILE            2
#define CMD_CAT_NETWORK         3
#define CMD_CAT_SYSTEM          4
#define CMD_CAT_PACKAGE         5
#define CMD_CAT_USER            6
#define CMD_CAT_SERVICE         7
#define CMD_CAT_SECURITY        8

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/* Sudo Command Entry */
typedef struct {
    char command[64];
    u32 category;
    u32 required_permission;
    bool requires_password;
    bool allowed;
} sudo_command_t;

/* Sudo User Entry */
typedef struct {
    char username[64];
    char hashed_password[128];
    u32 permissions;
    u64 last_sudo_time;
    u32 failed_attempts;
    bool locked;
    bool password_never_expires;
    char **allowed_commands;
    u32 allowed_command_count;
} sudo_user_t;

/* Sudo Session */
typedef struct {
    char username[64];
    u64 start_time;
    u64 expiry_time;
    u32 permissions;
    bool active;
    char **command_history;
    u32 history_count;
} sudo_session_t;

/* Enhanced Terminal UI Settings */
typedef struct {
    /* Window Appearance */
    bool show_title_bar;
    bool show_tab_bar;
    bool show_scrollbar;
    bool show_status_bar;
    bool show_toolbar;
    bool transparent_bg;
    u32 transparency_percent;
    bool blur_background;
    bool rounded_corners;
    u32 corner_radius;
    
    /* Font Settings */
    char font_family[64];
    u32 font_size;
    u32 line_height;
    u32 letter_spacing;
    bool font_ligatures;
    bool font_antialias;
    bool bold_text;
    
    /* Cursor Settings */
    u32 cursor_style;  /* 0=Block, 1=Underline, 2=Vertical */
    bool cursor_blink;
    u32 cursor_blink_rate_ms;
    u32 cursor_color;
    
    /* Color Scheme */
    u32 fg_color;
    u32 bg_color;
    u32 cursor_fg;
    u32 cursor_bg;
    u32 selection_bg;
    u32 bold_color;
    u32 faint_color;
    u32 italic_color;
    u32 underline_color;
    u32 strike_color;
    
    /* ANSI Colors */
    u32 color_0;  /* Black */
    u32 color_1;  /* Red */
    u32 color_2;  /* Green */
    u32 color_3;  /* Yellow */
    u32 color_4;  /* Blue */
    u32 color_5;  /* Magenta */
    u32 color_6;  /* Cyan */
    u32 color_7;  /* White */
    u32 color_8;  /* Bright Black */
    u32 color_9;  /* Bright Red */
    u32 color_10; /* Bright Green */
    u32 color_11; /* Bright Yellow */
    u32 color_12; /* Bright Blue */
    u32 color_13; /* Bright Magenta */
    u32 color_14; /* Bright Cyan */
    u32 color_15; /* Bright White */
    
    /* Layout */
    u32 padding_x;
    u32 padding_y;
    u32 margin_x;
    u32 margin_y;
    u32 scrollbar_width;
    u32 tab_height;
    u32 status_bar_height;
    
    /* Advanced */
    bool smooth_scrolling;
    bool pixel_perfect;
    bool gpu_acceleration;
    bool vsync;
} terminal_ui_settings_t;

/* Command History */
typedef struct {
    char **commands;
    u32 count;
    u32 max_count;
    u32 current_index;
    bool searching;
    char search_query[64];
} command_history_t;

/* Auto-Completion */
typedef struct {
    bool enabled;
    bool case_sensitive;
    bool show_suggestions;
    u32 max_suggestions;
    char **suggestions;
    u32 suggestion_count;
    u32 selected_suggestion;
    bool auto_insert;
} autocomplete_settings_t;

/* Enhanced Terminal Session */
typedef struct {
    terminal_window_t base;
    sudo_session_t sudo_session;
    command_history_t history;
    autocomplete_settings_t autocomplete;
    terminal_ui_settings_t ui_settings;
    bool enhanced_mode;
    bool mouse_mode;
    bool bracketed_paste;
    bool focus_events;
    bool utf8_mode;
    char *working_directory;
    char **environment;
    u32 env_count;
    u32 exit_code;
    u64 session_start;
    u64 last_activity;
} enhanced_terminal_t;

/* Global Enhanced Terminal */
static enhanced_terminal_t g_enhanced_terminal;
static sudo_user_t g_sudo_users[SUDOERS_MAX];
static u32 g_sudo_user_count = 0;
static sudo_command_t g_sudo_commands[64];
static u32 g_sudo_command_count = 0;

/*===========================================================================*/
/*                         SUDO IMPLEMENTATION                               */
/*===========================================================================*/

/* Initialize Sudo System */
int sudo_init(void)
{
    printk("[TERMINAL-SUDO] ========================================\n");
    printk("[TERMINAL-SUDO] NEXUS OS Sudo System\n");
    printk("[TERMINAL-SUDO] ========================================\n");
    
    memset(g_sudo_users, 0, sizeof(g_sudo_users));
    memset(g_sudo_commands, 0, sizeof(g_sudo_commands));
    g_sudo_user_count = 0;
    g_sudo_command_count = 0;
    
    /* Create default admin user */
    sudo_user_t *admin = &g_sudo_users[g_sudo_user_count++];
    strncpy(admin->username, "admin", 63);
    strncpy(admin->hashed_password, "*LOCKED*", 127);  /* Password must be set */
    admin->permissions = SUDO_PERM_ALL;
    admin->locked = false;
    admin->password_never_expires = false;
    
    /* Define sudo commands */
    const char *admin_commands[] = {
        "reboot", "shutdown", "halt", "poweroff",
        "mount", "umount", "fdisk", "mkfs",
        "passwd", "useradd", "userdel", "usermod",
        "groupadd", "groupdel", "groupmod",
        "chmod", "chown", "chgrp",
        "iptables", "firewall", "network",
        "service", "systemctl", "init",
        "install", "remove", "update", "upgrade",
        "sudo", "su", "visudo",
        NULL
    };
    
    for (u32 i = 0; admin_commands[i] != NULL; i++) {
        if (g_sudo_command_count < 64) {
            sudo_command_t *cmd = &g_sudo_commands[g_sudo_command_count++];
            strncpy(cmd->command, admin_commands[i], 63);
            cmd->category = CMD_CAT_SYSTEM;
            cmd->required_permission = SUDO_PERM_ADMIN;
            cmd->requires_password = true;
            cmd->allowed = true;
        }
    }
    
    printk("[TERMINAL-SUDO] Sudo system initialized\n");
    printk("[TERMINAL-SUDO] Users: %u / %u\n", g_sudo_user_count, SUDOERS_MAX);
    printk("[TERMINAL-SUDO] Commands: %u defined\n", g_sudo_command_count);
    printk("[TERMINAL-SUDO] ========================================\n");
    
    return 0;
}

/* Hash Password (Simple implementation - use proper crypto in production) */
static void hash_password(const char *password, char *output, u32 output_size)
{
    u32 hash = 5381;
    const char *p = password;
    
    while (*p) {
        hash = ((hash << 5) + hash) + *p;
        p++;
    }
    
    snprintf(output, output_size, "%08X", hash);
}

/* Add Sudo User */
int sudo_add_user(const char *username, const char *password, u32 permissions)
{
    if (g_sudo_user_count >= SUDOERS_MAX) {
        printk("[TERMINAL-SUDO] ERROR: Maximum users reached\n");
        return -1;
    }
    
    if (!username || !password) {
        return -EINVAL;
    }
    
    sudo_user_t *user = &g_sudo_users[g_sudo_user_count++];
    memset(user, 0, sizeof(sudo_user_t));
    
    strncpy(user->username, username, 63);
    hash_password(password, user->hashed_password, 127);
    user->permissions = permissions;
    user->locked = false;
    
    printk("[TERMINAL-SUDO] Added user '%s' with permissions 0x%x\n",
           username, permissions);
    
    return 0;
}

/* Authenticate User */
int sudo_authenticate(const char *username, const char *password)
{
    if (!username || !password) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < g_sudo_user_count; i++) {
        sudo_user_t *user = &g_sudo_users[i];
        
        if (strcmp(user->username, username) == 0) {
            if (user->locked) {
                printk("[TERMINAL-SUDO] User '%s' is locked\n", username);
                return -EACCES;
            }
            
            char hashed_input[128];
            hash_password(password, hashed_input, 127);
            
            if (strcmp(user->hashed_password, hashed_input) == 0) {
                user->failed_attempts = 0;
                printk("[TERMINAL-SUDO] User '%s' authenticated\n", username);
                return 0;
            } else {
                user->failed_attempts++;
                if (user->failed_attempts >= SUDO_MAX_ATTEMPTS) {
                    user->locked = true;
                    printk("[TERMINAL-SUDO] User '%s' locked (max attempts)\n", username);
                }
                return -EACCES;
            }
        }
    }
    
    printk("[TERMINAL-SUDO] User '%s' not found\n", username);
    return -ENOENT;
}

/* Start Sudo Session */
int sudo_start_session(const char *username, const char *password, sudo_session_t *session)
{
    if (!session) {
        return -EINVAL;
    }
    
    int ret = sudo_authenticate(username, password);
    if (ret < 0) {
        return ret;
    }
    
    /* Find user */
    for (u32 i = 0; i < g_sudo_user_count; i++) {
        sudo_user_t *user = &g_sudo_users[i];
        if (strcmp(user->username, username) == 0) {
            memset(session, 0, sizeof(sudo_session_t));
            strncpy(session->username, username, 63);
            session->start_time = 0;  /* Would get from kernel */
            session->expiry_time = session->start_time + SUDO_TIMEOUT_SECONDS;
            session->permissions = user->permissions;
            session->active = true;
            session->history_count = 0;
            
            printk("[TERMINAL-SUDO] Session started for '%s' (expires in %us)\n",
                   username, SUDO_TIMEOUT_SECONDS);
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/* Check Command Permission */
int sudo_check_command(sudo_session_t *session, const char *command)
{
    if (!session || !session->active) {
        printk("[TERMINAL-SUDO] No active session\n");
        return -EACCES;
    }
    
    /* Check expiry */
    u64 current_time = 0;  /* Would get from kernel */
    if (current_time > session->expiry_time) {
        printk("[TERMINAL-SUDO] Session expired\n");
        session->active = false;
        return -ETIMEDOUT;
    }
    
    /* Check command */
    for (u32 i = 0; i < g_sudo_command_count; i++) {
        sudo_command_t *cmd = &g_sudo_commands[i];
        if (strcmp(cmd->command, command) == 0) {
            if (!cmd->allowed) {
                printk("[TERMINAL-SUDO] Command '%s' is not allowed\n", command);
                return -EACCES;
            }
            
            if (!(session->permissions & cmd->required_permission)) {
                printk("[TERMINAL-SUDO] Insufficient permissions for '%s'\n", command);
                return -EACCES;
            }
            
            /* Add to history */
            if (session->history_count < SUDO_HISTORY_MAX) {
                session->command_history[session->history_count++] = command;
            }
            
            printk("[TERMINAL-SUDO] Command '%s' authorized\n", command);
            return 0;
        }
    }
    
    printk("[TERMINAL-SUDO] Unknown command '%s'\n", command);
    return -ENOENT;
}

/* End Sudo Session */
int sudo_end_session(sudo_session_t *session)
{
    if (!session) {
        return -EINVAL;
    }
    
    session->active = false;
    printk("[TERMINAL-SUDO] Session ended for '%s'\n", session->username);
    
    return 0;
}

/* Print Sudo Status */
void sudo_print_status(sudo_session_t *session)
{
    if (!session) {
        return;
    }
    
    printk("\n[TERMINAL-SUDO] ====== STATUS ======\n");
    printk("[TERMINAL-SUDO] User: %s\n", session->username);
    printk("[TERMINAL-SUDO] Active: %s\n", session->active ? "Yes" : "No");
    printk("[TERMINAL-SUDO] Permissions: 0x%x\n", session->permissions);
    printk("[TERMINAL-SUDO] Commands: %u\n", session->history_count);
    printk("[TERMINAL-SUDO] ========================\n\n");
}

/*===========================================================================*/
/*                         ENHANCED UI IMPLEMENTATION                        */
/*===========================================================================*/

/* Initialize Enhanced Terminal UI */
int enhanced_terminal_ui_init(enhanced_terminal_t *term)
{
    if (!term) {
        return -EINVAL;
    }
    
    terminal_ui_settings_t *ui = &term->ui_settings;
    
    /* Default UI Settings - Modern Theme */
    ui->show_title_bar = true;
    ui->show_tab_bar = true;
    ui->show_scrollbar = true;
    ui->show_status_bar = true;
    ui->show_toolbar = true;
    ui->transparent_bg = false;
    ui->transparency_percent = 90;
    ui->blur_background = true;
    ui->rounded_corners = true;
    ui->corner_radius = 8;
    
    /* Font Settings */
    strncpy(ui->font_family, "Consolas", 63);
    ui->font_size = 14;
    ui->line_height = 20;
    ui->letter_spacing = 0;
    ui->font_ligatures = true;
    ui->font_antialias = true;
    ui->bold_text = true;
    
    /* Cursor Settings */
    ui->cursor_style = 0;  /* Block */
    ui->cursor_blink = true;
    ui->cursor_blink_rate_ms = 500;
    ui->cursor_color = 0xFFFFFF;
    
    /* Color Scheme - Modern Dark */
    ui->fg_color = 0xF8F8F2;
    ui->bg_color = 0x282A36;
    ui->cursor_fg = 0x282A36;
    ui->cursor_bg = 0xF8F8F2;
    ui->selection_bg = 0x44475A;
    ui->bold_color = 0xF8F8F2;
    ui->faint_color = 0x6272A4;
    ui->italic_color = 0x8BE9FD;
    ui->underline_color = 0xFFB86C;
    ui->strike_color = 0xFF5555;
    
    /* ANSI Colors - Dracula Theme */
    ui->color_0 = 0x21222C;   /* Black */
    ui->color_1 = 0xFF5555;   /* Red */
    ui->color_2 = 0x50FA7B;   /* Green */
    ui->color_3 = 0xF1FA8C;   /* Yellow */
    ui->color_4 = 0xBD93F9;   /* Blue */
    ui->color_5 = 0xFF79C6;   /* Magenta */
    ui->color_6 = 0x8BE9FD;   /* Cyan */
    ui->color_7 = 0xF8F8F2;   /* White */
    ui->color_8 = 0x6272A4;   /* Bright Black */
    ui->color_9 = 0xFF6E6E;   /* Bright Red */
    ui->color_10 = 0x69FF94;  /* Bright Green */
    ui->color_11 = 0xFFFFA5;  /* Bright Yellow */
    ui->color_12 = 0xD6ACFF;  /* Bright Blue */
    ui->color_13 = 0xFF92DF;  /* Bright Magenta */
    ui->color_14 = 0xA4FFFF;  /* Bright Cyan */
    ui->color_15 = 0xFFFFFF;  /* Bright White */
    
    /* Layout */
    ui->padding_x = 10;
    ui->padding_y = 10;
    ui->margin_x = 0;
    ui->margin_y = 0;
    ui->scrollbar_width = 12;
    ui->tab_height = 32;
    ui->status_bar_height = 24;
    
    /* Advanced */
    ui->smooth_scrolling = true;
    ui->pixel_perfect = true;
    ui->gpu_acceleration = true;
    ui->vsync = true;
    
    printk("[TERMINAL-UI] Enhanced UI initialized\n");
    printk("[TERMINAL-UI] Theme: Modern Dark (Dracula)\n");
    printk("[TERMINAL-UI] Font: %s %upx\n", ui->font_family, ui->font_size);
    printk("[TERMINAL-UI] GPU Acceleration: %s\n", ui->gpu_acceleration ? "Enabled" : "Disabled");
    
    return 0;
}

/* Initialize Command History */
int command_history_init(command_history_t *history, u32 max_count)
{
    if (!history) {
        return -EINVAL;
    }
    
    history->commands = (char **)kmalloc(sizeof(char *) * max_count);
    if (!history->commands) {
        return -ENOMEM;
    }
    
    memset(history->commands, 0, sizeof(char *) * max_count);
    history->count = 0;
    history->max_count = max_count;
    history->current_index = 0;
    history->searching = false;
    
    printk("[TERMINAL-HISTORY] Initialized (max: %u)\n", max_count);
    
    return 0;
}

/* Add Command to History */
int command_history_add(command_history_t *history, const char *command)
{
    if (!history || !command) {
        return -EINVAL;
    }
    
    /* Don't add duplicates */
    if (history->count > 0) {
        if (strcmp(history->commands[history->count - 1], command) == 0) {
            return 0;
        }
    }
    
    /* Add command */
    if (history->count < history->max_count) {
        history->commands[history->count] = (char *)kmalloc(strlen(command) + 1);
        if (history->commands[history->count]) {
            strcpy(history->commands[history->count], command);
            history->count++;
        }
    } else {
        /* Shift and add */
        kfree(history->commands[0]);
        for (u32 i = 0; i < history->max_count - 1; i++) {
            history->commands[i] = history->commands[i + 1];
        }
        history->commands[history->max_count - 1] = (char *)kmalloc(strlen(command) + 1);
        if (history->commands[history->max_count - 1]) {
            strcpy(history->commands[history->max_count - 1], command);
        }
    }
    
    history->current_index = history->count;
    
    return 0;
}

/* Get Previous Command */
const char *command_history_prev(command_history_t *history)
{
    if (!history || history->count == 0) {
        return NULL;
    }
    
    if (history->current_index > 0) {
        history->current_index--;
        return history->commands[history->current_index];
    }
    
    return history->commands[0];
}

/* Get Next Command */
const char *command_history_next(command_history_t *history)
{
    if (!history || history->count == 0) {
        return NULL;
    }
    
    if (history->current_index < history->count - 1) {
        history->current_index++;
        return history->commands[history->current_index];
    }
    
    history->current_index = history->count;
    return "";
}

/* Initialize Enhanced Terminal */
int enhanced_terminal_init(enhanced_terminal_t *term)
{
    if (!term) {
        return -EINVAL;
    }
    
    printk("[TERMINAL] ========================================\n");
    printk("[TERMINAL] NEXUS OS Enhanced Terminal\n");
    printk("[TERMINAL] With Sudo Support & Modern UI\n");
    printk("[TERMINAL] ========================================\n");
    
    memset(term, 0, sizeof(enhanced_terminal_t));
    term->enhanced_mode = true;
    
    /* Initialize sudo session */
    term->sudo_session.active = false;
    
    /* Initialize command history */
    command_history_init(&term->history, 1000);
    
    /* Initialize autocomplete */
    term->autocomplete.enabled = true;
    term->autocomplete.case_sensitive = false;
    term->autocomplete.show_suggestions = true;
    term->autocomplete.max_suggestions = 10;
    term->autocomplete.auto_insert = false;
    
    /* Initialize UI */
    enhanced_terminal_ui_init(term);
    
    /* Initialize base terminal */
    /* terminal_window_init(&term->base); */
    
    /* Initialize sudo system */
    sudo_init();
    
    /* Create default admin user */
    sudo_add_user("admin", "admin123", SUDO_PERM_ALL);
    
    term->session_start = 0;  /* Would get from kernel */
    term->last_activity = term->session_start;
    
    printk("[TERMINAL] Enhanced terminal initialized\n");
    printk("[TERMINAL] Sudo: Enabled\n");
    printk("[TERMINAL] History: %u commands\n", term->history.max_count);
    printk("[TERMINAL] Autocomplete: %s\n", term->autocomplete.enabled ? "Enabled" : "Disabled");
    printk("[TERMINAL] ========================================\n");
    
    return 0;
}

/* Process Sudo Command */
int enhanced_terminal_process_sudo(enhanced_terminal_t *term, const char *cmdline)
{
    if (!term || !cmdline) {
        return -EINVAL;
    }
    
    /* Check if command starts with sudo */
    if (strncmp(cmdline, "sudo ", 5) != 0) {
        return 0;  /* Not a sudo command */
    }
    
    const char *actual_cmd = cmdline + 5;
    
    /* Extract command name */
    char cmd_name[64];
    const char *space = strchr(actual_cmd, ' ');
    if (space) {
        strncpy(cmd_name, actual_cmd, space - actual_cmd);
    } else {
        strncpy(cmd_name, actual_cmd, 63);
    }
    
    /* Check if session is active */
    if (!term->sudo_session.active) {
        printk("[TERMINAL] Sudo authentication required\n");
        printk("[TERMINAL] Password: ");
        /* Would prompt for password */
        return 1;  /* Needs authentication */
    }
    
    /* Check command permission */
    int ret = sudo_check_command(&term->sudo_session, cmd_name);
    if (ret < 0) {
        printk("[TERMINAL] Permission denied\n");
        return ret;
    }
    
    printk("[TERMINAL] Executing: %s (as root)\n", actual_cmd);
    
    /* Execute command with elevated privileges */
    /* Would actually execute the command */
    
    return 0;
}

/* Handle Key Press */
void enhanced_terminal_on_key(enhanced_terminal_t *term, u32 key, u32 modifiers)
{
    if (!term) {
        return;
    }
    
    term->last_activity = 0;  /* Would get from kernel */
    
    /* Ctrl+Shift+T - New Tab */
    if (key == 'T' && (modifiers & 0x04)) {
        printk("[TERMINAL] New tab (Ctrl+Shift+T)\n");
        return;
    }
    
    /* Ctrl+Shift+N - New Window */
    if (key == 'N' && (modifiers & 0x0C)) {
        printk("[TERMINAL] New window (Ctrl+Shift+N)\n");
        return;
    }
    
    /* Ctrl+Shift+W - Close Tab */
    if (key == 'W' && (modifiers & 0x0C)) {
        printk("[TERMINAL] Close tab (Ctrl+Shift+W)\n");
        return;
    }
    
    /* Ctrl+Shift+C - Copy */
    if (key == 'C' && (modifiers & 0x0C)) {
        printk("[TERMINAL] Copy (Ctrl+Shift+C)\n");
        return;
    }
    
    /* Ctrl+Shift+V - Paste */
    if (key == 'V' && (modifiers & 0x0C)) {
        printk("[TERMINAL] Paste (Ctrl+Shift+V)\n");
        return;
    }
    
    /* Ctrl+Shift+Plus - Zoom In */
    if (key == '=' && (modifiers & 0x0C)) {
        term->ui_settings.font_size++;
        printk("[TERMINAL] Zoom in: %upx\n", term->ui_settings.font_size);
        return;
    }
    
    /* Ctrl+Shift+Minus - Zoom Out */
    if (key == '-' && (modifiers & 0x0C)) {
        if (term->ui_settings.font_size > 8) {
            term->ui_settings.font_size--;
            printk("[TERMINAL] Zoom out: %upx\n", term->ui_settings.font_size);
        }
        return;
    }
    
    /* Ctrl+Shift+0 - Reset Zoom */
    if (key == '0' && (modifiers & 0x0C)) {
        term->ui_settings.font_size = 14;
        printk("[TERMINAL] Zoom reset\n");
        return;
    }
    
    /* Up Arrow - Previous Command */
    if (key == TERM_KEY_UP) {
        const char *prev = command_history_prev(&term->history);
        if (prev) {
            printk("[TERMINAL] Previous: %s\n", prev);
        }
        return;
    }
    
    /* Down Arrow - Next Command */
    if (key == TERM_KEY_DOWN) {
        const char *next = command_history_next(&term->history);
        if (next) {
            printk("[TERMINAL] Next: %s\n", next);
        }
        return;
    }
    
    /* Ctrl+R - Reverse Search */
    if (key == 'R' && (modifiers & 0x04)) {
        term->history.searching = true;
        printk("[TERMINAL] Reverse search: ");
        return;
    }
    
    /* Ctrl+L - Clear Screen */
    if (key == 'L' && (modifiers & 0x04)) {
        printk("[TERMINAL] Clear screen (Ctrl+L)\n");
        return;
    }
    
    /* Ctrl+D - Exit */
    if (key == 'D' && (modifiers & 0x04)) {
        printk("[TERMINAL] Exit (Ctrl+D)\n");
        return;
    }
}

/* Print Enhanced Terminal Status */
void enhanced_terminal_print_status(enhanced_terminal_t *term)
{
    if (!term) {
        return;
    }
    
    printk("\n[TERMINAL] ====== STATUS ======\n");
    printk("[TERMINAL] Enhanced Mode: %s\n", term->enhanced_mode ? "Yes" : "No");
    printk("[TERMINAL] Sudo Active: %s\n", term->sudo_session.active ? "Yes" : "No");
    printk("[TERMINAL] History: %u commands\n", term->history.count);
    printk("[TERMINAL] Autocomplete: %s\n", term->autocomplete.enabled ? "Enabled" : "Disabled");
    printk("[TERMINAL] Font: %s %upx\n", term->ui_settings.font_family, term->ui_settings.font_size);
    printk("[TERMINAL] Theme: Modern Dark\n");
    printk("[TERMINAL] GPU: %s\n", term->ui_settings.gpu_acceleration ? "Enabled" : "Disabled");
    printk("[TERMINAL] ========================\n\n");
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int terminal_enhanced_init(void)
{
    return enhanced_terminal_init(&g_enhanced_terminal);
}

void terminal_enhanced_run(void)
{
    printk("[TERMINAL] Enhanced terminal running...\n");
    enhanced_terminal_print_status(&g_enhanced_terminal);
}
