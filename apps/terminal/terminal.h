/*
 * NEXUS OS - Terminal Emulator Framework
 * apps/terminal/terminal.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Provides multiple terminal types:
 * - Bash terminal
 * - SSH terminal
 * - Serial terminal
 * - PowerShell-compatible
 */

#ifndef _NEXUS_TERMINAL_H
#define _NEXUS_TERMINAL_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         TERMINAL TYPES                                    */
/*===========================================================================*/

#define TERMINAL_BASH           0
#define TERMINAL_SH             1
#define TERMINAL_SSH            2
#define TERMINAL_SERIAL         3
#define TERMINAL_POWERSHELL     4
#define TERMINAL_CUSTOM         5

/*===========================================================================*/
/*                         TERMINAL STRUCTURES                               */
/*===========================================================================*/

/**
 * terminal_t - Terminal instance
 */
typedef struct terminal {
    char name[64];
    int type;
    int id;
    
    /* I/O */
    int fd_in;
    int fd_out;
    int fd_err;
    
    /* Buffer */
    char *buffer;
    size_t buffer_size;
    size_t cursor_x;
    size_t cursor_y;
    size_t scroll_offset;
    
    /* Display */
    int width;
    int height;
    u32 fg_color;
    u32 bg_color;
    
    /* State */
    bool running;
    bool connected;
    bool login;
    
    /* History */
    char **history;
    int history_count;
    int history_index;
    int history_max;
    
    /* Tabs */
    struct terminal **tabs;
    int tab_count;
    int active_tab;
    
    /* SSH specific */
    struct {
        char host[256];
        char user[64];
        int port;
        char *password;
        char *key_path;
        bool authenticated;
    } ssh;
    
    /* Serial specific */
    struct {
        char device[64];
        int baud_rate;
        int data_bits;
        int stop_bits;
        char parity;
    } serial;
    
    /* Callbacks */
    void (*on_output)(struct terminal *, const char *, size_t);
    void (*on_input)(struct terminal *, const char *, size_t);
    void (*on_exit)(struct terminal *, int);
    
    /* Private data */
    void *priv;
    
    /* Link */
    struct terminal *next;
} terminal_t;

/**
 * terminal_config_t - Terminal configuration
 */
typedef struct {
    char shell[64];
    char *shell_args[16];
    int shell_arg_count;
    
    /* Display */
    u32 default_fg_color;
    u32 default_bg_color;
    char font_name[64];
    int font_size;
    
    /* Behavior */
    int history_max;
    bool enable_tabs;
    bool enable_scrollback;
    int scrollback_lines;
    bool bell_enabled;
    bool cursor_blink;
    
    /* SSH defaults */
    char ssh_user[64];
    int ssh_port;
    
    /* Security */
    bool allow_root;
    bool log_commands;
    char *allowed_commands[64];
    int allowed_command_count;
} terminal_config_t;

/*===========================================================================*/
/*                         TERMINAL API                                      */
/*===========================================================================*/

/* Initialization */
int terminal_init(terminal_config_t *config);
int terminal_shutdown(void);

/* Create/Destroy */
terminal_t *terminal_create(int type);
int terminal_destroy(terminal_t *term);

/* Bash terminal */
terminal_t *terminal_create_bash(void);
terminal_t *terminal_create_shell(const char *shell_path);

/* SSH terminal */
terminal_t *terminal_create_ssh(const char *host, const char *user, int port);
int terminal_ssh_connect(terminal_t *term, const char *host, const char *user, 
                         int port, const char *password);
int terminal_ssh_connect_key(terminal_t *term, const char *host, const char *user,
                             int port, const char *key_path);
int terminal_ssh_disconnect(terminal_t *term);
bool terminal_ssh_is_connected(terminal_t *term);

/* Serial terminal */
terminal_t *terminal_create_serial(const char *device, int baud_rate);
int terminal_serial_configure(terminal_t *term, int baud_rate, int data_bits,
                              int stop_bits, char parity);

/* I/O Operations */
int terminal_write(terminal_t *term, const char *data, size_t len);
int terminal_read(terminal_t *term, char *buffer, size_t len);
int terminal_writeln(terminal_t *term, const char *data);

/* Input handling */
int terminal_handle_key(terminal_t *term, u32 key, u32 modifiers);
int terminal_handle_input(terminal_t *term, const char *input, size_t len);

/* Display */
int terminal_resize(terminal_t *term, int width, int height);
int terminal_clear(terminal_t *term);
int terminal_scroll(terminal_t *term, int lines);
int terminal_set_colors(terminal_t *term, u32 fg, u32 bg);
int terminal_set_cursor(terminal_t *term, int x, int y);
int terminal_hide_cursor(terminal_t *term);
int terminal_show_cursor(terminal_t *term);

/* History */
int terminal_history_add(terminal_t *term, const char *command);
const char *terminal_history_get(terminal_t *term, int index);
int terminal_history_clear(terminal_t *term);
int terminal_history_save(terminal_t *term, const char *path);
int terminal_history_load(terminal_t *term, const char *path);

/* Tabs */
int terminal_tab_create(terminal_t *term);
int terminal_tab_close(terminal_t *term, int index);
int terminal_tab_switch(terminal_t *term, int index);
int terminal_tab_next(terminal_t *term);
int terminal_tab_prev(terminal_t *term);

/* Process management */
int terminal_get_pid(terminal_t *term);
int terminal_send_signal(terminal_t *term, int signal);
int terminal_wait(terminal_t *term);

/* Session management */
int terminal_session_save(terminal_t *term, const char *path);
int terminal_session_restore(terminal_t *term, const char *path);

/* Utilities */
int terminal_copy_selection(terminal_t *term);
int terminal_paste(terminal_t *term);
int terminal_select_all(terminal_t *term);
int terminal_find(terminal_t *term, const char *pattern);
int terminal_zoom_in(terminal_t *term);
int terminal_zoom_out(terminal_t *term);
int terminal_zoom_reset(terminal_t *term);

/* Preferences */
int terminal_load_preferences(terminal_config_t *config, const char *path);
int terminal_save_preferences(terminal_config_t *config, const char *path);

/* Global terminal manager */
terminal_t *terminal_get_active(void);
terminal_t *terminal_get_all(void);
int terminal_get_count(void);
int terminal_focus(terminal_t *term);

#endif /* _NEXUS_TERMINAL_H */
