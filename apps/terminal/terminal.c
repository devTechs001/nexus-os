/*
 * NEXUS OS - Terminal Emulator Implementation
 * apps/terminal/terminal.c
 * 
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "terminal.h"

/*===========================================================================*/
/*                         GLOBAL TERMINAL MANAGER                           */
/*===========================================================================*/

static terminal_config_t g_terminal_config;
static terminal_t *g_terminal_list = NULL;
static terminal_t *g_active_terminal = NULL;
static int g_terminal_count = 0;
static bool g_initialized = false;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/**
 * terminal_init - Initialize terminal framework
 */
int terminal_init(terminal_config_t *config)
{
    if (!config) return -1;
    
    printk("[TERMINAL] Initializing terminal framework...\n");
    
    memcpy(&g_terminal_config, config, sizeof(terminal_config_t));
    
    g_terminal_list = NULL;
    g_active_terminal = NULL;
    g_terminal_count = 0;
    g_initialized = true;
    
    printk("[TERMINAL] Terminal framework initialized\n");
    return 0;
}

/**
 * terminal_shutdown - Shutdown terminal framework
 */
int terminal_shutdown(void)
{
    if (!g_initialized) return -1;
    
    printk("[TERMINAL] Shutting down terminal framework...\n");
    
    /* Close all terminals */
    terminal_t *term = g_terminal_list;
    while (term) {
        terminal_t *next = term->next;
        terminal_destroy(term);
        term = next;
    }
    
    g_terminal_list = NULL;
    g_active_terminal = NULL;
    g_terminal_count = 0;
    g_initialized = false;
    
    printk("[TERMINAL] Terminal framework shutdown complete\n");
    return 0;
}

/*===========================================================================*/
/*                         TERMINAL CREATE/DESTROY                           */
/*===========================================================================*/

/**
 * terminal_create - Create a new terminal
 */
terminal_t *terminal_create(int type)
{
    terminal_t *term = kmalloc(sizeof(terminal_t));
    if (!term) return NULL;
    
    memset(term, 0, sizeof(terminal_t));
    term->id = g_terminal_count++;
    term->type = type;
    term->running = true;
    term->connected = false;
    
    /* Default colors */
    term->fg_color = g_terminal_config.default_fg_color;
    term->bg_color = g_terminal_config.default_bg_color;
    
    /* Default size */
    term->width = 80;
    term->height = 24;
    
    /* History */
    term->history_max = g_terminal_config.history_max;
    term->history = kmalloc(sizeof(char*) * term->history_max);
    term->history_count = 0;
    term->history_index = -1;
    
    /* Buffer */
    term->buffer_size = 4096;
    term->buffer = kmalloc(term->buffer_size);
    
    /* Add to list */
    term->next = g_terminal_list;
    g_terminal_list = term;
    
    if (!g_active_terminal) {
        g_active_terminal = term;
    }
    
    printk("[TERMINAL] Created terminal %d (type=%d)\n", term->id, type);
    return term;
}

/**
 * terminal_destroy - Destroy a terminal
 */
int terminal_destroy(terminal_t *term)
{
    if (!term) return -1;
    
    printk("[TERMINAL] Destroying terminal %d\n", term->id);
    
    /* Remove from list */
    if (g_terminal_list == term) {
        g_terminal_list = term->next;
    } else {
        terminal_t *t = g_terminal_list;
        while (t && t->next != term) t = t->next;
        if (t) t->next = term->next;
    }
    
    if (g_active_terminal == term) {
        g_active_terminal = g_terminal_list;
    }
    
    /* Free resources */
    if (term->buffer) kfree(term->buffer);
    if (term->history) {
        for (int i = 0; i < term->history_count; i++) {
            if (term->history[i]) kfree(term->history[i]);
        }
        kfree(term->history);
    }
    
    term->running = false;
    kfree(term);
    
    g_terminal_count--;
    return 0;
}

/*===========================================================================*/
/*                         BASH TERMINAL                                     */
/*===========================================================================*/

/**
 * terminal_create_bash - Create bash terminal
 */
terminal_t *terminal_create_bash(void)
{
    terminal_t *term = terminal_create(TERMINAL_BASH);
    if (!term) return NULL;
    
    strcpy(term->name, "Bash");
    
    printk("[TERMINAL] Bash terminal created\n");
    return term;
}

/**
 * terminal_create_shell - Create custom shell terminal
 */
terminal_t *terminal_create_shell(const char *shell_path)
{
    terminal_t *term = terminal_create(TERMINAL_CUSTOM);
    if (!term) return NULL;
    
    if (shell_path) {
        strncpy(term->name, shell_path, sizeof(term->name) - 1);
    } else {
        strcpy(term->name, "Shell");
    }
    
    printk("[TERMINAL] Custom shell terminal created: %s\n", shell_path);
    return term;
}

/*===========================================================================*/
/*                         SSH TERMINAL                                      */
/*===========================================================================*/

/**
 * terminal_create_ssh - Create SSH terminal
 */
terminal_t *terminal_create_ssh(const char *host, const char *user, int port)
{
    terminal_t *term = terminal_create(TERMINAL_SSH);
    if (!term) return NULL;
    
    strcpy(term->name, "SSH");
    strncpy(term->ssh.host, host ? host : "", sizeof(term->ssh.host) - 1);
    strncpy(term->ssh.user, user ? user : "", sizeof(term->ssh.user) - 1);
    term->ssh.port = port ? port : 22;
    
    printk("[TERMINAL] SSH terminal created: %s@%s:%d\n", 
           term->ssh.user, term->ssh.host, term->ssh.port);
    
    return term;
}

/**
 * terminal_ssh_connect - Connect to SSH server
 */
int terminal_ssh_connect(terminal_t *term, const char *host, const char *user,
                         int port, const char *password)
{
    if (!term || !host || !user) return -1;
    
    printk("[SSH] Connecting to %s@%s:%d...\n", user, host, port);
    
    /* Store connection info */
    strncpy(term->ssh.host, host, sizeof(term->ssh.host) - 1);
    strncpy(term->ssh.user, user, sizeof(term->ssh.user) - 1);
    term->ssh.port = port;
    
    if (password) {
        term->ssh.password = kmalloc(strlen(password) + 1);
        if (term->ssh.password) {
            strcpy(term->ssh.password, password);
        }
    }
    
    /* In real implementation: establish SSH connection */
    /* - Create socket */
    /* - Perform SSH handshake */
    /* - Authenticate */
    /* - Start shell session */
    
    term->ssh.authenticated = true;
    term->connected = true;
    
    printk("[SSH] Connected to %s\n", host);
    return 0;
}

/**
 * terminal_ssh_connect_key - Connect to SSH with key authentication
 */
int terminal_ssh_connect_key(terminal_t *term, const char *host, const char *user,
                             int port, const char *key_path)
{
    if (!term || !host || !user || !key_path) return -1;
    
    printk("[SSH] Connecting with key: %s\n", key_path);
    
    strncpy(term->ssh.host, host, sizeof(term->ssh.host) - 1);
    strncpy(term->ssh.user, user, sizeof(term->ssh.user) - 1);
    term->ssh.port = port;
    
    term->ssh.key_path = kmalloc(strlen(key_path) + 1);
    if (term->ssh.key_path) {
        strcpy(term->ssh.key_path, key_path);
    }
    
    /* In real implementation: load key and authenticate */
    
    term->ssh.authenticated = true;
    term->connected = true;
    
    printk("[SSH] Connected to %s with key authentication\n", host);
    return 0;
}

/**
 * terminal_ssh_disconnect - Disconnect from SSH server
 */
int terminal_ssh_disconnect(terminal_t *term)
{
    if (!term) return -1;
    
    printk("[SSH] Disconnecting from %s\n", term->ssh.host);
    
    if (term->ssh.password) {
        kfree(term->ssh.password);
        term->ssh.password = NULL;
    }
    
    if (term->ssh.key_path) {
        kfree(term->ssh.key_path);
        term->ssh.key_path = NULL;
    }
    
    term->ssh.authenticated = false;
    term->connected = false;
    
    printk("[SSH] Disconnected\n");
    return 0;
}

/**
 * terminal_ssh_is_connected - Check SSH connection status
 */
bool terminal_ssh_is_connected(terminal_t *term)
{
    if (!term) return false;
    return term->ssh.authenticated && term->connected;
}

/*===========================================================================*/
/*                         SERIAL TERMINAL                                   */
/*===========================================================================*/

/**
 * terminal_create_serial - Create serial terminal
 */
terminal_t *terminal_create_serial(const char *device, int baud_rate)
{
    terminal_t *term = terminal_create(TERMINAL_SERIAL);
    if (!term) return NULL;
    
    strcpy(term->name, "Serial");
    
    if (device) {
        strncpy(term->serial.device, device, sizeof(term->serial.device) - 1);
    }
    
    term->serial.baud_rate = baud_rate ? baud_rate : 9600;
    term->serial.data_bits = 8;
    term->serial.stop_bits = 1;
    term->serial.parity = 'N';
    
    printk("[TERMINAL] Serial terminal created: %s @ %d baud\n", 
           term->serial.device, term->serial.baud_rate);
    
    return term;
}

/**
 * terminal_serial_configure - Configure serial port
 */
int terminal_serial_configure(terminal_t *term, int baud_rate, int data_bits,
                              int stop_bits, char parity)
{
    if (!term) return -1;
    
    term->serial.baud_rate = baud_rate;
    term->serial.data_bits = data_bits;
    term->serial.stop_bits = stop_bits;
    term->serial.parity = parity;
    
    printk("[SERIAL] Configured: %d baud, %d data, %d stop, %c parity\n",
           baud_rate, data_bits, stop_bits, parity);
    
    return 0;
}

/*===========================================================================*/
/*                         I/O OPERATIONS                                    */
/*===========================================================================*/

/**
 * terminal_write - Write to terminal
 */
int terminal_write(terminal_t *term, const char *data, size_t len)
{
    if (!term || !data) return -1;
    
    /* Write to terminal buffer */
    if (term->on_output) {
        term->on_output(term, data, len);
    }
    
    return len;
}

/**
 * terminal_writeln - Write line to terminal
 */
int terminal_writeln(terminal_t *term, const char *data)
{
    if (!term || !data) return -1;
    
    terminal_write(term, data, strlen(data));
    terminal_write(term, "\r\n", 2);
    
    return 0;
}

/**
 * terminal_read - Read from terminal
 */
int terminal_read(terminal_t *term, char *buffer, size_t len)
{
    if (!term || !buffer) return -1;
    
    /* In real implementation: read from PTY */
    return 0;
}

/*===========================================================================*/
/*                         INPUT HANDLING                                    */
/*===========================================================================*/

/**
 * terminal_handle_key - Handle keyboard input
 */
int terminal_handle_key(terminal_t *term, u32 key, u32 modifiers)
{
    if (!term) return -1;
    
    /* Handle special keys */
    switch (key) {
    case 0x48: /* Up arrow */
        /* History navigation */
        if (term->history_count > 0) {
            if (term->history_index < term->history_count - 1) {
                term->history_index++;
                /* Show previous command */
            }
        }
        break;
        
    case 0x50: /* Down arrow */
        if (term->history_index > 0) {
            term->history_index--;
            /* Show next command */
        }
        break;
        
    case 0x0E: /* Backspace */
        /* Delete previous character */
        break;
        
    case 0x01: /* Ctrl+A - Home */
        terminal_set_cursor(term, 0, term->cursor_y);
        break;
        
    case 0x05: /* Ctrl+E - End */
        terminal_set_cursor(term, term->width - 1, term->cursor_y);
        break;
        
    case 0x0C: /* Ctrl+L - Clear */
        terminal_clear(term);
        break;
        
    case 0x14: /* Ctrl+T - New tab */
        terminal_tab_create(term);
        break;
        
    case 0x17: /* Ctrl+W - Close tab */
        terminal_tab_close(term, term->active_tab);
        break;
        
    case 0x21: /* Ctrl+Shift+T - New tab (alternative) */
        if (modifiers & 0x03) {
            terminal_tab_create(term);
        }
        break;
    }
    
    return 0;
}

/**
 * terminal_handle_input - Handle text input
 */
int terminal_handle_input(terminal_t *term, const char *input, size_t len)
{
    if (!term || !input) return -1;
    
    /* Add to history if it's a command */
    if (input[len-1] == '\n') {
        char *cmd = kmalloc(len);
        if (cmd) {
            strncpy(cmd, input, len - 1);
            terminal_history_add(term, cmd);
            kfree(cmd);
        }
    }
    
    /* Send to shell/process */
    if (term->on_input) {
        term->on_input(term, input, len);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         HISTORY MANAGEMENT                                */
/*===========================================================================*/

/**
 * terminal_history_add - Add command to history
 */
int terminal_history_add(terminal_t *term, const char *command)
{
    if (!term || !command) return -1;
    
    if (term->history_count >= term->history_max) {
        /* Remove oldest */
        if (term->history[0]) kfree(term->history[0]);
        for (int i = 0; i < term->history_count - 1; i++) {
            term->history[i] = term->history[i + 1];
        }
        term->history_count--;
    }
    
    /* Add new command */
    term->history[term->history_count] = kmalloc(strlen(command) + 1);
    if (term->history[term->history_count]) {
        strcpy(term->history[term->history_count], command);
        term->history_count++;
    }
    
    term->history_index = -1;
    return 0;
}

/**
 * terminal_history_get - Get command from history
 */
const char *terminal_history_get(terminal_t *term, int index)
{
    if (!term || index < 0 || index >= term->history_count) {
        return NULL;
    }
    
    return term->history[index];
}

/**
 * terminal_history_clear - Clear command history
 */
int terminal_history_clear(terminal_t *term)
{
    if (!term) return -1;
    
    for (int i = 0; i < term->history_count; i++) {
        if (term->history[i]) kfree(term->history[i]);
    }
    
    term->history_count = 0;
    term->history_index = -1;
    
    return 0;
}

/**
 * terminal_history_save - Save history to file
 */
int terminal_history_save(terminal_t *term, const char *path)
{
    if (!term || !path) return -1;
    
    printk("[TERMINAL] Saving history to: %s\n", path);
    /* In real implementation: write to file */
    
    return 0;
}

/**
 * terminal_history_load - Load history from file
 */
int terminal_history_load(terminal_t *term, const char *path)
{
    if (!term || !path) return -1;
    
    printk("[TERMINAL] Loading history from: %s\n", path);
    /* In real implementation: read from file */
    
    return 0;
}

/*===========================================================================*/
/*                         TAB MANAGEMENT                                    */
/*===========================================================================*/

/**
 * terminal_tab_create - Create new tab
 */
int terminal_tab_create(terminal_t *term)
{
    if (!term) return -1;
    
    printk("[TERMINAL] Creating new tab\n");
    
    /* In real implementation: create new terminal and add to tabs array */
    
    return 0;
}

/**
 * terminal_tab_close - Close tab
 */
int terminal_tab_close(terminal_t *term, int index)
{
    if (!term) return -1;
    
    printk("[TERMINAL] Closing tab %d\n", index);
    
    /* In real implementation: close terminal and remove from tabs */
    
    return 0;
}

/**
 * terminal_tab_switch - Switch to tab
 */
int terminal_tab_switch(terminal_t *term, int index)
{
    if (!term) return -1;
    
    term->active_tab = index;
    printk("[TERMINAL] Switched to tab %d\n", index);
    
    return 0;
}

/**
 * terminal_tab_next - Switch to next tab
 */
int terminal_tab_next(terminal_t *term)
{
    if (!term || term->tab_count <= 1) return -1;
    
    term->active_tab = (term->active_tab + 1) % term->tab_count;
    terminal_tab_switch(term, term->active_tab);
    
    return 0;
}

/**
 * terminal_tab_prev - Switch to previous tab
 */
int terminal_tab_prev(terminal_t *term)
{
    if (!term || term->tab_count <= 1) return -1;
    
    term->active_tab = (term->active_tab - 1 + term->tab_count) % term->tab_count;
    terminal_tab_switch(term, term->active_tab);
    
    return 0;
}

/*===========================================================================*/
/*                         DISPLAY OPERATIONS                                */
/*===========================================================================*/

/**
 * terminal_resize - Resize terminal
 */
int terminal_resize(terminal_t *term, int width, int height)
{
    if (!term) return -1;
    
    term->width = width;
    term->height = height;
    
    printk("[TERMINAL] Resized to %dx%d\n", width, height);
    return 0;
}

/**
 * terminal_clear - Clear terminal screen
 */
int terminal_clear(terminal_t *term)
{
    if (!term) return -1;
    
    printk("[TERMINAL] Clear screen\n");
    
    /* In real implementation: clear buffer and redraw */
    
    return 0;
}

/**
 * terminal_set_colors - Set terminal colors
 */
int terminal_set_colors(terminal_t *term, u32 fg, u32 bg)
{
    if (!term) return -1;
    
    term->fg_color = fg;
    term->bg_color = bg;
    
    return 0;
}

/**
 * terminal_set_cursor - Set cursor position
 */
int terminal_set_cursor(terminal_t *term, int x, int y)
{
    if (!term) return -1;
    
    term->cursor_x = x;
    term->cursor_y = y;
    
    return 0;
}

/*===========================================================================*/
/*                         GLOBAL TERMINAL MANAGER                           */
/*===========================================================================*/

/**
 * terminal_get_active - Get active terminal
 */
terminal_t *terminal_get_active(void)
{
    return g_active_terminal;
}

/**
 * terminal_get_all - Get all terminals
 */
terminal_t *terminal_get_all(void)
{
    return g_terminal_list;
}

/**
 * terminal_get_count - Get terminal count
 */
int terminal_get_count(void)
{
    return g_terminal_count;
}

/**
 * terminal_focus - Focus a terminal
 */
int terminal_focus(terminal_t *term)
{
    if (!term) return -1;
    
    g_active_terminal = term;
    return 0;
}

/*===========================================================================*/
/*                         PREFERENCES                                       */
/*===========================================================================*/

/**
 * terminal_load_preferences - Load terminal preferences
 */
int terminal_load_preferences(terminal_config_t *config, const char *path)
{
    if (!config || !path) return -1;
    
    printk("[TERMINAL] Loading preferences from: %s\n", path);
    /* In real implementation: read from config file */
    
    return 0;
}

/**
 * terminal_save_preferences - Save terminal preferences
 */
int terminal_save_preferences(terminal_config_t *config, const char *path)
{
    if (!config || !path) return -1;
    
    printk("[TERMINAL] Saving preferences to: %s\n", path);
    /* In real implementation: write to config file */
    
    return 0;
}
