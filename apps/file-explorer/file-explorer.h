/*
 * NEXUS OS - File Explorer Application
 * apps/file-explorer/file-explorer.h
 *
 * Modern file manager with thumbnails, preview, and network support
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

#define FILE_EXPLORER_VERSION       "1.0.0"
#define MAX_PATH_LENGTH             4096
#define MAX_FILENAME_LENGTH         256
#define MAX_PREVIEW_SIZE            1024 * 1024
#define MAX_THUMBNAIL_CACHE         1000
#define MAX_BOOKMARKS               32
#define MAX_RECENT_FOLDERS          20
#define MAX_SEARCH_HISTORY          50

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
    
    /* View Settings */
    u32 view_mode;                        /* View Mode */
    u32 sort_by;                          /* Sort By */
    u32 sort_order;                       /* Sort Order */
    bool show_hidden;                     /* Show Hidden */
    bool group_folders_first;             /* Group Folders First */
    u32 icon_size;                        /* Icon Size */
    bool show_previews;                   /* Show Previews */
    
    /* Filter */
    u32 filter_category;                  /* Filter Category */
    char filter_pattern[64];              /* Filter Pattern */
    
    /* UI Components */
    widget_t *toolbar;                    /* Toolbar */
    widget_t *address_bar;                /* Address Bar */
    widget_t *search_box;                 /* Search Box */
    widget_t *sidebar;                    /* Sidebar */
    widget_t *file_view;                  /* File View */
    widget_t *status_bar;                 /* Status Bar */
    widget_t *preview_panel;              /* Preview Panel */
    widget_t *bookmarks_panel;            /* Bookmarks Panel */
    
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
    
    /* Preview */
    preview_panel_t preview;              /* Preview Panel */
    
    /* Clipboard */
    char **clipboard_files;               /* Clipboard Files */
    u32 clipboard_count;                  /* Clipboard Count */
    u32 clipboard_operation;              /* Clipboard Operation (0=none, 1=copy, 2=cut) */
    
    /* Drag & Drop */
    bool is_dragging;                     /* Is Dragging */
    directory_entry_t *drag_source;       /* Drag Source */
    
    /* Progress */
    bool has_progress;                    /* Has Progress */
    char progress_title[128];             /* Progress Title */
    char progress_message[256];           /* Progress Message */
    u32 progress_current;                 /* Progress Current */
    u32 progress_total;                   /* Progress Total */
    
    /* Callbacks */
    void (*on_navigate)(struct file_explorer_window *, const char *);
    void (*on_selection_changed)(struct file_explorer_window *);
    void (*on_file_activated)(struct file_explorer_window *, file_info_t *);
    void (*on_context_menu)(struct file_explorer_window *, s32, s32);
    
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

/* Navigation */
int file_explorer_navigate(file_explorer_window_t *win, const char *path);
int file_explorer_navigate_up(file_explorer_window_t *win);
int file_explorer_navigate_home(file_explorer_window_t *win);
int file_explorer_navigate_back(file_explorer_window_t *win);
int file_explorer_navigate_forward(file_explorer_window_t *win);
int file_explorer_refresh(file_explorer_window_t *win);

/* File operations */
int file_explorer_select_file(file_explorer_window_t *win, const char *path);
int file_explorer_open_file(file_explorer_window_t *win, file_info_t *file);
int file_explorer_delete_file(file_explorer_window_t *win, file_info_t *file);
int file_explorer_rename_file(file_explorer_window_t *win, file_info_t *file, const char *new_name);
int file_explorer_copy_file(file_explorer_window_t *win, file_info_t *file, const char *dest_path);
int file_explorer_move_file(file_explorer_window_t *win, file_info_t *file, const char *dest_path);
int file_explorer_create_folder(file_explorer_window_t *win, const char *name);
int file_explorer_create_file(file_explorer_window_t *win, const char *name);

/* Clipboard operations */
int file_explorer_copy_to_clipboard(file_explorer_t *explorer, file_info_t **files, u32 count, u32 operation);
int file_explorer_paste_from_clipboard(file_explorer_window_t *win, const char *dest_path);
int file_explorer_clear_clipboard(file_explorer_t *explorer);

/* Bookmarks */
int file_explorer_add_bookmark(file_explorer_t *explorer, const char *name, const char *path);
int file_explorer_remove_bookmark(file_explorer_t *explorer, u32 bookmark_id);
int file_explorer_navigate_bookmark(file_explorer_window_t *win, u32 bookmark_id);

/* Search */
int file_explorer_search(file_explorer_window_t *win, const char *query);
int file_explorer_clear_search(file_explorer_window_t *win);
search_result_t *file_explorer_get_search_results(file_explorer_window_t *win, u32 *count);

/* View settings */
int file_explorer_set_view_mode(file_explorer_window_t *win, u32 mode);
int file_explorer_set_sort(file_explorer_window_t *win, u32 sort_by, u32 sort_order);
int file_explorer_toggle_hidden(file_explorer_window_t *win);
int file_explorer_toggle_preview(file_explorer_window_t *win);

/* Thumbnail management */
int file_explorer_generate_thumbnail(file_explorer_t *explorer, file_info_t *file);
int file_explorer_clear_thumbnail_cache(file_explorer_t *explorer);

/* Utility functions */
const char *file_get_type_name(u32 type);
const char *file_get_category_name(u32 category);
const char *file_get_size_string(u64 size, char *buffer, u32 buffer_size);
const char *file_get_date_string(u64 timestamp, char *buffer, u32 buffer_size);
u32 file_get_icon_for_type(u32 type, u32 category, const char *extension);

/* Global instance */
file_explorer_t *file_explorer_get_instance(void);

#endif /* _FILE_EXPLORER_H */
