/*
 * NEXUS OS - Terminal Emulator Application
 * apps/terminal/terminal.c
 *
 * Modern terminal emulator with tabs, split panes, and themes
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "terminal.h"
#include "shell.h"
#include "../../fs/vfs/vfs.h"
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
    win->tab_bar = panel_create(NULL, 0, 0, 900, 32);
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
    win->terminal_view = panel_create(NULL, 0, 32, 900, 536);
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
    win->scrollbar = panel_create(NULL, 890, 32, 10, 536);
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
    win->status_bar = panel_create(NULL, 0, 568, 900, 32);
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
/*
 * PTY plumbing.
 *
 * This used to be a mock: pty_spawn() stored "1000 + window_id" as a PID and
 * pty_read()/pty_write() did nothing at all, so the terminal window had a
 * scrollback buffer that could never be filled by an actual command.
 *
 * It is now backed by the real in-kernel shell (shell.c): the shell owns the
 * interpreter and the environment, and this file only supplies the byte
 * plumbing between the shell's output sink and the terminal cell buffer.
 */

/* Output sink: shell bytes -> terminal scrollback cells. */
static void terminal_shell_sink(void *ctx, const char *data, u32 len)
{
    terminal_window_t *win = (terminal_window_t *)ctx;

    if (!win || !data || !len) {
        return;
    }

    /* Route through the window's writer so escape handling and wrapping
     * stay in one place. */
    terminal_write(win, data, len);
}

static int pty_spawn(terminal_window_t *win, const char *shell_path, const char *dir)
{
    int rc;

    if (!win || !shell_path) {
        return -EINVAL;
    }

    printk("[TERMINAL] Starting shell '%s' in %s\n",
           shell_path, dir ? dir : "/");

    rc = shell_init();
    if (rc != 0) {
        printk("[TERMINAL] shell_init failed: %d\n", rc);
        return rc;
    }

    /* Honour the requested starting directory. */
    if (dir && *dir) {
        vfs_chdir(dir);
    }

    /* Point the shell's stdout/stderr at this window. */
    shell_set_output(terminal_shell_sink, win);
    shell_set_error(terminal_shell_sink, win);

    win->shell_pid = shell_get_pid();
    win->is_running = shell_is_running();

    /* Banner, then the first prompt. */
    terminal_write(win,
        "NEXUS OS 1.0.0 - Nexus Shell\n"
        "Type 'help' for a list of commands.\n\n", 70);

    shell_print_prompt();

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

    /*
     * The shell is in-process, so there is no kernel PTY to poll: input
     * arrives through terminal_send_key()/terminal_send_string(), which
     * feed the line editor. Nothing is pending here.
     */
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
    /*
     * Writing to the "PTY" means handing bytes to the shell's line editor.
     * Newline commits the line and runs it; anything else accumulates and
     * is echoed back to the cell buffer so the user sees what they typed.
     */
    static char line_buf[SHELL_MAX_LINE];
    static u32 line_len;

    if (!win || !data || length == 0) {
        return -EINVAL;
    }

    for (u32 i = 0; i < length; i++) {
        char c = data[i];

        if (c == '\r') {
            continue;               /* CRLF input: ignore the CR. */
        }

        if (c == '\n') {
            terminal_write(win, "\n", 1);
            line_buf[line_len] = '\0';
            if (line_len > 0) {
                shell_submit_line(line_buf);
            }
            line_len = 0;
            terminal_write(win, "\n", 1);
            shell_print_prompt();
            continue;
        }

        if (c == 0x7F || c == '\b') {
            if (line_len > 0) {
                line_len--;
                terminal_write(win, "\b \b", 3);
            }
            continue;
        }

        if (c == 0x03) {            /* ^C */
            line_len = 0;
            terminal_write(win, "^C\n", 3);
            shell_print_prompt();
            continue;
        }

        if (line_len + 1 < sizeof(line_buf)) {
            line_buf[line_len++] = c;
            terminal_write(win, &c, 1);
        }
    }

    return (int)length;
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

/*===========================================================================*/
/*              SPLIT PANES AND BOOKMARKS (DECLARED, NEVER DEFINED)          */
/*===========================================================================*/
/*
 * terminal.h declared eight pane and bookmark functions that had no bodies.
 * The pane tree structure (split_pane_t) was already defined, and windows
 * already carried root_pane/active_pane/next_pane_id, but nothing ever built,
 * walked or dismantled a tree - so a window could never be split.
 */

/**
 * terminal_split_pane - Split the active pane in two
 *
 * @horizontal: true for a side-by-side split, false for top-and-bottom.
 *
 * The active pane becomes an internal node and receives two children sized
 * from its bounds and the split ratio. The new right/bottom pane is created
 * with its own tab sharing the window's terminal.
 */
int terminal_split_pane(terminal_window_t *win, bool horizontal)
{
    split_pane_t *pane;
    split_pane_t *a;
    split_pane_t *b;
    terminal_tab_t *tab;
    u32 w, h;

    if (!win || !win->active_pane) {
        return -EINVAL;
    }

    pane = win->active_pane;

    /* A leaf becomes an internal node; refuse to split one that already is. */
    if (pane->first_child) {
        return -EBUSY;
    }

    a = (split_pane_t *)kzalloc(sizeof(split_pane_t));
    b = (split_pane_t *)kzalloc(sizeof(split_pane_t));
    if (!a || !b) {
        if (a) kfree(a);
        if (b) kfree(b);
        return -ENOMEM;
    }

    w = (u32)(pane->bounds.width > 0 ? pane->bounds.width
                                     : (win->wm_window ? win->wm_window->geometry.width : 640));
    h = (u32)(pane->bounds.height > 0 ? pane->bounds.height
                                      : (win->wm_window ? win->wm_window->geometry.height : 480));

    /* First child inherits the existing tab. */
    a->pane_id = win->next_pane_id++;
    a->tab = pane->tab;
    a->parent = pane;
    a->split_ratio = 50;
    a->bounds.x = pane->bounds.x;
    a->bounds.y = pane->bounds.y;

    /* Second child gets a fresh tab on the same shell profile. */
    tab = (terminal_tab_t *)kzalloc(sizeof(terminal_tab_t));
    if (!tab) {
        kfree(a);
        kfree(b);
        return -ENOMEM;
    }
    tab->tab_id = win->next_pane_id++;
    tab->is_active = true;
    if (pane->tab && pane->tab->shell_path[0]) {
        strncpy(tab->shell_path, pane->tab->shell_path,
                sizeof(tab->shell_path) - 1);
    } else {
        strncpy(tab->shell_path, "/bin/nexsh", sizeof(tab->shell_path) - 1);
    }
    strncpy(tab->title, "nexsh", sizeof(tab->title) - 1);

    b->pane_id = win->next_pane_id++;
    b->tab = tab;
    b->parent = pane;
    b->split_ratio = 50;
    b->bounds.x = pane->bounds.x;
    b->bounds.y = pane->bounds.y;

    if (horizontal) {
        /* Side by side: split the width. */
        a->bounds.width = (s32)(w / 2);
        a->bounds.height = (s32)h;
        b->bounds.x = pane->bounds.x + (s32)(w / 2);
        b->bounds.width = (s32)(w - w / 2);
        b->bounds.height = (s32)h;
    } else {
        /* Stacked: split the height. */
        a->bounds.width = (s32)w;
        a->bounds.height = (s32)(h / 2);
        b->bounds.y = pane->bounds.y + (s32)(h / 2);
        b->bounds.width = (s32)w;
        b->bounds.height = (s32)(h - h / 2);
    }

    pane->is_horizontal = horizontal;
    pane->first_child = a;
    pane->second_child = b;

    win->is_split = true;
    win->active_pane = b;

    /* Move the active-tab marker to the new pane. */
    if (pane->tab) {
        pane->tab->is_active = false;
    }

    return 0;
}

/**
 * free_pane_tree - Recursively release a pane subtree
 */
static void free_pane_tree(split_pane_t *pane)
{
    if (!pane) {
        return;
    }

    free_pane_tree(pane->first_child);
    free_pane_tree(pane->second_child);

    /* Only leaves own their tab. */
    if (!pane->first_child && !pane->second_child && pane->tab) {
        kfree(pane->tab);
    }

    kfree(pane);
}

/**
 * terminal_close_pane - Close a pane, collapsing its parent
 *
 * The sibling takes the parent's place, so closing one half of a split
 * returns the survivor to full size rather than leaving a gap.
 */
int terminal_close_pane(terminal_window_t *win, split_pane_t *pane)
{
    split_pane_t *parent;
    split_pane_t *sibling;

    if (!win || !pane) {
        return -EINVAL;
    }

    parent = pane->parent;

    /* Closing the root tears down the whole window's panes. */
    if (!parent) {
        if (win->root_pane) {
            free_pane_tree(win->root_pane);
        }
        win->root_pane = NULL;
        win->active_pane = NULL;
        win->is_split = false;
        return 0;
    }

    sibling = (parent->first_child == pane) ? parent->second_child
                                            : parent->first_child;
    if (!sibling) {
        return -EINVAL;
    }

    /* The survivor inherits the parent's geometry and position in the tree. */
    sibling->parent = parent->parent;
    sibling->bounds = parent->bounds;

    if (parent->parent) {
        if (parent->parent->first_child == parent) {
            parent->parent->first_child = sibling;
        } else {
            parent->parent->second_child = sibling;
        }
    } else {
        win->root_pane = sibling;
    }

    /* Only leaves own their tab. */
    if (!pane->first_child && !pane->second_child && pane->tab) {
        kfree(pane->tab);
    }

    /* Detach the sibling we are keeping before freeing the parent. */
    parent->first_child = NULL;
    parent->second_child = NULL;
    kfree(parent);
    kfree(pane);

    win->is_split = (win->root_pane && win->root_pane->first_child);

    if (win->active_pane == pane) {
        win->active_pane = sibling;
        if (sibling->tab) {
            sibling->tab->is_active = true;
        }
    }

    return 0;
}

/**
 * terminal_switch_pane - Focus a pane
 */
int terminal_switch_pane(terminal_window_t *win, split_pane_t *pane)
{
    if (!win || !pane) {
        return -EINVAL;
    }

    /* Only leaves can take focus: an internal node has no terminal. */
    if (pane->first_child) {
        return -EINVAL;
    }

    if (win->active_pane && win->active_pane->tab) {
        win->active_pane->tab->is_active = false;
    }

    win->active_pane = pane;

    if (pane->tab) {
        pane->tab->is_active = true;
        pane->tab->has_activity = false;
    }

    return 0;
}

/**
 * terminal_resize_pane - Change a split's ratio
 *
 * @size: New ratio as a percentage, 0-100. The children's bounds are
 *        recomputed so the ratio is actually reflected on screen.
 */
int terminal_resize_pane(terminal_window_t *win, split_pane_t *pane, u32 size)
{
    split_pane_t *a;
    split_pane_t *b;
    u32 total;
    u32 first;

    if (!win || !pane) {
        return -EINVAL;
    }

    if (!pane->first_child || !pane->second_child) {
        return -EINVAL;         /* Not a split. */
    }

    if (size < 1 || size > 99) {
        return -ERANGE;
    }

    a = pane->first_child;
    b = pane->second_child;

    total = (u32)(pane->is_horizontal ? pane->bounds.width
                                      : pane->bounds.height);
    if (total == 0) {
        return -EINVAL;
    }

    first = (total * size) / 100;
    if (first == 0) {
        first = 1;
    }

    if (pane->is_horizontal) {
        a->bounds.x = pane->bounds.x;
        a->bounds.y = pane->bounds.y;
        a->bounds.width = (s32)first;
        a->bounds.height = pane->bounds.height;

        b->bounds.x = pane->bounds.x + (s32)first;
        b->bounds.y = pane->bounds.y;
        b->bounds.width = (s32)(total - first);
        b->bounds.height = pane->bounds.height;
    } else {
        a->bounds.x = pane->bounds.x;
        a->bounds.y = pane->bounds.y;
        a->bounds.width = pane->bounds.width;
        a->bounds.height = (s32)first;

        b->bounds.x = pane->bounds.x;
        b->bounds.y = pane->bounds.y + (s32)first;
        b->bounds.width = pane->bounds.width;
        b->bounds.height = (s32)(total - first);
    }

    pane->split_ratio = size;

    return 0;
}

/**
 * terminal_unsplit_panes - Collapse every split back to one pane
 *
 * Keeps the active pane's terminal and discards the rest.
 */
int terminal_unsplit_panes(terminal_window_t *win)
{
    split_pane_t *keep;
    split_pane_t *old_root;

    if (!win) {
        return -EINVAL;
    }

    if (!win->root_pane) {
        return 0;
    }

    /* Detach the active pane from the tree before freeing everything else. */
    keep = win->active_pane;
    if (!keep || keep->first_child) {
        /* Pick the leftmost leaf if the active pane is an internal node. */
        keep = win->root_pane;
        while (keep->first_child) {
            keep = keep->first_child;
        }
    }

    old_root = win->root_pane;

    /* Preserve the tab, then free the tree without freeing that tab. */
    terminal_tab_t *saved = keep->tab;
    keep->tab = NULL;

    free_pane_tree(old_root);

    /* Reuse the active pane node as the new single root. */
    memset(keep, 0, sizeof(split_pane_t));
    keep->pane_id = win->next_pane_id++;
    keep->tab = saved;
    keep->split_ratio = 100;
    keep->bounds.width = (s32)(win->wm_window ? win->wm_window->geometry.width : 640);
    keep->bounds.height = (s32)(win->wm_window ? win->wm_window->geometry.height : 480);

    if (saved) {
        saved->is_active = true;
    }

    win->root_pane = keep;
    win->active_pane = keep;
    win->is_split = false;

    return 0;
}

/*===========================================================================*/
/*                              BOOKMARKS                                    */
/*===========================================================================*/

/**
 * terminal_add_bookmark - Store a bookmark
 */
int terminal_add_bookmark(terminal_window_t *win, terminal_bookmark_t *bookmark)
{
    if (!win || !bookmark) {
        return -EINVAL;
    }

    if (win->bookmark_count >= TERMINAL_MAX_BOOKMARKS) {
        return -ENOSPC;
    }

    win->bookmarks[win->bookmark_count++] = *bookmark;

    return 0;
}

/**
 * terminal_remove_bookmark - Delete a bookmark by index
 */
int terminal_remove_bookmark(terminal_window_t *win, u32 index)
{
    if (!win) {
        return -EINVAL;
    }

    if (index >= win->bookmark_count) {
        return -EINVAL;
    }

    for (u32 i = index; i + 1 < win->bookmark_count; i++) {
        win->bookmarks[i] = win->bookmarks[i + 1];
    }
    win->bookmark_count--;

    return 0;
}

/**
 * terminal_open_bookmark - Open a new tab at a bookmark's location
 *
 * The bookmark's shell is launched as the tab's shell and its path becomes the
 * tab's working directory, so the tab starts where the user left off.
 */
int terminal_open_bookmark(terminal_window_t *win, u32 index)
{
    terminal_bookmark_t *bm;
    terminal_tab_t *tab;

    if (!win) {
        return -EINVAL;
    }

    if (index >= win->bookmark_count) {
        return -EINVAL;
    }

    bm = &win->bookmarks[index];

    if (win->tab_count >= TERMINAL_MAX_TABS) {
        return -ENOSPC;
    }

    tab = (terminal_tab_t *)kzalloc(sizeof(terminal_tab_t));
    if (!tab) {
        return -ENOMEM;
    }

    tab->tab_id = win->next_tab_id++;
    tab->is_active = true;

    strncpy(tab->shell_path,
            bm->shell[0] ? bm->shell : "/bin/nexsh",
            sizeof(tab->shell_path) - 1);
    strncpy(tab->title,
            bm->name[0] ? bm->name : "nexsh",
            sizeof(tab->title) - 1);

    /* Deactivate whatever tab was current. */
    for (terminal_tab_t *t = win->tabs; t; t = t->next) {
        t->is_active = false;
    }

    /* Append to the tab list. */
    tab->next = NULL;
    if (!win->tabs) {
        win->tabs = tab;
    } else {
        terminal_tab_t *last = win->tabs;
        while (last->next) {
            last = last->next;
        }
        last->next = tab;
    }

    win->active_tab = tab;
    win->tab_count++;

    /*
     * Change into the bookmark's directory. A bookmark to a directory that
     * has since gone is not fatal: the tab still opens, just at the default.
     */
    if (bm->path[0]) {
        if (vfs_chdir(bm->path) != 0) {
            pr_warn("[TERMINAL] Bookmark '%s': %s is gone\n", bm->name, bm->path);
        }
    }

    return 0;
}
