/*
 * NEXUS OS - Advanced File Explorer Application
 * apps/file-explorer/file-explorer.h
 *
 * Modern, powerful file manager with advanced features
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _FILE_EXPLORER_H
#define _FILE_EXPLORER_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         FILE EXPLORER CONFIGURATION                       */
/*===========================================================================*/

#define FILE_EXPLORER_VERSION       "2.0.0"
#define MAX_PATH_LENGTH             4096
#define MAX_FILENAME_LENGTH         256
#define MAX_PREVIEW_SIZE            1024 * 1024
#define MAX_THUMBNAIL_CACHE         5000
#define MAX_BOOKMARKS               64
#define MAX_RECENT_FOLDERS          50
#define MAX_SEARCH_HISTORY          100
#define MAX_TABS                    32
#define MAX_COLUMNS                 16
#define MAX_FILTERS                 32
#define MAX_QUICK_ACTIONS           16

/*===========================================================================*/
/*                         FILE TYPES                                        */
/*===========================================================================*/

#define FILE_TYPE_UNKNOWN         0
#define FILE_TYPE_FILE            1
#define FILE_TYPE_FOLDER          2
#define FILE_TYPE_SYMLINK         3
#define FILE_TYPE_DEVICE          4
#define FILE_TYPE_MOUNT           5

/*===========================================================================*/
/*                         FILE CATEGORIES                                   */
/*===========================================================================*/

#define FILE_CATEGORY_ALL         0
#define FILE_CATEGORY_DOCUMENTS   1
#define FILE_CATEGORY_IMAGES      2
#define FILE_CATEGORY_AUDIO       3
#define FILE_CATEGORY_VIDEO       4
#define FILE_CATEGORY_ARCHIVES    5
#define FILE_CATEGORY_CODE        6
#define FILE_CATEGORY_SYSTEM      7

/*===========================================================================*/
/*                         VIEW MODES                                        */
/*===========================================================================*/

#define VIEW_MODE_ICONS_LARGE     0
#define VIEW_MODE_ICONS_MEDIUM    1
#define VIEW_MODE_ICONS_SMALL     2
#define VIEW_MODE_LIST            3
#define VIEW_MODE_DETAILS         4
#define VIEW_MODE_TILES           5
#define VIEW_MODE_CONTENT         6

/*===========================================================================*/
/*                         SORT OPTIONS                                      */
/*===========================================================================*/

#define SORT_BY_NAME              0
#define SORT_BY_DATE              1
#define SORT_BY_SIZE              2
#define SORT_BY_TYPE              3
#define SORT_BY_EXTENSION         4

#define SORT_ORDER_ASCENDING      0
#define SORT_ORDER_DESCENDING     1

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * File Information
 */
typedef struct {
    char name[MAX_FILENAME_LENGTH];       /* File Name */
    char path[MAX_PATH_LENGTH];           /* Full Path */
    char extension[16];                   /* File Extension */
    u32 file_type;                        /* File Type */
    u32 file_category;                    /* File Category */
    u64 size;                             /* File Size */
    u64 created_time;                     /* Created Time */
    u64 modified_time;                    /* Modified Time */
    u64 accessed_time;                    /* Accessed Time */
    bool is_hidden;                       /* Is Hidden */
    bool is_system;                       /* Is System File */
    bool is_readonly;                     /* Is Read-Only */
    bool is_archive;                      /* Is Archive */
    bool is_compressed;                   /* Is Compressed */
    bool is_encrypted;                    /* Is Encrypted */
    char owner[64];                       /* Owner */
    char group[64];                       /* Group */
    u32 permissions;                      /* Permissions */
    char mime_type[64];                   /* MIME Type */
    char icon_path[MAX_PATH_LENGTH];      /* Icon Path */
    void *thumbnail;                      /* Thumbnail Data */
    u32 thumbnail_size;                   /* Thumbnail Size */
    char tooltip[512];                    /* Tooltip */
    void *data;                           /* User Data */
} file_info_t;

/**
 * Directory Entry
 */
typedef struct directory_entry {
    file_info_t file;                     /* File Info */
    struct directory_entry *next;         /* Next Entry */
} directory_entry_t;

/**
 * Navigation History Entry
 */
typedef struct {
    char path[MAX_PATH_LENGTH];           /* Path */
    u64 timestamp;                        /* Timestamp */
} nav_history_entry_t;

/**
 * Bookmark Entry
 */
typedef struct {
    u32 bookmark_id;                      /* Bookmark ID */
    char name[128];                       /* Bookmark Name */
    char path[MAX_PATH_LENGTH];           /* Bookmark Path */
    char icon_path[MAX_PATH_LENGTH];      /* Icon Path */
    bool is_system;                       /* Is System Bookmark */
} bookmark_entry_t;

/**
 * Search Result
 */
typedef struct {
    file_info_t file;                     /* File Info */
    u32 relevance;                        /* Relevance Score */
    char match_context[256];              /* Match Context */
} search_result_t;

/**
 * Preview Panel
 */
typedef struct {
    bool visible;                         /* Is Visible */
    u32 preview_type;                     /* Preview Type */
    file_info_t *current_file;            /* Current File */
    void *preview_data;                   /* Preview Data */
    u32 preview_size;                     /* Preview Size */
    char text_preview[4096];              /* Text Preview */
    void *image_preview;                  /* Image Preview */
    char metadata[1024];                  /* File Metadata */
} preview_panel_t;

/**
 * File Explorer Window
 */
typedef struct file_explorer_window {
    u32 window_id;                        /* Window ID */
    wm_window_t *wm_window;               /* WM Window */

    /* Tabs */
    char tab_paths[MAX_TABS][MAX_PATH_LENGTH]; /* Tab Paths */
    u32 tab_count;                        /* Tab Count */
    u32 active_tab;                       /* Active Tab */
    bool show_tabs;                       /* Show Tabs */

    /* Dual Pane */
    bool dual_pane_mode;                  /* Dual Pane Mode */
    struct file_explorer_window *paired_window; /* Paired Window */

    /* Navigation */
    char current_path[MAX_PATH_LENGTH];   /* Current Path */
    char home_path[MAX_PATH_LENGTH];      /* Home Path */
    nav_history_entry_t history[100];     /* Navigation History */
    u32 history_index;                    /* History Index */
    u32 history_count;                    /* History Count */

    /* File List */
    directory_entry_t *entries;           /* Directory Entries */
    u32 entry_count;                      /* Entry Count */
    u32 selected_count;                   /* Selected Count */
    directory_entry_t **selected;         /* Selected Entries */
    u32 scroll_offset;                    /* Scroll Offset */
    u32 scroll_max;                       /* Scroll Max */
    bool smooth_scroll;                   /* Smooth Scrolling */
    float scroll_velocity;                /* Scroll Velocity */

    /* View Settings */
    u32 view_mode;                        /* View Mode */
    u32 sort_by;                          /* Sort By */
    u32 sort_order;                       /* Sort Order */
    bool show_hidden;                     /* Show Hidden */
    bool group_folders_first;             /* Group Folders First */
    u32 icon_size;                        /* Icon Size */
    bool show_previews;                   /* Show Previews */
    bool show_thumbnails;                 /* Show Thumbnails */
    bool show_file_size;                  /* Show File Size */
    bool show_file_dates;                 /* Show File Dates */
    bool show_file_type;                  /* Show File Type */
    bool tree_view;                       /* Tree View in Sidebar */

    /* Columns (Details View) */
    struct {
        char name[64];                    /* Column Name */
        u32 width;                        /* Column Width */
        bool visible;                     /* Column Visible */
        u32 sort_order;                   /* Sort Order */
    } columns[MAX_COLUMNS];
    u32 column_count;                     /* Column Count */

    /* Filter */
    u32 filter_category;                  /* Filter Category */
    char filter_pattern[64];              /* Filter Pattern */
    u64 filter_min_size;                  /* Min Size Filter */
    u64 filter_max_size;                  /* Max Size Filter */
    u64 filter_date_from;                 /* Date From Filter */
    u64 filter_date_to;                   /* Date To Filter */

    /* UI Components */
    widget_t *toolbar;                    /* Toolbar */
    widget_t *tab_bar;                    /* Tab Bar */
    widget_t *address_bar;                /* Address Bar */
    widget_t *search_box;                 /* Search Box */
    widget_t *sidebar;                    /* Sidebar */
    widget_t *file_view;                  /* File View */
    widget_t *status_bar;                 /* Status Bar */
    widget_t *preview_panel;              /* Preview Panel */
    widget_t *bookmarks_panel;            /* Bookmarks Panel */
    widget_t *filter_panel;               /* Filter Panel */
    widget_t *tree_panel;                 /* Tree Panel */
    widget_t *dual_pane_splitter;         /* Dual Pane Splitter */

    /* Context Menu */
    desktop_menu_t context_menu;          /* Context Menu */
    desktop_menu_t *active_context_menu;  /* Active Context Menu */
    s32 context_menu_x;                   /* Context Menu X */
    s32 context_menu_y;                   /* Context Menu Y */
    file_info_t *context_file;            /* Context Menu File */

    /* Quick Actions */
    struct {
        char name[64];                    /* Action Name */
        char icon[256];                   /* Action Icon */
        char shortcut[32];                /* Keyboard Shortcut */
        void (*callback)(struct file_explorer_window *);
    } quick_actions[MAX_QUICK_ACTIONS];
    u32 quick_action_count;               /* Quick Action Count */

    /* Bookmarks */
    bookmark_entry_t bookmarks[MAX_BOOKMARKS]; /* Bookmarks */
    u32 bookmark_count;                   /* Bookmark Count */

    /* Recent Folders */
    char recent_folders[MAX_RECENT_FOLDERS][MAX_PATH_LENGTH];
    u32 recent_folder_count;              /* Recent Folder Count */

    /* Search */
    search_result_t *search_results;      /* Search Results */
    u32 search_result_count;              /* Search Result Count */
    bool is_searching;                    /* Is Searching */
    char search_query[256];               /* Search Query */
    char search_history[MAX_SEARCH_HISTORY][256]; /* Search History */
    u32 search_history_count;             /* Search History Count */
    bool search_in_subfolders;            /* Search in Subfolders */
    bool search_case_sensitive;           /* Case Sensitive */
    bool search_regex;                    /* Use Regex */

    /* Preview */
    preview_panel_t preview;              /* Preview Panel */

    /* Clipboard */
    char **clipboard_files;               /* Clipboard Files */
    u32 clipboard_count;                  /* Clipboard Count */
    u32 clipboard_operation;              /* Clipboard Operation (0=none, 1=copy, 2=cut) */

    /* Drag & Drop */
    bool is_dragging;                     /* Is Dragging */
    directory_entry_t *drag_source;       /* Drag Source */
    bool drag_accept;                     /* Drag Accept */

    /* Progress */
    bool has_progress;                    /* Has Progress */
    char progress_title[128];             /* Progress Title */
    char progress_message[256];           /* Progress Message */
    u32 progress_current;                 /* Progress Current */
    u32 progress_total;                   /* Progress Total */
    bool progress_cancelled;              /* Progress Cancelled */

    /* Bulk Operations */
    bool bulk_rename_mode;                /* Bulk Rename Mode */
    char bulk_rename_pattern[128];        /* Bulk Rename Pattern */
    u32 bulk_operation_count;             /* Bulk Operation Count */

    /* View State */
    bool fullscreen_mode;                 /* Fullscreen Mode */
    bool focus_mode;                      /* Focus Mode */
    bool compact_mode;                    /* Compact Mode */

    /* Callbacks */
    void (*on_navigate)(struct file_explorer_window *, const char *);
    void (*on_selection_changed)(struct file_explorer_window *);
    void (*on_file_activated)(struct file_explorer_window *, file_info_t *);
    void (*on_context_menu)(struct file_explorer_window *, s32, s32);
    void (*on_tab_changed)(struct file_explorer_window *, u32);
    void (*on_drop)(struct file_explorer_window *, const char **, u32);

    /* User Data */
    void *user_data;

} file_explorer_window_t;

/**
 * File Explorer Application
 */
typedef struct {
    bool initialized;                     /* Is Initialized */
    bool running;                         /* Is Running */
    
    /* Windows */
    file_explorer_window_t *windows;      /* Windows */
    u32 window_count;                     /* Window Count */
    u32 next_window_id;                   /* Next Window ID */
    
    /* Global Settings */
    u32 default_view_mode;                /* Default View Mode */
    u32 default_sort_by;                  /* Default Sort By */
    u32 default_sort_order;               /* Default Sort Order */
    bool show_hidden_default;             /* Show Hidden Default */
    bool show_extensions;                 /* Show Extensions */
    bool show_previews;                   /* Show Previews */
    u32 thumbnail_cache_size;             /* Thumbnail Cache Size */
    
    /* Thumbnail Cache */
    void *thumbnail_cache;                /* Thumbnail Cache */
    u32 cache_count;                      /* Cache Count */
    
    /* File Type Associations */
    void *file_associations;              /* File Associations */
    
    /* Network Locations */
    void *network_locations;              /* Network Locations */
    u32 network_count;                    /* Network Count */
    
    /* Mounted Volumes */
    void *mounted_volumes;                /* Mounted Volumes */
    u32 volume_count;                     /* Volume Count */
    
    /* Clipboard */
    char **global_clipboard;              /* Global Clipboard */
    u32 global_clipboard_count;           /* Global Clipboard Count */
    u32 global_clipboard_op;              /* Global Clipboard Operation */
    
    /* Search History */
    char search_history[MAX_SEARCH_HISTORY][256]; /* Search History */
    u32 search_history_count;             /* Search History Count */
    
    /* Callbacks */
    int (*on_window_created)(file_explorer_window_t *);
    int (*on_window_closed)(file_explorer_window_t *);
    
    /* User Data */
    void *user_data;
    
} file_explorer_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Application lifecycle */
int file_explorer_init(file_explorer_t *explorer);
int file_explorer_run(file_explorer_t *explorer);
int file_explorer_shutdown(file_explorer_t *explorer);
bool file_explorer_is_running(file_explorer_t *explorer);

/* Window management */
file_explorer_window_t *file_explorer_create_window(file_explorer_t *explorer, const char *path);
int file_explorer_close_window(file_explorer_t *explorer, file_explorer_window_t *win);
int file_explorer_open_path(file_explorer_t *explorer, const char *path);
file_explorer_window_t *file_explorer_get_window(file_explorer_t *explorer, u32 window_id);

/* Tab management */
int file_explorer_new_tab(file_explorer_window_t *win, const char *path);
int file_explorer_close_tab(file_explorer_window_t *win, u32 tab_index);
int file_explorer_switch_tab(file_explorer_window_t *win, u32 tab_index);
int file_explorer_duplicate_tab(file_explorer_window_t *win);

/* Dual pane */
int file_explorer_toggle_dual_pane(file_explorer_window_t *win);
int file_explorer_swap_panes(file_explorer_window_t *win);
int file_explorer_copy_to_pane(file_explorer_window_t *win);
int file_explorer_move_to_pane(file_explorer_window_t *win);

/* Navigation */
int file_explorer_navigate(file_explorer_window_t *win, const char *path);
int file_explorer_navigate_up(file_explorer_window_t *win);
int file_explorer_navigate_home(file_explorer_window_t *win);
int file_explorer_navigate_back(file_explorer_window_t *win);
int file_explorer_navigate_forward(file_explorer_window_t *win);
int file_explorer_refresh(file_explorer_window_t *win);
int file_explorer_navigate_tree(file_explorer_window_t *win, const char *path);

/* File operations */
int file_explorer_select_file(file_explorer_window_t *win, const char *path);
int file_explorer_select_all(file_explorer_window_t *win);
int file_explorer_invert_selection(file_explorer_window_t *win);
int file_explorer_open_file(file_explorer_window_t *win, file_info_t *file);
int file_explorer_open_with(file_explorer_window_t *win, file_info_t *file, const char *app);
int file_explorer_delete_file(file_explorer_window_t *win, file_info_t *file);
int file_explorer_delete_permanent(file_explorer_window_t *win, file_info_t *file);
int file_explorer_rename_file(file_explorer_window_t *win, file_info_t *file, const char *new_name);
int file_explorer_copy_file(file_explorer_window_t *win, file_info_t *file, const char *dest_path);
int file_explorer_move_file(file_explorer_window_t *win, file_info_t *file, const char *dest_path);
int file_explorer_create_folder(file_explorer_window_t *win, const char *name);
int file_explorer_create_file(file_explorer_window_t *win, const char *name);
int file_explorer_create_symlink(file_explorer_window_t *win, file_info_t *target, const char *name);
int file_explorer_compress_files(file_explorer_window_t *win, file_info_t **files, u32 count, const char *output);
int file_explorer_extract_archive(file_explorer_window_t *win, file_info_t *archive, const char *dest);

/* Bulk operations */
int file_explorer_bulk_rename(file_explorer_window_t *win, const char *pattern);
int file_explorer_bulk_convert(file_explorer_window_t *win, const char *format);

/* Clipboard operations */
int file_explorer_copy_to_clipboard(file_explorer_t *explorer, file_info_t **files, u32 count, u32 operation);
int file_explorer_paste_from_clipboard(file_explorer_window_t *win, const char *dest_path);
int file_explorer_clear_clipboard(file_explorer_t *explorer);

/* Context menu */
int file_explorer_show_context_menu(file_explorer_window_t *win, s32 x, s32 y, file_info_t *file);
int file_explorer_show_desktop_context_menu(desktop_environment_t *env, s32 x, s32 y);
int file_explorer_add_context_action(const char *name, const char *icon, const char *shortcut, void (*callback)(void *));
int file_explorer_create_submenu(desktop_menu_t *menu, const char *label);

/* Bookmarks */
int file_explorer_add_bookmark(file_explorer_t *explorer, const char *name, const char *path);
int file_explorer_remove_bookmark(file_explorer_t *explorer, u32 bookmark_id);
int file_explorer_navigate_bookmark(file_explorer_window_t *win, u32 bookmark_id);
int file_explorer_edit_bookmark(file_explorer_t *explorer, u32 bookmark_id, const char *name, const char *path);

/* Search */
int file_explorer_search(file_explorer_window_t *win, const char *query);
int file_explorer_advanced_search(file_explorer_window_t *win, const char *query, u32 flags);
int file_explorer_clear_search(file_explorer_window_t *win);
search_result_t *file_explorer_get_search_results(file_explorer_window_t *win, u32 *count);
int file_explorer_save_search(file_explorer_window_t *win, const char *name);
int file_explorer_load_search(file_explorer_window_t *win, const char *name);

/* View settings */
int file_explorer_set_view_mode(file_explorer_window_t *win, u32 mode);
int file_explorer_set_sort(file_explorer_window_t *win, u32 sort_by, u32 sort_order);
int file_explorer_toggle_hidden(file_explorer_window_t *win);
int file_explorer_toggle_preview(file_explorer_window_t *win);
int file_explorer_toggle_tree_view(file_explorer_window_t *win);
int file_explorer_toggle_fullscreen(file_explorer_window_t *win);
int file_explorer_toggle_focus_mode(file_explorer_window_t *win);
int file_explorer_set_column_width(file_explorer_window_t *win, u32 column, u32 width);
int file_explorer_reorder_columns(file_explorer_window_t *win, u32 from, u32 to);

/* Scroll behavior */
int file_explorer_scroll_to(file_explorer_window_t *win, u32 offset);
int file_explorer_scroll_smooth(file_explorer_window_t *win, float delta);
int file_explorer_scroll_to_file(file_explorer_window_t *win, file_info_t *file);
int file_explorer_center_selection(file_explorer_window_t *win);

/* Thumbnail management */
int file_explorer_generate_thumbnail(file_explorer_t *explorer, file_info_t *file);
int file_explorer_clear_thumbnail_cache(file_explorer_t *explorer);
int file_explorer_prefetch_thumbnails(file_explorer_window_t *win);

/* Filter */
int file_explorer_set_filter(file_explorer_window_t *win, u32 category, const char *pattern);
int file_explorer_set_size_filter(file_explorer_window_t *win, u64 min, u64 max);
int file_explorer_set_date_filter(file_explorer_window_t *win, u64 from, u64 to);
int file_explorer_clear_filters(file_explorer_window_t *win);

/* Properties */
int file_explorer_show_properties(file_explorer_window_t *win, file_info_t *file);
int file_explorer_show_folder_size(file_explorer_window_t *win, file_info_t *folder);

/* Network */
int file_explorer_connect_network_location(file_explorer_t *explorer, const char *url);
int file_explorer_disconnect_network_location(file_explorer_t *explorer, const char *url);

/* Utility functions */
const char *file_get_type_name(u32 type);
const char *file_get_category_name(u32 category);
const char *file_get_size_string(u64 size, char *buffer, u32 buffer_size);
const char *file_get_date_string(u64 timestamp, char *buffer, u32 buffer_size);
u32 file_get_icon_for_type(u32 type, u32 category, const char *extension);
int file_compare_paths(const char *path1, const char *path2);
char *file_normalize_path(const char *path, char *normalized, u32 size);

/* Global instance */
file_explorer_t *file_explorer_get_instance(void);

#endif /* _FILE_EXPLORER_H */
