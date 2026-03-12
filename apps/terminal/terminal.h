/*
 * NEXUS OS - Terminal Emulator Application
 * apps/terminal/terminal.h
 *
 * Modern terminal emulator with tabs, split panes, and themes
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         TERMINAL CONFIGURATION                            */
/*===========================================================================*/

#define TERMINAL_VERSION            "1.0.0"
#define TERMINAL_MAX_TABS           16
#define TERMINAL_MAX_HISTORY        10000
#define TERMINAL_MAX_SPLIT_PANES    8
#define TERMINAL_MAX_THEMES         32
#define TERMINAL_MAX_FONTS          16
#define TERMINAL_MAX_PROFILES       32
#define TERMINAL_MAX_SHORTCUTS      64
#define TERMINAL_MAX_BOOKMARKS      64

/*===========================================================================*/
/*                         TERMINAL MODES                                    */
/*===========================================================================*/

#define TERMINAL_MODE_NORMAL        0
#define TERMINAL_MODE_FULLSCREEN    1
#define TERMINAL_MODE_SPLIT         2

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Terminal Character Cell
 */
typedef struct {
    char character;                   /* Character */
    u8 fg_color;                      /* Foreground Color */
    u8 bg_color;                      /* Background Color */
    bool bold;                        /* Bold */
    bool underline;                   /* Underline */
    bool italic;                      /* Italic */
    bool reverse;                     /* Reverse */
    bool blink;                       /* Blink */
} terminal_cell_t;

/**
 * Terminal Screen Buffer
 */
typedef struct {
    terminal_cell_t *cells;           /* Cell Buffer */
    u32 width;                        /* Width (columns) */
    u32 height;                       /* Height (rows) */
    u32 scrollback_size;              /* Scrollback Size */
    u32 cursor_x;                     /* Cursor X */
    u32 cursor_y;                     /* Cursor Y */
    bool cursor_visible;              /* Cursor Visible */
    bool cursor_blink;                /* Cursor Blink */
    u8 fg_color;                      /* Default Foreground */
    u8 bg_color;                      /* Default Background */
} terminal_screen_t;

/**
 * Terminal Tab
 */
typedef struct terminal_tab {
    u32 tab_id;                       /* Tab ID */
    char title[128];                  /* Tab Title */
    char shell_path[256];             /* Shell Path */
    pid_t shell_pid;                  /* Shell PID */
    terminal_screen_t *screen;        /* Screen Buffer */
    bool is_active;                   /* Is Active */
    bool has_activity;                /* Has Activity */
    bool is_bell;                     /* Has Bell */
    u64 created_time;                 /* Created Time */
    struct terminal_tab *next;        /* Next Tab */
} terminal_tab_t;

/**
 * Split Pane
 */
typedef struct split_pane {
    u32 pane_id;                      /* Pane ID */
    rect_t bounds;                    /* Pane Bounds */
    terminal_tab_t *tab;              /* Active Tab */
    bool is_horizontal;               /* Horizontal Split */
    struct split_pane *parent;        /* Parent Pane */
    struct split_pane *next;          /* Next Sibling */
    struct split_pane *prev;          /* Previous Sibling */
    struct split_pane *first_child;   /* First Child */
    struct split_pane *second_child;  /* Second Child */
    u32 split_ratio;                  /* Split Ratio (0-100) */
} split_pane_t;

/**
 * Terminal Theme
 */
typedef struct {
    char name[64];                    /* Theme Name */
    char fg_color[16];                /* Foreground Color */
    char bg_color[16];                /* Background Color */
    char cursor_color[16];            /* Cursor Color */
    char colors[16][16];              /* ANSI Colors */
    char font_name[64];               /* Font Name */
    u32 font_size;                    /* Font Size */
    bool bold_text;                   /* Bold Text */
    bool allow_bold;                  /* Allow Bold */
    bool cursor_blink;                /* Cursor Blink */
    bool scrollback_unlimited;        /* Unlimited Scrollback */
    u32 scrollback_lines;             /* Scrollback Lines */
} terminal_theme_t;

/**
 * Terminal Profile
 */
typedef struct {
    char name[64];                    /* Profile Name */
    char shell_path[256];             /* Shell Path */
    char working_dir[512];            /* Working Directory */
    terminal_theme_t theme;           /* Theme */
    char **environment;               /* Environment Variables */
    u32 env_count;                    /* Environment Count */
    bool is_default;                  /* Is Default */
} terminal_profile_t;

/**
 * Keyboard Shortcut
 */
typedef struct {
    char name[64];                    /* Shortcut Name */
    u32 key;                          /* Key Code */
    u32 modifiers;                    /* Modifiers */
    void (*callback)(void *);         /* Callback */
    void *user_data;                  /* User Data */
} terminal_shortcut_t;

/**
 * Terminal Bookmark
 */
typedef struct {
    char name[128];                   /* Bookmark Name */
    char path[512];                   /* Path */
    char shell[64];                   /* Shell */
    u32 profile_id;                   /* Profile ID */
} terminal_bookmark_t;

/**
 * Terminal Window
 */
typedef struct terminal_window {
    u32 window_id;                    /* Window ID */
    wm_window_t *wm_window;           /* WM Window */
    
    /* Tabs */
    terminal_tab_t *tabs;             /* Tabs */
    u32 tab_count;                    /* Tab Count */
    u32 active_tab;                   /* Active Tab */
    u32 next_tab_id;                  /* Next Tab ID */
    widget_t *tab_bar;                /* Tab Bar Widget */
    
    /* Split Panes */
    split_pane_t *root_pane;          /* Root Pane */
    split_pane_t *active_pane;        /* Active Pane */
    u32 next_pane_id;                 /* Next Pane ID */
    bool is_split;                    /* Is Split */
    
    /* UI Components */
    widget_t *terminal_view;          /* Terminal View */
    widget_t *scrollbar;              /* Scrollbar */
    widget_t *toolbar;                /* Toolbar */
    widget_t *status_bar;             /* Status Bar */
    widget_t *find_bar;               /* Find Bar */
    desktop_menu_t *context_menu;     /* Context Menu */
    
    /* Screen */
    terminal_screen_t *screen;        /* Current Screen */
    u32 cell_width;                   /* Cell Width */
    u32 cell_height;                  /* Cell Height */
    
    /* Input */
    char input_buffer[4096];          /* Input Buffer */
    u32 input_length;                 /* Input Length */
    bool is_selecting;                /* Is Selecting */
    u32 selection_start_x;            /* Selection Start X */
    u32 selection_start_y;            /* Selection Start Y */
    u32 selection_end_x;              /* Selection End X */
    u32 selection_end_y;              /* Selection End Y */
    char *clipboard;                  /* Clipboard */
    
    /* Find */
    bool is_finding;                  /* Is Finding */
    char find_query[256];             /* Find Query */
    bool find_case_sensitive;         /* Case Sensitive */
    bool find_regex;                  /* Use Regex */
    u32 find_matches;                 /* Find Matches */
    
    /* Settings */
    terminal_theme_t current_theme;   /* Current Theme */
    terminal_profile_t current_profile; /* Current Profile */
    u32 opacity;                      /* Opacity (0-100) */
    bool blur_background;             /* Blur Background */
    bool show_menubar;                /* Show Menubar */
    bool show_toolbar;                /* Show Toolbar */
    bool show_scrollbar;              /* Show Scrollbar */
    bool show_status_bar;             /* Show Status Bar */
    bool audible_bell;                /* Audible Bell */
    bool visual_bell;                 /* Visual Bell */
    bool mouse_reporting;             /* Mouse Reporting */
    bool allow_resize;                /* Allow Resize */
    
    /* History */
    char **command_history;           /* Command History */
    u32 history_count;                /* History Count */
    u32 history_index;                /* History Index */
    
    /* Bookmarks */
    terminal_bookmark_t bookmarks[TERMINAL_MAX_BOOKMARKS];
    u32 bookmark_count;               /* Bookmark Count */
    
    /* Shortcuts */
    terminal_shortcut_t shortcuts[TERMINAL_MAX_SHORTCUTS];
    u32 shortcut_count;               /* Shortcut Count */
    
    /* Process */
    pid_t shell_pid;                  /* Shell PID */
    int master_fd;                    /* Master FD */
    int slave_fd;                     /* Slave FD */
    bool is_running;                  /* Is Running */
    u32 exit_code;                    /* Exit Code */
    
    /* Callbacks */
    void (*on_output)(struct terminal_window *, const char *, u32);
    void (*on_title_change)(struct terminal_window *, const char *);
    void (*on_close)(struct terminal_window *);
    
    /* User Data */
    void *user_data;
    
} terminal_window_t;

/**
 * Terminal Application
 */
typedef struct {
    bool initialized;                 /* Is Initialized */
    bool running;                     /* Is Running */
    
    /* Windows */
    terminal_window_t *windows;       /* Windows */
    u32 window_count;                 /* Window Count */
    u32 next_window_id;               /* Next Window ID */
    
    /* Themes */
    terminal_theme_t themes[TERMINAL_MAX_THEMES];
    u32 theme_count;                  /* Theme Count */
    
    /* Profiles */
    terminal_profile_t profiles[TERMINAL_MAX_PROFILES];
    u32 profile_count;                /* Profile Count */
    
    /* Fonts */
    char fonts[TERMINAL_MAX_FONTS][64];
    u32 font_count;                   /* Font Count */
    
    /* Global Settings */
    terminal_profile_t default_profile; /* Default Profile */
    u32 default_theme;                /* Default Theme */
    bool save_session;                /* Save Session */
    bool confirm_close;               /* Confirm Close */
    u32 word_separators;              /* Word Separators */
    
    /* Callbacks */
    int (*on_window_created)(terminal_window_t *);
    int (*on_window_closed)(terminal_window_t *);
    
    /* User Data */
    void *user_data;
    
} terminal_app_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Application lifecycle */
int terminal_app_init(terminal_app_t *app);
int terminal_app_run(terminal_app_t *app);
int terminal_app_shutdown(terminal_app_t *app);
bool terminal_app_is_running(terminal_app_t *app);

/* Window management */
terminal_window_t *terminal_create_window(terminal_app_t *app, terminal_profile_t *profile);
int terminal_close_window(terminal_app_t *app, terminal_window_t *win);
terminal_window_t *terminal_get_window(terminal_app_t *app, u32 window_id);

/* Tab management */
terminal_tab_t *terminal_new_tab(terminal_window_t *win, const char *shell);
int terminal_close_tab(terminal_window_t *win, u32 tab_id);
int terminal_switch_tab(terminal_window_t *win, u32 tab_id);
int terminal_rename_tab(terminal_window_t *win, u32 tab_id, const char *title);
int terminal_duplicate_tab(terminal_window_t *win);

/* Split pane management */
int terminal_split_pane(terminal_window_t *win, bool horizontal);
int terminal_close_pane(terminal_window_t *win, split_pane_t *pane);
int terminal_switch_pane(terminal_window_t *win, split_pane_t *pane);
int terminal_resize_pane(terminal_window_t *win, split_pane_t *pane, u32 size);
int terminal_unsplit_panes(terminal_window_t *win);

/* Terminal I/O */
int terminal_write(terminal_window_t *win, const char *data, u32 length);
int terminal_read(terminal_window_t *win, char *buffer, u32 length);
int terminal_send_key(terminal_window_t *win, u32 key, u32 modifiers);
int terminal_send_string(terminal_window_t *win, const char *str);

/* Screen operations */
int terminal_clear_screen(terminal_window_t *win);
int terminal_scroll_up(terminal_window_t *win, u32 lines);
int terminal_scroll_down(terminal_window_t *win, u32 lines);
int terminal_resize_screen(terminal_window_t *win, u32 width, u32 height);

/* Selection */
int terminal_select_all(terminal_window_t *win);
int terminal_copy_selection(terminal_window_t *win);
int terminal_paste(terminal_window_t *win);
char *terminal_get_selection(terminal_window_t *win);

/* Find */
int terminal_find(terminal_window_t *win, const char *query, bool forward);
int terminal_clear_find(terminal_window_t *win);

/* Themes */
int terminal_load_theme(terminal_app_t *app, const char *path);
int terminal_set_theme(terminal_window_t *win, const char *theme_name);
int terminal_export_theme(terminal_app_t *app, const char *path);

/* Profiles */
int terminal_create_profile(terminal_app_t *app, terminal_profile_t *profile);
int terminal_set_profile(terminal_window_t *win, terminal_profile_t *profile);
int terminal_delete_profile(terminal_app_t *app, const char *name);

/* Bookmarks */
int terminal_add_bookmark(terminal_app_t *app, terminal_bookmark_t *bookmark);
int terminal_remove_bookmark(terminal_app_t *app, u32 index);
int terminal_open_bookmark(terminal_window_t *win, u32 index);

/* Shortcuts */
int terminal_add_shortcut(terminal_app_t *app, terminal_shortcut_t *shortcut);
int terminal_remove_shortcut(terminal_app_t *app, const char *name);

/* Settings */
int terminal_set_opacity(terminal_window_t *win, u32 opacity);
int terminal_set_font(terminal_window_t *win, const char *font, u32 size);
int terminal_toggle_menubar(terminal_window_t *win);
int terminal_toggle_toolbar(terminal_window_t *win);
int terminal_toggle_fullscreen(terminal_window_t *win);

/* Process management */
int terminal_spawn_shell(terminal_window_t *win, const char *shell, const char *dir);
int terminal_kill_process(terminal_window_t *win);
int terminal_get_process_info(terminal_window_t *win, char *buffer, u32 size);

/* Utility functions */
const char *terminal_get_version(void);
terminal_app_t *terminal_app_get_instance(void);

#endif /* _TERMINAL_H */
