/*
 * NEXUS OS - Terminal Emulator Application
 * apps/terminal/terminal.c
 *
 * Modern terminal emulator with tabs, split panes, and themes
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "terminal.h"
#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../gui/window-manager/window-manager.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL TERMINAL APPLICATION                       */
/*===========================================================================*/

static terminal_app_t g_terminal_app;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* Window creation */
static int create_terminal_window(terminal_app_t *app, terminal_window_t *win, terminal_profile_t *profile);
static int create_tab_bar(terminal_window_t *win);
static int create_terminal_view(terminal_window_t *win);
static int create_scrollbar(terminal_window_t *win);
static int create_status_bar(terminal_window_t *win);

/* Tab operations */
static terminal_tab_t *create_tab(terminal_window_t *win, const char *shell);
static int destroy_tab(terminal_window_t *win, terminal_tab_t *tab);
static int update_tab_titles(terminal_window_t *win);

/* Rendering */
static int render_screen(terminal_window_t *win);
static int render_cell(terminal_window_t *win, u32 x, u32 y, terminal_cell_t *cell);
static int update_cursor(terminal_window_t *win);

/* PTY operations */
static int pty_spawn(terminal_window_t *win, const char *shell, const char *dir);
static int pty_read(terminal_window_t *win);
static int pty_write(terminal_window_t *win, const char *data, u32 length);

/* Event handlers */
static void on_key_press(terminal_window_t *win, u32 key, u32 modifiers);
static void on_mouse_click(terminal_window_t *win, s32 x, s32 y, u32 button);
static void on_resize(terminal_window_t *win, u32 width, u32 height);

/* Utilities */
static void init_default_themes(terminal_app_t *app);
static void init_default_profiles(terminal_app_t *app);
static void init_default_shortcuts(terminal_app_t *app);

/*===========================================================================*/
/*                         APPLICATION LIFECYCLE                             */
/*===========================================================================*/

/**
 * terminal_app_init - Initialize terminal application
 * @app: Pointer to terminal application structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int terminal_app_init(terminal_app_t *app)
{
    if (!app) {
        return -EINVAL;
    }
    
    printk("[TERMINAL] ========================================\n");
    printk("[TERMINAL] NEXUS OS Terminal v%s\n", TERMINAL_VERSION);
    printk("[TERMINAL] ========================================\n");
    printk("[TERMINAL] Initializing terminal application...\n");
    
    /* Clear structure */
    memset(app, 0, sizeof(terminal_app_t));
    app->initialized = true;
    app->window_count = 0;
    app->next_window_id = 1;
    
    /* Default settings */
    app->save_session = true;
    app->confirm_close = true;
    app->word_separators = 0x20202020; /* Space, tab, etc. */
    
    /* Initialize default themes */
    init_default_themes(app);
    
    /* Initialize default profiles */
    init_default_profiles(app);
    
    /* Initialize default shortcuts */
    init_default_shortcuts(app);
    
    printk("[TERMINAL] Terminal application initialized\n");
    printk("[TERMINAL] %d themes, %d profiles loaded\n", 
           app->theme_count, app->profile_count);
    printk("[TERMINAL] ========================================\n");
    
    return 0;
}

/**
 * terminal_app_run - Start terminal application
 * @app: Pointer to terminal application structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int terminal_app_run(terminal_app_t *app)
{
    if (!app || !app->initialized) {
        return -EINVAL;
    }
    
    printk("[TERMINAL] Starting terminal application...\n");
    
    app->running = true;
    
    /* Open default window */
    terminal_create_window(app, &app->default_profile);
    
    return 0;
}

/**
 * terminal_app_shutdown - Shutdown terminal application
 * @app: Pointer to terminal application structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int terminal_app_shutdown(terminal_app_t *app)
{
    if (!app || !app->initialized) {
        return -EINVAL;
    }
    
    printk("[TERMINAL] Shutting down terminal application...\n");
    
    app->running = false;
    
    /* Close all windows */
    while (app->windows) {
        terminal_close_window(app, app->windows);
    }
    
    app->initialized = false;
    
    printk("[TERMINAL] Terminal application shutdown complete\n");
    
    return 0;
}

/**
 * terminal_app_is_running - Check if terminal is running
 * @app: Pointer to terminal application structure
 *
 * Returns: true if running, false otherwise
 */
bool terminal_app_is_running(terminal_app_t *app)
{
    return app && app->running;
}

/*===========================================================================*/
/*                         WINDOW MANAGEMENT                                 */
/*===========================================================================*/

/**
 * terminal_create_window - Create new terminal window
 * @app: Pointer to terminal application structure
 * @profile: Profile to use
 *
 * Returns: Pointer to created window, or NULL on failure
 */
terminal_window_t *terminal_create_window(terminal_app_t *app, terminal_profile_t *profile)
{
    if (!app) {
        return NULL;
    }
    
    /* Allocate window */
    terminal_window_t *win = kzalloc(sizeof(terminal_window_t));
    if (!win) {
        return NULL;
    }
    
    /* Initialize window */
    win->window_id = app->next_window_id++;
    win->opacity = 100;
    win->show_menubar = true;
    win->show_scrollbar = true;
    win->show_status_bar = true;
    win->audible_bell = true;
    win->visual_bell = true;
    win->mouse_reporting = true;
    win->allow_resize = true;
    win->cursor_blink = true;
    win->scrollback_lines = 10000;
    
    /* Copy profile settings */
    if (profile) {
        memcpy(&win->current_profile, profile, sizeof(terminal_profile_t));
        memcpy(&win->current_theme, &profile->theme, sizeof(terminal_theme_t));
    }
    
    /* Create WM window */
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    snprintf(props.title, sizeof(props.title), "Terminal");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_RESIZABLE;
    props.bounds.x = 100 + (app->window_count * 30);
    props.bounds.y = 100 + (app->window_count * 30);
    props.bounds.width = 900;
    props.bounds.height = 600;
    props.background = color_from_rgba(30, 30, 40, 255);
    
    win->wm_window = wm_create_window(NULL, &props);
    if (!win->wm_window) {
        kfree(win);
        return NULL;
    }
    
    /* Create UI components */
    if (create_terminal_window(app, win, profile) != 0) {
        wm_destroy_window(NULL, win->wm_window);
        kfree(win);
        return NULL;
    }
    
    /* Add to window list */
    win->next = app->windows;
    app->windows = win;
    app->window_count++;
    
    /* Spawn shell */
    const char *shell = profile ? profile->shell_path : "/bin/bash";
    terminal_spawn_shell(win, shell, profile ? profile->working_dir : NULL);
    
    /* Call callback */
    if (app->on_window_created) {
        app->on_window_created(win);
    }
    
    printk("[TERMINAL] Created window %d: %s\n", win->window_id, shell);
    
    return win;
}

/**
 * create_terminal_window - Create terminal window UI
 * @app: Pointer to terminal application structure
 * @win: Window to create UI for
 * @profile: Profile to use
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_terminal_window(terminal_app_t *app, terminal_window_t *win, terminal_profile_t *profile)
{
    (void)app; (void)profile;
    
    if (!win || !win->wm_window) {
        return -EINVAL;
    }
    
    /* Create tab bar */
    create_tab_bar(win);
    
    /* Create terminal view */
    create_terminal_view(win);
    
    /* Create scrollbar */
    create_scrollbar(win);
    
    /* Create status bar */
    create_status_bar(win);
    
    /* Initialize screen buffer */
    win->screen = kzalloc(sizeof(terminal_screen_t));
    if (!win->screen) {
        return -ENOMEM;
    }
    
    win->screen->width = 120;  /* 120 columns */
    win->screen->height = 40;  /* 40 rows */
    win->screen->scrollback_size = win->scrollback_lines;
    win->screen->cursor_visible = true;
    win->screen->cursor_blink = true;
    win->screen->fg_color = 7;  /* White */
    win->screen->bg_color = 0;  /* Black */
    
    /* Allocate cell buffer */
    u32 cell_count = win->screen->width * win->screen->height;
    win->screen->cells = kzalloc(sizeof(terminal_cell_t) * cell_count);
    if (!win->screen->cells) {
        kfree(win->screen);
        return -ENOMEM;
    }
    
    /* Initialize cells */
    for (u32 i = 0; i < cell_count; i++) {
        win->screen->cells[i].character = ' ';
        win->screen->cells[i].fg_color = 7;
        win->screen->cells[i].bg_color = 0;
    }
    
    /* Calculate cell size */
    win->cell_width = 8;
    win->cell_height = 16;
    
    /* Set cursor position */
    win->screen->cursor_x = 0;
    win->screen->cursor_y = 0;
    
    return 0;
}

/**
 * create_tab_bar - Create tab bar
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
static int create_tab_bar(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create tab bar panel */
    win->tab_bar = panel_create(win->wm_window, 0, 0, 900, 32);
    if (!win->tab_bar) {
        return -1;
    }
    
    widget_set_colors(win->tab_bar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 50, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* In real implementation, would add:
     * - Tab buttons
     * - New tab button
     * - Close tab button
     */
    
    return 0;
}

/**
 * create_terminal_view - Create terminal view
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
static int create_terminal_view(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create terminal view panel */
    win->terminal_view = panel_create(win->wm_window, 0, 32, 900, 536);
    if (!win->terminal_view) {
        return -1;
    }
    
    widget_set_colors(win->terminal_view,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(30, 30, 40, 255),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_scrollbar - Create scrollbar
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
static int create_scrollbar(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create scrollbar */
    win->scrollbar = panel_create(win->wm_window, 890, 32, 10, 536);
    if (!win->scrollbar) {
        return -1;
    }
    
    widget_set_colors(win->scrollbar,
                      color_from_rgba(200, 200, 200, 255),
                      color_from_rgba(50, 50, 60, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    return 0;
}

/**
 * create_status_bar - Create status bar
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
static int create_status_bar(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create status bar panel */
    win->status_bar = panel_create(win->wm_window, 0, 568, 900, 32);
    if (!win->status_bar) {
        return -1;
    }
    
    widget_set_colors(win->status_bar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 50, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* Status labels would be added here */
    
    return 0;
}

/**
 * terminal_close_window - Close terminal window
 * @app: Pointer to terminal application structure
 * @win: Window to close
 *
 * Returns: 0 on success, negative error code on failure
 */
int terminal_close_window(terminal_app_t *app, terminal_window_t *win)
{
    if (!app || !win) {
        return -EINVAL;
    }
    
    printk("[TERMINAL] Closing window %d\n", win->window_id);
    
    /* Kill shell process */
    if (win->shell_pid > 0) {
        terminal_kill_process(win);
    }
    
    /* Remove from window list */
    terminal_window_t **prev = &app->windows;
    terminal_window_t *curr = app->windows;
    
    while (curr) {
        if (curr == win) {
            *prev = curr->next;
            app->window_count--;
            break;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    /* Free tabs */
    terminal_tab_t *tab = win->tabs;
    while (tab) {
        terminal_tab_t *next = tab->next;
        destroy_tab(win, tab);
        tab = next;
    }
    
    /* Free screen buffer */
    if (win->screen && win->screen->cells) {
        kfree(win->screen->cells);
    }
    if (win->screen) {
        kfree(win->screen);
    }
    
    /* Destroy WM window */
    if (win->wm_window) {
        wm_destroy_window(NULL, win->wm_window);
    }
    
    /* Free window */
    kfree(win);
    
    if (app->on_window_closed) {
        app->on_window_closed(win);
    }
    
    return 0;
}

/**
 * terminal_get_window - Get window by ID
 * @app: Pointer to terminal application structure
 * @window_id: Window ID
 *
 * Returns: Window pointer, or NULL if not found
 */
terminal_window_t *terminal_get_window(terminal_app_t *app, u32 window_id)
{
    if (!app) {
        return NULL;
    }
    
    terminal_window_t *win = app->windows;
    while (win) {
        if (win->window_id == window_id) {
            return win;
        }
        win = win->next;
    }
    
    return NULL;
}

/*===========================================================================*/
/*                         TAB MANAGEMENT                                    */
/*===========================================================================*/

/**
 * terminal_new_tab - Create new tab
 * @win: Terminal window
 * @shell: Shell path
 *
 * Returns: Pointer to created tab, or NULL on failure
 */
terminal_tab_t *terminal_new_tab(terminal_window_t *win, const char *shell)
{
    if (!win) {
        return NULL;
    }
    
    terminal_tab_t *tab = create_tab(win, shell);
    if (!tab) {
        return NULL;
    }
    
    /* Add to tab list */
    tab->next = win->tabs;
    win->tabs = tab;
    win->tab_count++;
    
    /* Switch to new tab */
    win->active_tab = tab->tab_id;
    
    /* Update tab bar */
    update_tab_titles(win);
    
    return tab;
}

/**
 * create_tab - Create tab internally
 * @win: Terminal window
 * @shell: Shell path
 *
 * Returns: Pointer to created tab
 */
static terminal_tab_t *create_tab(terminal_window_t *win, const char *shell)
{
    if (!win) {
        return NULL;
    }
    
    terminal_tab_t *tab = kzalloc(sizeof(terminal_tab_t));
    if (!tab) {
        return NULL;
    }
    
    tab->tab_id = win->next_tab_id++;
    strncpy(tab->title, shell ? shell : "bash", sizeof(tab->title) - 1);
    strncpy(tab->shell_path, shell ? shell : "/bin/bash", sizeof(tab->shell_path) - 1);
    tab->is_active = false;
    
    /* Create screen buffer for tab */
    tab->screen = kzalloc(sizeof(terminal_screen_t));
    if (!tab->screen) {
        kfree(tab);
        return NULL;
    }
    
    tab->screen->width = 120;
    tab->screen->height = 40;
    tab->screen->cells = kzalloc(sizeof(terminal_cell_t) * 120 * 40);
    
    return tab;
}

/**
 * destroy_tab - Destroy tab
 * @win: Terminal window
 * @tab: Tab to destroy
 *
 * Returns: 0 on success
 */
static int destroy_tab(terminal_window_t *win, terminal_tab_t *tab)
{
    (void)win;
    
    if (!tab) {
        return -EINVAL;
    }
    
    /* Kill shell process */
    if (tab->shell_pid > 0) {
        /* Would kill process */
    }
    
    /* Free screen buffer */
    if (tab->screen && tab->screen->cells) {
        kfree(tab->screen->cells);
    }
    if (tab->screen) {
        kfree(tab->screen);
    }
    
    kfree(tab);
    
    return 0;
}

/**
 * terminal_close_tab - Close tab
 * @win: Terminal window
 * @tab_id: Tab ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int terminal_close_tab(terminal_window_t *win, u32 tab_id)
{
    if (!win || win->tab_count <= 1) {
        return win->tab_count <= 1 ? -EPERM : -EINVAL;
    }
    
    /* Find and remove tab */
    terminal_tab_t **prev = &win->tabs;
    terminal_tab_t *curr = win->tabs;
    
    while (curr) {
        if (curr->tab_id == tab_id) {
            *prev = curr->next;
            win->tab_count--;
            
            /* If closing active tab, switch to another */
            if (win->active_tab == tab_id) {
                win->active_tab = (*prev) ? (*prev)->tab_id : win->tabs->tab_id;
            }
            
            destroy_tab(win, curr);
            update_tab_titles(win);
            return 0;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    return -ENOENT;
}

/**
 * terminal_switch_tab - Switch to tab
 * @win: Terminal window
 * @tab_id: Tab ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int terminal_switch_tab(terminal_window_t *win, u32 tab_id)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Verify tab exists */
    terminal_tab_t *tab = win->tabs;
    while (tab) {
        if (tab->tab_id == tab_id) {
            win->active_tab = tab_id;
            
            /* Switch screen buffer */
            win->screen = tab->screen;
            
            /* Update display */
            render_screen(win);
            update_tab_titles(win);
            
            return 0;
        }
        tab = tab->next;
    }
    
    return -ENOENT;
}

/**
 * terminal_rename_tab - Rename tab
 * @win: Terminal window
 * @tab_id: Tab ID
 * @title: New title
 *
 * Returns: 0 on success
 */
int terminal_rename_tab(terminal_window_t *win, u32 tab_id, const char *title)
{
    if (!win || !title) {
        return -EINVAL;
    }
    
    terminal_tab_t *tab = win->tabs;
    while (tab) {
        if (tab->tab_id == tab_id) {
            strncpy(tab->title, title, sizeof(tab->title) - 1);
            update_tab_titles(win);
            return 0;
        }
        tab = tab->next;
    }
    
    return -ENOENT;
}

/**
 * terminal_duplicate_tab - Duplicate tab
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_duplicate_tab(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Find active tab */
    terminal_tab_t *tab = win->tabs;
    while (tab) {
        if (tab->tab_id == win->active_tab) {
            terminal_new_tab(win, tab->shell_path);
            return 0;
        }
        tab = tab->next;
    }
    
    return -ENOENT;
}

/**
 * update_tab_titles - Update tab bar titles
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
static int update_tab_titles(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* In real implementation, would update tab bar widget */
    
    return 0;
}

/*===========================================================================*/
/*                         TERMINAL I/O                                      */
/*===========================================================================*/

/**
 * terminal_write - Write to terminal
 * @win: Terminal window
 * @data: Data to write
 * @length: Data length
 *
 * Returns: 0 on success, negative error code on failure
 */
int terminal_write(terminal_window_t *win, const char *data, u32 length)
{
    if (!win || !data || length == 0) {
        return -EINVAL;
    }
    
    return pty_write(win, data, length);
}

/**
 * terminal_read - Read from terminal
 * @win: Terminal window
 * @buffer: Buffer to read into
 * @length: Buffer length
 *
 * Returns: Bytes read, or negative error code on failure
 */
int terminal_read(terminal_window_t *win, char *buffer, u32 length)
{
    if (!win || !buffer || length == 0) {
        return -EINVAL;
    }
    
    return pty_read(win);
}

/**
 * terminal_send_key - Send key to terminal
 * @win: Terminal window
 * @key: Key code
 * @modifiers: Modifiers
 *
 * Returns: 0 on success
 */
int terminal_send_key(terminal_window_t *win, u32 key, u32 modifiers)
{
    if (!win) {
        return -EINVAL;
    }
    
    on_key_press(win, key, modifiers);
    
    return 0;
}

/**
 * terminal_send_string - Send string to terminal
 * @win: Terminal window
 * @str: String to send
 *
 * Returns: 0 on success
 */
int terminal_send_string(terminal_window_t *win, const char *str)
{
    if (!win || !str) {
        return -EINVAL;
    }
    
    return terminal_write(win, str, strlen(str));
}

/*===========================================================================*/
/*                         PTY OPERATIONS                                    */
/*===========================================================================*/

/**
 * pty_spawn - Spawn shell in PTY
 * @win: Terminal window
 * @shell: Shell path
 * @dir: Working directory
 *
 * Returns: 0 on success, negative error code on failure
 */
static int pty_spawn(terminal_window_t *win, const char *shell, const char *dir)
{
    if (!win || !shell) {
        return -EINVAL;
    }
    
    printk("[TERMINAL] Spawning shell: %s in %s\n", shell, dir ? dir : "/");
    
    /* In real implementation, would:
     * - Create PTY master/slave
     * - Fork process
     * - Execute shell
     * - Set up environment
     */
    
    /* Mock PID for now */
    win->shell_pid = 1000 + win->window_id;
    win->is_running = true;
    
    return 0;
}

/**
 * pty_read - Read from PTY
 * @win: Terminal window
 *
 * Returns: Bytes read, or negative error code
 */
static int pty_read(terminal_window_t *win)
{
    if (!win || !win->is_running) {
        return -1;
    }
    
    /* In real implementation, would read from PTY master FD */
    
    return 0;
}

/**
 * pty_write - Write to PTY
 * @win: Terminal window
 * @data: Data to write
 * @length: Data length
 *
 * Returns: 0 on success, negative error code on failure
 */
static int pty_write(terminal_window_t *win, const char *data, u32 length)
{
    if (!win || !data || length == 0) {
        return -EINVAL;
    }
    
    /* In real implementation, would write to PTY master FD */
    
    return 0;
}

/**
 * terminal_spawn_shell - Spawn shell
 * @win: Terminal window
 * @shell: Shell path
 * @dir: Working directory
 *
 * Returns: 0 on success
 */
int terminal_spawn_shell(terminal_window_t *win, const char *shell, const char *dir)
{
    return pty_spawn(win, shell, dir);
}

/**
 * terminal_kill_process - Kill shell process
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_kill_process(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    printk("[TERMINAL] Killing shell process %d\n", win->shell_pid);
    
    /* In real implementation, would send SIGTERM/SIGKILL */
    
    win->shell_pid = 0;
    win->is_running = false;
    
    return 0;
}

/**
 * terminal_get_process_info - Get process info
 * @win: Terminal window
 * @buffer: Buffer to store info
 * @size: Buffer size
 *
 * Returns: 0 on success
 */
int terminal_get_process_info(terminal_window_t *win, char *buffer, u32 size)
{
    if (!win || !buffer || size == 0) {
        return -EINVAL;
    }
    
    snprintf(buffer, size, "PID: %d, Running: %s", 
             win->shell_pid, win->is_running ? "yes" : "no");
    
    return 0;
}

/*===========================================================================*/
/*                         SCREEN OPERATIONS                                 */
/*===========================================================================*/

/**
 * terminal_clear_screen - Clear terminal screen
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_clear_screen(terminal_window_t *win)
{
    if (!win || !win->screen) {
        return -EINVAL;
    }
    
    /* Clear all cells */
    u32 cell_count = win->screen->width * win->screen->height;
    for (u32 i = 0; i < cell_count; i++) {
        win->screen->cells[i].character = ' ';
        win->screen->cells[i].fg_color = 7;
        win->screen->cells[i].bg_color = 0;
    }
    
    /* Reset cursor */
    win->screen->cursor_x = 0;
    win->screen->cursor_y = 0;
    
    render_screen(win);
    
    return 0;
}

/**
 * terminal_scroll_up - Scroll screen up
 * @win: Terminal window
 * @lines: Number of lines
 *
 * Returns: 0 on success
 */
int terminal_scroll_up(terminal_window_t *win, u32 lines)
{
    if (!win || !win->screen || lines == 0) {
        return -EINVAL;
    }
    
    /* In real implementation, would scroll screen buffer */
    
    return 0;
}

/**
 * terminal_scroll_down - Scroll screen down
 * @win: Terminal window
 * @lines: Number of lines
 *
 * Returns: 0 on success
 */
int terminal_scroll_down(terminal_window_t *win, u32 lines)
{
    if (!win || !win->screen || lines == 0) {
        return -EINVAL;
    }
    
    /* In real implementation, would scroll screen buffer */
    
    return 0;
}

/**
 * terminal_resize_screen - Resize terminal screen
 * @win: Terminal window
 * @width: New width (columns)
 * @height: New height (rows)
 *
 * Returns: 0 on success
 */
int terminal_resize_screen(terminal_window_t *win, u32 width, u32 height)
{
    if (!win || !win->screen || width == 0 || height == 0) {
        return -EINVAL;
    }
    
    /* In real implementation, would reallocate screen buffer */
    
    win->screen->width = width;
    win->screen->height = height;
    
    /* Notify shell of new size */
    /* Would send TIOCSWINSZ ioctl */
    
    return 0;
}

/**
 * render_screen - Render terminal screen
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
static int render_screen(terminal_window_t *win)
{
    if (!win || !win->screen) {
        return -EINVAL;
    }
    
    /* In real implementation, would render screen to widget */
    
    update_cursor(win);
    
    return 0;
}

/**
 * render_cell - Render single cell
 * @win: Terminal window
 * @x: X position
 * @y: Y position
 * @cell: Cell to render
 *
 * Returns: 0 on success
 */
static int render_cell(terminal_window_t *win, u32 x, u32 y, terminal_cell_t *cell)
{
    (void)win; (void)x; (void)y; (void)cell;
    
    /* In real implementation, would render character with colors */
    
    return 0;
}

/**
 * update_cursor - Update cursor position
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
static int update_cursor(terminal_window_t *win)
{
    if (!win || !win->screen) {
        return -EINVAL;
    }
    
    /* In real implementation, would update cursor widget */
    
    return 0;
}

/*===========================================================================*/
/*                         SELECTION & CLIPBOARD                             */
/*===========================================================================*/

/**
 * terminal_select_all - Select all text
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_select_all(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->is_selecting = true;
    win->selection_start_x = 0;
    win->selection_start_y = 0;
    win->selection_end_x = win->screen->width - 1;
    win->selection_end_y = win->screen->height - 1;
    
    return 0;
}

/**
 * terminal_copy_selection - Copy selection to clipboard
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_copy_selection(terminal_window_t *win)
{
    if (!win || !win->is_selecting) {
        return -EINVAL;
    }
    
    /* In real implementation, would copy selected text to clipboard */
    
    return 0;
}

/**
 * terminal_paste - Paste from clipboard
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_paste(terminal_window_t *win)
{
    if (!win || !win->clipboard) {
        return -EINVAL;
    }
    
    return terminal_send_string(win, win->clipboard);
}

/**
 * terminal_get_selection - Get selected text
 * @win: Terminal window
 *
 * Returns: Selected text, or NULL
 */
char *terminal_get_selection(terminal_window_t *win)
{
    if (!win || !win->is_selecting) {
        return NULL;
    }
    
    /* In real implementation, would extract selected text */
    
    return win->clipboard;
}

/*===========================================================================*/
/*                         FIND                                              */
/*===========================================================================*/

/**
 * terminal_find - Find text in terminal
 * @win: Terminal window
 * @query: Search query
 * @forward: Search forward
 *
 * Returns: Number of matches found
 */
int terminal_find(terminal_window_t *win, const char *query, bool forward)
{
    if (!win || !query) {
        return -EINVAL;
    }
    
    win->is_finding = true;
    strncpy(win->find_query, query, sizeof(win->find_query) - 1);
    
    /* In real implementation, would search screen buffer */
    
    win->find_matches = 0;
    
    return 0;
}

/**
 * terminal_clear_find - Clear find
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_clear_find(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->is_finding = false;
    win->find_query[0] = '\0';
    win->find_matches = 0;
    
    return 0;
}

/*===========================================================================*/
/*                         THEMES                                            */
/*===========================================================================*/

/**
 * init_default_themes - Initialize default themes
 * @app: Pointer to terminal application structure
 */
static void init_default_themes(terminal_app_t *app)
{
    if (!app) return;
    
    /* Dark theme (default) */
    terminal_theme_t *dark = &app->themes[app->theme_count++];
    strcpy(dark->name, "Dark");
    strcpy(dark->fg_color, "#FFFFFF");
    strcpy(dark->bg_color, "#1E1E2E");
    strcpy(dark->cursor_color, "#FFFFFF");
    strcpy(dark->colors[0], "#000000");  /* Black */
    strcpy(dark->colors[1], "#FF5555");  /* Red */
    strcpy(dark->colors[2], "#50FA7B");  /* Green */
    strcpy(dark->colors[3], "#F1FA8C");  /* Yellow */
    strcpy(dark->colors[4], "#BD93F9");  /* Blue */
    strcpy(dark->colors[5], "#FF79C6");  /* Magenta */
    strcpy(dark->colors[6], "#8BE9FD");  /* Cyan */
    strcpy(dark->colors[7], "#BBBBBB");  /* White */
    dark->font_size = 12;
    dark->bold_text = true;
    dark->cursor_blink = true;
    dark->scrollback_lines = 10000;
    
    /* Light theme */
    terminal_theme_t *light = &app->themes[app->theme_count++];
    strcpy(light->name, "Light");
    strcpy(light->fg_color, "#000000");
    strcpy(light->bg_color, "#FFFFFF");
    strcpy(light->cursor_color, "#000000");
    light->font_size = 12;
    light->cursor_blink = true;
    
    /* Solarized Dark */
    terminal_theme_t *solarized = &app->themes[app->theme_count++];
    strcpy(solarized->name, "Solarized Dark");
    strcpy(solarized->fg_color, "#839496");
    strcpy(solarized->bg_color, "#002B36");
    strcpy(solarized->cursor_color, "#839496");
    solarized->font_size = 12;
}

/**
 * terminal_load_theme - Load theme from file
 * @app: Pointer to terminal application structure
 * @path: Theme file path
 *
 * Returns: 0 on success
 */
int terminal_load_theme(terminal_app_t *app, const char *path)
{
    (void)app; (void)path;
    
    /* In real implementation, would load theme from JSON/XML file */
    
    return 0;
}

/**
 * terminal_set_theme - Set terminal theme
 * @win: Terminal window
 * @theme_name: Theme name
 *
 * Returns: 0 on success
 */
int terminal_set_theme(terminal_window_t *win, const char *theme_name)
{
    if (!win || !theme_name) {
        return -EINVAL;
    }
    
    terminal_app_t *app = &g_terminal_app;
    
    for (u32 i = 0; i < app->theme_count; i++) {
        if (strcmp(app->themes[i].name, theme_name) == 0) {
            memcpy(&win->current_theme, &app->themes[i], sizeof(terminal_theme_t));
            render_screen(win);
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * terminal_export_theme - Export theme to file
 * @app: Pointer to terminal application structure
 * @path: Output file path
 *
 * Returns: 0 on success
 */
int terminal_export_theme(terminal_app_t *app, const char *path)
{
    (void)app; (void)path;
    
    /* In real implementation, would save theme to JSON/XML file */
    
    return 0;
}

/*===========================================================================*/
/*                         PROFILES                                          */
/*===========================================================================*/

/**
 * init_default_profiles - Initialize default profiles
 * @app: Pointer to terminal application structure
 */
static void init_default_profiles(terminal_app_t *app)
{
    if (!app) return;
    
    /* Default profile */
    terminal_profile_t *default_profile = &app->profiles[app->profile_count++];
    strcpy(default_profile->name, "Default");
    strcpy(default_profile->shell_path, "/bin/bash");
    strcpy(default_profile->working_dir, "/home");
    memcpy(&default_profile->theme, &app->themes[0], sizeof(terminal_theme_t));
    default_profile->is_default = true;
    
    /* Copy to app default */
    memcpy(&app->default_profile, default_profile, sizeof(terminal_profile_t));
    
    /* Root profile */
    terminal_profile_t *root_profile = &app->profiles[app->profile_count++];
    strcpy(root_profile->name, "Root");
    strcpy(root_profile->shell_path, "/bin/bash");
    strcpy(root_profile->working_dir, "/root");
    memcpy(&root_profile->theme, &app->themes[0], sizeof(terminal_theme_t));
    
    /* Add environment variable */
    root_profile->environment = kzalloc(sizeof(char *));
    root_profile->environment[0] = "PS1=\\u@\\h:\\w# ";
    root_profile->env_count = 1;
}

/**
 * terminal_create_profile - Create profile
 * @app: Pointer to terminal application structure
 * @profile: Profile to create
 *
 * Returns: 0 on success
 */
int terminal_create_profile(terminal_app_t *app, terminal_profile_t *profile)
{
    if (!app || !profile) {
        return -EINVAL;
    }
    
    if (app->profile_count >= TERMINAL_MAX_PROFILES) {
        return -ENOSPC;
    }
    
    memcpy(&app->profiles[app->profile_count], profile, sizeof(terminal_profile_t));
    app->profile_count++;
    
    return 0;
}

/**
 * terminal_set_profile - Set terminal profile
 * @win: Terminal window
 * @profile: Profile to use
 *
 * Returns: 0 on success
 */
int terminal_set_profile(terminal_window_t *win, terminal_profile_t *profile)
{
    if (!win || !profile) {
        return -EINVAL;
    }
    
    memcpy(&win->current_profile, profile, sizeof(terminal_profile_t));
    memcpy(&win->current_theme, &profile->theme, sizeof(terminal_theme_t));
    
    return 0;
}

/**
 * terminal_delete_profile - Delete profile
 * @app: Pointer to terminal application structure
 * @name: Profile name
 *
 * Returns: 0 on success
 */
int terminal_delete_profile(terminal_app_t *app, const char *name)
{
    if (!app || !name) {
        return -EINVAL;
    }
    
    /* Find and remove profile */
    for (u32 i = 0; i < app->profile_count; i++) {
        if (strcmp(app->profiles[i].name, name) == 0) {
            if (app->profiles[i].is_default) {
                return -EPERM;  /* Cannot delete default profile */
            }
            
            /* Shift remaining profiles */
            for (u32 j = i; j < app->profile_count - 1; j++) {
                app->profiles[j] = app->profiles[j + 1];
            }
            app->profile_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         SETTINGS                                          */
/*===========================================================================*/

/**
 * terminal_set_opacity - Set window opacity
 * @win: Terminal window
 * @opacity: Opacity (0-100)
 *
 * Returns: 0 on success
 */
int terminal_set_opacity(terminal_window_t *win, u32 opacity)
{
    if (!win) {
        return -EINVAL;
    }
    
    if (opacity > 100) opacity = 100;
    win->opacity = opacity;
    
    /* In real implementation, would update window opacity */
    
    return 0;
}

/**
 * terminal_set_font - Set terminal font
 * @win: Terminal window
 * @font: Font name
 * @size: Font size
 *
 * Returns: 0 on success
 */
int terminal_set_font(terminal_window_t *win, const char *font, u32 size)
{
    if (!win || !font) {
        return -EINVAL;
    }
    
    strncpy(win->current_theme.font_name, font, sizeof(win->current_theme.font_name) - 1);
    win->current_theme.font_size = size;
    
    /* Recalculate cell size */
    win->cell_width = size / 2;
    win->cell_height = size;
    
    render_screen(win);
    
    return 0;
}

/**
 * terminal_toggle_menubar - Toggle menubar
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_toggle_menubar(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->show_menubar = !win->show_menubar;
    
    /* In real implementation, would show/hide menubar widget */
    
    return 0;
}

/**
 * terminal_toggle_toolbar - Toggle toolbar
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_toggle_toolbar(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->show_toolbar = !win->show_toolbar;
    
    return 0;
}

/**
 * terminal_toggle_fullscreen - Toggle fullscreen
 * @win: Terminal window
 *
 * Returns: 0 on success
 */
int terminal_toggle_fullscreen(terminal_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* In real implementation, would toggle window fullscreen */
    
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION HELPERS                            */
/*===========================================================================*/

/**
 * init_default_shortcuts - Initialize default shortcuts
 * @app: Pointer to terminal application structure
 */
static void init_default_shortcuts(terminal_app_t *app)
{
    if (!app) return;
    
    /* Ctrl+Shift+T - New tab */
    terminal_shortcut_t *shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "New Tab");
    shortcut->key = 0x54;  /* T */
    shortcut->modifiers = 0x03;  /* Ctrl+Shift */
    shortcut->callback = NULL;  /* Would set callback */
    
    /* Ctrl+Shift+W - Close tab */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Close Tab");
    shortcut->key = 0x57;  /* W */
    shortcut->modifiers = 0x03;
    
    /* Ctrl+Tab - Next tab */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Next Tab");
    shortcut->key = 0x09;  /* Tab */
    shortcut->modifiers = 0x01;  /* Ctrl */
    
    /* Ctrl+Shift+Tab - Previous tab */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Previous Tab");
    shortcut->key = 0x09;
    shortcut->modifiers = 0x03;  /* Ctrl+Shift */
    
    /* F11 - Fullscreen */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Fullscreen");
    shortcut->key = 0x7A;  /* F11 */
    shortcut->modifiers = 0;
    
    /* Ctrl+Shift+C - Copy */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Copy");
    shortcut->key = 0x43;  /* C */
    shortcut->modifiers = 0x03;
    
    /* Ctrl+Shift+V - Paste */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Paste");
    shortcut->key = 0x56;  /* V */
    shortcut->modifiers = 0x03;
    
    /* Ctrl+Shift+F - Find */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Find");
    shortcut->key = 0x46;  /* F */
    shortcut->modifiers = 0x03;
    
    /* Ctrl+Shift+A - Select All */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Select All");
    shortcut->key = 0x41;  /* A */
    shortcut->modifiers = 0x03;
    
    /* Ctrl+Shift+L - Clear */
    shortcut = &app->shortcuts[app->shortcut_count++];
    strcpy(shortcut->name, "Clear");
    shortcut->key = 0x4C;  /* L */
    shortcut->modifiers = 0x03;
}

/**
 * terminal_add_shortcut - Add shortcut
 * @app: Pointer to terminal application structure
 * @shortcut: Shortcut to add
 *
 * Returns: 0 on success
 */
int terminal_add_shortcut(terminal_app_t *app, terminal_shortcut_t *shortcut)
{
    if (!app || !shortcut) {
        return -EINVAL;
    }
    
    if (app->shortcut_count >= TERMINAL_MAX_SHORTCUTS) {
        return -ENOSPC;
    }
    
    memcpy(&app->shortcuts[app->shortcut_count], shortcut, sizeof(terminal_shortcut_t));
    app->shortcut_count++;
    
    return 0;
}

/**
 * terminal_remove_shortcut - Remove shortcut
 * @app: Pointer to terminal application structure
 * @name: Shortcut name
 *
 * Returns: 0 on success
 */
int terminal_remove_shortcut(terminal_app_t *app, const char *name)
{
    if (!app || !name) {
        return -EINVAL;
    }
    
    /* Find and remove shortcut */
    for (u32 i = 0; i < app->shortcut_count; i++) {
        if (strcmp(app->shortcuts[i].name, name) == 0) {
            /* Shift remaining shortcuts */
            for (u32 j = i; j < app->shortcut_count - 1; j++) {
                app->shortcuts[j] = app->shortcuts[j + 1];
            }
            app->shortcut_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

/**
 * on_key_press - Handle key press
 * @win: Terminal window
 * @key: Key code
 * @modifiers: Modifiers
 */
static void on_key_press(terminal_window_t *win, u32 key, u32 modifiers)
{
    if (!win) return;
    
    /* Check for shortcuts first */
    terminal_app_t *app = &g_terminal_app;
    for (u32 i = 0; i < app->shortcut_count; i++) {
        terminal_shortcut_t *shortcut = &app->shortcuts[i];
        if (shortcut->key == key && shortcut->modifiers == modifiers) {
            if (shortcut->callback) {
                shortcut->callback(shortcut->user_data);
            }
            return;
        }
    }
    
    /* Handle special keys */
    switch (key) {
        case 0x0D:  /* Enter */
            terminal_send_string(win, "\r");
            break;
        case 0x08:  /* Backspace */
            terminal_send_string(win, "\x7F");
            break;
        case 0x09:  /* Tab */
            terminal_send_string(win, "\t");
            break;
        case 0x1B:  /* Escape */
            terminal_send_string(win, "\x1B");
            break;
        default:
            /* Send character */
            if (key >= 0x20 && key <= 0x7E) {
                char c = (char)key;
                terminal_send_string(win, &c);
            }
            break;
    }
}

/**
 * on_mouse_click - Handle mouse click
 * @win: Terminal window
 * @x: X position
 * @y: Y position
 * @button: Mouse button
 */
static void on_mouse_click(terminal_window_t *win, s32 x, s32 y, u32 button)
{
    (void)win; (void)x; (void)y; (void)button;
    
    /* In real implementation, would handle mouse clicks */
    /* - Left click: Place cursor / select text */
    /* - Right click: Show context menu */
    /* - Middle click: Paste */
}

/**
 * on_resize - Handle window resize
 * @win: Terminal window
 * @width: New width
 * @height: New height
 */
static void on_resize(terminal_window_t *win, u32 width, u32 height)
{
    if (!win) return;
    
    /* Calculate new columns/rows */
    u32 cols = width / win->cell_width;
    u32 rows = height / win->cell_height;
    
    terminal_resize_screen(win, cols, rows);
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * terminal_get_version - Get terminal version
 *
 * Returns: Version string
 */
const char *terminal_get_version(void)
{
    return TERMINAL_VERSION;
}

/**
 * terminal_app_get_instance - Get global terminal application instance
 *
 * Returns: Pointer to global instance
 */
terminal_app_t *terminal_app_get_instance(void)
{
    return &g_terminal_app;
}
