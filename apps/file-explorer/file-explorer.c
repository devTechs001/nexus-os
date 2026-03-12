/*
 * NEXUS OS - File Explorer Application Implementation
 * apps/file-explorer/file-explorer.c
 *
 * Modern file manager with thumbnails, preview, and network support
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "file-explorer.h"
#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../gui/window-manager/window-manager.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL FILE EXPLORER INSTANCE                     */
/*===========================================================================*/

static file_explorer_t g_file_explorer;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* Window creation */
static int create_explorer_window(file_explorer_t *explorer, file_explorer_window_t *win, const char *path);
static int create_toolbar(file_explorer_window_t *win);
static int create_address_bar(file_explorer_window_t *win);
static int create_search_box(file_explorer_window_t *win);
static int create_sidebar(file_explorer_window_t *win);
static int create_file_view(file_explorer_window_t *win);
static int create_status_bar(file_explorer_window_t *win);
static int create_preview_panel(file_explorer_window_t *win);

/* File operations */
static int load_directory(file_explorer_window_t *win, const char *path);
static int sort_entries(file_explorer_window_t *win);
static void free_entries(directory_entry_t *entries);

/* Event handlers */
static void on_back_clicked(widget_t *button);
static void on_forward_clicked(widget_t *button);
static void on_up_clicked(widget_t *button);
static void on_home_clicked(widget_t *button);
static void on_refresh_clicked(widget_t *button);
static void on_address_activate(widget_t *widget, const char *path);
static void on_search_changed(widget_t *widget, const char *query);
static void on_file_double_click(widget_t *widget);
static void on_file_right_click(widget_t *widget, s32 x, s32 y);

/* UI updates */
static int update_file_view(file_explorer_window_t *win);
static int update_status_bar(file_explorer_window_t *win);
static int update_preview(file_explorer_window_t *win);
static int render_file_icon(file_explorer_window_t *win, file_info_t *file, s32 x, s32 y);

/* Utilities */
static void init_default_bookmarks(file_explorer_t *explorer);
static const char *get_file_icon(u32 type, u32 category, const char *ext);

/*===========================================================================*/
/*                         APPLICATION LIFECYCLE                             */
/*===========================================================================*/

/**
 * file_explorer_init - Initialize file explorer application
 * @explorer: Pointer to file explorer structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_init(file_explorer_t *explorer)
{
    if (!explorer) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] ========================================\n");
    printk("[FILE-EXPLORER] NEXUS OS File Explorer v%s\n", FILE_EXPLORER_VERSION);
    printk("[FILE-EXPLORER] ========================================\n");
    printk("[FILE-EXPLORER] Initializing file explorer...\n");
    
    /* Clear structure */
    memset(explorer, 0, sizeof(file_explorer_t));
    explorer->initialized = true;
    explorer->window_count = 0;
    explorer->next_window_id = 1;
    
    /* Default settings */
    explorer->default_view_mode = VIEW_MODE_ICONS_MEDIUM;
    explorer->default_sort_by = SORT_BY_NAME;
    explorer->default_sort_order = SORT_ORDER_ASCENDING;
    explorer->show_hidden_default = false;
    explorer->show_extensions = true;
    explorer->show_previews = true;
    explorer->thumbnail_cache_size = MAX_THUMBNAIL_CACHE;
    
    /* Initialize default bookmarks */
    init_default_bookmarks(explorer);
    
    printk("[FILE-EXPLORER] File explorer initialized\n");
    printk("[FILE-EXPLORER] ========================================\n");
    
    return 0;
}

/**
 * file_explorer_run - Start file explorer application
 * @explorer: Pointer to file explorer structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_run(file_explorer_t *explorer)
{
    if (!explorer || !explorer->initialized) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Starting file explorer...\n");
    
    explorer->running = true;
    
    /* Open default window (home directory) */
    file_explorer_create_window(explorer, "/home");
    
    return 0;
}

/**
 * file_explorer_shutdown - Shutdown file explorer application
 * @explorer: Pointer to file explorer structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_shutdown(file_explorer_t *explorer)
{
    if (!explorer || !explorer->initialized) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Shutting down file explorer...\n");
    
    explorer->running = false;
    
    /* Close all windows */
    while (explorer->windows) {
        file_explorer_close_window(explorer, explorer->windows);
    }
    
    /* Clear clipboard */
    file_explorer_clear_clipboard(explorer);
    
    /* Clear thumbnail cache */
    file_explorer_clear_thumbnail_cache(explorer);
    
    explorer->initialized = false;
    
    printk("[FILE-EXPLORER] File explorer shutdown complete\n");
    
    return 0;
}

/**
 * file_explorer_is_running - Check if file explorer is running
 * @explorer: Pointer to file explorer structure
 *
 * Returns: true if running, false otherwise
 */
bool file_explorer_is_running(file_explorer_t *explorer)
{
    return explorer && explorer->running;
}

/*===========================================================================*/
/*                         WINDOW MANAGEMENT                                 */
/*===========================================================================*/

/**
 * file_explorer_create_window - Create new explorer window
 * @explorer: Pointer to file explorer structure
 * @path: Initial path
 *
 * Returns: Pointer to created window, or NULL on failure
 */
file_explorer_window_t *file_explorer_create_window(file_explorer_t *explorer, const char *path)
{
    if (!explorer) {
        return NULL;
    }
    
    /* Allocate window */
    file_explorer_window_t *win = kzalloc(sizeof(file_explorer_window_t));
    if (!win) {
        return NULL;
    }
    
    /* Initialize window */
    win->window_id = explorer->next_window_id++;
    strncpy(win->current_path, path ? path : "/home", MAX_PATH_LENGTH - 1);
    strncpy(win->home_path, "/home", MAX_PATH_LENGTH - 1);
    
    /* Default view settings */
    win->view_mode = explorer->default_view_mode;
    win->sort_by = explorer->default_sort_by;
    win->sort_order = explorer->default_sort_order;
    win->show_hidden = explorer->show_hidden_default;
    win->show_previews = explorer->show_previews;
    win->icon_size = 48;
    win->group_folders_first = true;
    
    /* Create WM window */
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    snprintf(props.title, sizeof(props.title), "File Explorer");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_RESIZABLE;
    props.bounds.x = 100 + (explorer->window_count * 30);
    props.bounds.y = 100 + (explorer->window_count * 30);
    props.bounds.width = 1000;
    props.bounds.height = 650;
    props.background = color_from_rgba(45, 45, 60, 255);
    
    win->wm_window = wm_create_window(NULL, &props);
    if (!win->wm_window) {
        kfree(win);
        return NULL;
    }
    
    /* Create UI components */
    if (create_explorer_window(explorer, win, path) != 0) {
        wm_destroy_window(NULL, win->wm_window);
        kfree(win);
        return NULL;
    }
    
    /* Add to window list */
    win->next = explorer->windows;
    explorer->windows = win;
    explorer->window_count++;
    
    /* Load initial directory */
    load_directory(win, win->current_path);
    
    /* Call callback */
    if (explorer->on_window_created) {
        explorer->on_window_created(win);
    }
    
    printk("[FILE-EXPLORER] Created window %d: %s\n", win->window_id, win->current_path);
    
    return win;
}

/**
 * create_explorer_window - Create explorer window UI
 * @explorer: Pointer to file explorer structure
 * @win: Window to create UI for
 * @path: Initial path
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_explorer_window(file_explorer_t *explorer, file_explorer_window_t *win, const char *path)
{
    (void)explorer; (void)path;
    
    if (!win || !win->wm_window) {
        return -EINVAL;
    }
    
    /* Create toolbar */
    create_toolbar(win);
    
    /* Create address bar */
    create_address_bar(win);
    
    /* Create search box */
    create_search_box(win);
    
    /* Create sidebar */
    create_sidebar(win);
    
    /* Create file view */
    create_file_view(win);
    
    /* Create status bar */
    create_status_bar(win);
    
    /* Create preview panel (optional) */
    if (win->show_previews) {
        create_preview_panel(win);
    }
    
    return 0;
}

/**
 * create_toolbar - Create toolbar
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int create_toolbar(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create toolbar panel */
    win->toolbar = panel_create(win->wm_window, 0, 0, 1000, 40);
    if (!win->toolbar) {
        return -1;
    }
    
    widget_set_colors(win->toolbar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(50, 50, 70, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Back button */
    widget_t *back_btn = button_create(win->toolbar, "←", 5, 5, 32, 32);
    if (back_btn) {
        back_btn->on_click = on_back_clicked;
        back_btn->user_data = win;
        widget_set_colors(back_btn, color_from_rgba(255,255,255,255),
                          color_from_rgba(60,60,80,255), color_from_rgba(80,80,100,255));
    }
    
    /* Forward button */
    widget_t *forward_btn = button_create(win->toolbar, "→", 40, 5, 32, 32);
    if (forward_btn) {
        forward_btn->on_click = on_forward_clicked;
        forward_btn->user_data = win;
        widget_set_colors(forward_btn, color_from_rgba(255,255,255,255),
                          color_from_rgba(60,60,80,255), color_from_rgba(80,80,100,255));
    }
    
    /* Up button */
    widget_t *up_btn = button_create(win->toolbar, "↑", 75, 5, 32, 32);
    if (up_btn) {
        up_btn->on_click = on_up_clicked;
        up_btn->user_data = win;
        widget_set_colors(up_btn, color_from_rgba(255,255,255,255),
                          color_from_rgba(60,60,80,255), color_from_rgba(80,80,100,255));
    }
    
    /* Home button */
    widget_t *home_btn = button_create(win->toolbar, "🏠", 110, 5, 32, 32);
    if (home_btn) {
        home_btn->on_click = on_home_clicked;
        home_btn->user_data = win;
        widget_set_colors(home_btn, color_from_rgba(255,255,255,255),
                          color_from_rgba(60,60,80,255), color_from_rgba(80,80,100,255));
    }
    
    /* Refresh button */
    widget_t *refresh_btn = button_create(win->toolbar, "⟳", 145, 5, 32, 32);
    if (refresh_btn) {
        refresh_btn->on_click = on_refresh_clicked;
        refresh_btn->user_data = win;
        widget_set_colors(refresh_btn, color_from_rgba(255,255,255,255),
                          color_from_rgba(60,60,80,255), color_from_rgba(80,80,100,255));
    }
    
    return 0;
}

/**
 * create_address_bar - Create address bar
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int create_address_bar(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create address bar edit */
    win->address_bar = edit_create(win->toolbar, win->current_path, 200, 5, 500, 32);
    if (!win->address_bar) {
        return -1;
    }
    
    widget_set_colors(win->address_bar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 55, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Set activate callback */
    /* In real implementation, would set on_activate callback */
    
    return 0;
}

/**
 * create_search_box - Create search box
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int create_search_box(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create search box */
    win->search_box = edit_create(win->toolbar, 720, 5, 250, 32);
    if (!win->search_box) {
        return -1;
    }
    
    widget_set_text(win->search_box, "Search...");
    widget_set_colors(win->search_box,
                      color_from_rgba(200, 200, 200, 255),
                      color_from_rgba(40, 40, 55, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Set text changed callback */
    /* In real implementation, would set on_text_changed callback */
    
    return 0;
}

/**
 * create_sidebar - Create sidebar with bookmarks and navigation
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int create_sidebar(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create sidebar panel */
    win->sidebar = panel_create(win->wm_window, 0, 40, 200, 570);
    if (!win->sidebar) {
        return -1;
    }
    
    widget_set_colors(win->sidebar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 55, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* In real implementation, would add:
     * - Quick access section
     * - Bookmarks section
     * - Network locations
     * - Mounted volumes
     * - Tags section
     */
    
    return 0;
}

/**
 * create_file_view - Create file view area
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int create_file_view(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create file view panel */
    win->file_view = panel_create(win->wm_window, 200, 40, 800, 570);
    if (!win->file_view) {
        return -1;
    }
    
    widget_set_colors(win->file_view,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(45, 45, 60, 255),
                      color_from_rgba(0, 0, 0, 0));
    
    /* In real implementation, would add:
     * - Scrollable area
     * - Grid/list view rendering
     * - Selection handling
     * - Drag & drop support
     */
    
    return 0;
}

/**
 * create_status_bar - Create status bar
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int create_status_bar(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create status bar panel */
    win->status_bar = panel_create(win->wm_window, 0, 610, 1000, 40);
    if (!win->status_bar) {
        return -1;
    }
    
    widget_set_colors(win->status_bar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(35, 35, 50, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* Status label */
    widget_t *status_label = label_create(win->status_bar, "0 items", 10, 10, 200, 20);
    if (!status_label) {
        return -1;
    }
    
    widget_set_font(status_label, 0, 11);
    widget_set_colors(status_label,
                      color_from_rgba(180, 180, 190, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_preview_panel - Create preview panel
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int create_preview_panel(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Create preview panel */
    win->preview_panel = panel_create(win->wm_window, 800, 40, 200, 570);
    if (!win->preview_panel) {
        return -1;
    }
    
    widget_set_colors(win->preview_panel,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 55, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* Initialize preview structure */
    win->preview.visible = true;
    win->preview.preview_type = 0;
    win->preview.current_file = NULL;
    
    return 0;
}

/**
 * file_explorer_close_window - Close explorer window
 * @explorer: Pointer to file explorer structure
 * @win: Window to close
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_close_window(file_explorer_t *explorer, file_explorer_window_t *win)
{
    if (!explorer || !win) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Closing window %d\n", win->window_id);
    
    /* Remove from window list */
    file_explorer_window_t **prev = &explorer->windows;
    file_explorer_window_t *curr = explorer->windows;
    
    while (curr) {
        if (curr == win) {
            *prev = curr->next;
            explorer->window_count--;
            break;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    /* Free entries */
    if (win->entries) {
        free_entries(win->entries);
    }
    
    /* Destroy WM window */
    if (win->wm_window) {
        wm_destroy_window(NULL, win->wm_window);
    }
    
    /* Free window */
    kfree(win);
    
    if (explorer->on_window_closed) {
        explorer->on_window_closed(win);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         NAVIGATION                                        */
/*===========================================================================*/

/**
 * file_explorer_navigate - Navigate to path
 * @win: Explorer window
 * @path: Path to navigate to
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_navigate(file_explorer_window_t *win, const char *path)
{
    if (!win || !path) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Navigating to: %s\n", path);
    
    /* Add to history */
    if (win->history_index < 99) {
        win->history_index++;
    }
    strncpy(win->history[win->history_index].path, path, MAX_PATH_LENGTH - 1);
    win->history[win->history_index].timestamp = get_time_ms();
    win->history_count = win->history_index + 1;
    
    /* Update current path */
    strncpy(win->current_path, path, MAX_PATH_LENGTH - 1);
    
    /* Update address bar */
    if (win->address_bar) {
        widget_set_text(win->address_bar, path);
    }
    
    /* Load directory */
    load_directory(win, path);
    
    /* Call callback */
    if (win->on_navigate) {
        win->on_navigate(win, path);
    }
    
    return 0;
}

/**
 * file_explorer_navigate_up - Navigate to parent directory
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_navigate_up(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Simple parent path calculation */
    char parent_path[MAX_PATH_LENGTH];
    strncpy(parent_path, win->current_path, MAX_PATH_LENGTH - 1);
    
    /* Remove trailing slash */
    u32 len = strlen(parent_path);
    if (len > 0 && parent_path[len-1] == '/') {
        parent_path[len-1] = '\0';
        len--;
    }
    
    /* Find last slash */
    for (s32 i = len - 1; i >= 0; i--) {
        if (parent_path[i] == '/') {
            if (i == 0) {
                parent_path[1] = '\0';  /* Root */
            } else {
                parent_path[i] = '\0';
            }
            break;
        }
    }
    
    return file_explorer_navigate(win, parent_path);
}

/**
 * file_explorer_navigate_home - Navigate to home directory
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_navigate_home(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    return file_explorer_navigate(win, win->home_path);
}

/**
 * file_explorer_navigate_back - Navigate back in history
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_navigate_back(file_explorer_window_t *win)
{
    if (!win || win->history_index == 0) {
        return -EINVAL;
    }
    
    win->history_index--;
    return file_explorer_navigate(win, win->history[win->history_index].path);
}

/**
 * file_explorer_navigate_forward - Navigate forward in history
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_navigate_forward(file_explorer_window_t *win)
{
    if (!win || win->history_index >= win->history_count - 1) {
        return -EINVAL;
    }
    
    win->history_index++;
    return file_explorer_navigate(win, win->history[win->history_index].path);
}

/**
 * file_explorer_refresh - Refresh current directory
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_refresh(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Refreshing: %s\n", win->current_path);
    
    /* Reload directory */
    load_directory(win, win->current_path);
    
    return 0;
}

/*===========================================================================*/
/*                         FILE OPERATIONS                                   */
/*===========================================================================*/

/**
 * load_directory - Load directory contents
 * @win: Explorer window
 * @path: Directory path
 *
 * Returns: 0 on success, negative error code on failure
 */
static int load_directory(file_explorer_window_t *win, const char *path)
{
    if (!win || !path) {
        return -EINVAL;
    }
    
    /* Free existing entries */
    if (win->entries) {
        free_entries(win->entries);
        win->entries = NULL;
        win->entry_count = 0;
    }
    
    /* In real implementation, would:
     * - Open directory
     * - Read entries
     * - Get file info (size, dates, permissions)
     * - Generate thumbnails for images
     * - Apply filters
     * - Sort entries
     */
    
    /* Mock entries for demonstration */
    directory_entry_t *entries = NULL;
    u32 count = 0;
    
    /* Add mock entries */
    const char *mock_folders[] = {"Documents", "Downloads", "Pictures", "Music", "Videos", NULL};
    const char *mock_files[] = {"readme.txt", "config.json", "data.csv", NULL};
    
    /* Add folders */
    for (u32 i = 0; mock_folders[i] != NULL; i++) {
        directory_entry_t *entry = kzalloc(sizeof(directory_entry_t));
        if (!entry) continue;
        
        strncpy(entry->file.name, mock_folders[i], MAX_FILENAME_LENGTH - 1);
        snprintf(entry->file.path, MAX_PATH_LENGTH, "%s/%s", path, mock_folders[i]);
        entry->file.file_type = FILE_TYPE_FOLDER;
        entry->file.file_category = FILE_CATEGORY_ALL;
        entry->file.size = 0;
        entry->file.modified_time = get_time_ms();
        strcpy(entry->file.icon_path, "/usr/share/icons/folder.png");
        
        entry->next = entries;
        entries = entry;
        count++;
    }
    
    /* Add files */
    for (u32 i = 0; mock_files[i] != NULL; i++) {
        directory_entry_t *entry = kzalloc(sizeof(directory_entry_t));
        if (!entry) continue;
        
        strncpy(entry->file.name, mock_files[i], MAX_FILENAME_LENGTH - 1);
        snprintf(entry->file.path, MAX_PATH_LENGTH, "%s/%s", path, mock_files[i]);
        entry->file.file_type = FILE_TYPE_FILE;
        
        /* Determine category by extension */
        const char *ext = strrchr(mock_files[i], '.');
        if (ext) {
            strncpy(entry->file.extension, ext + 1, sizeof(entry->file.extension) - 1);
            if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".json") == 0) {
                entry->file.file_category = FILE_CATEGORY_DOCUMENTS;
            } else if (strcmp(ext, ".csv") == 0) {
                entry->file.file_category = FILE_CATEGORY_DOCUMENTS;
            }
        }
        
        entry->file.size = 1024 * (i + 1);
        entry->file.modified_time = get_time_ms();
        strcpy(entry->file.icon_path, "/usr/share/icons/file.png");
        
        entry->next = entries;
        entries = entry;
        count++;
    }
    
    win->entries = entries;
    win->entry_count = count;
    
    /* Sort entries */
    sort_entries(win);
    
    /* Update UI */
    update_file_view(win);
    update_status_bar(win);
    
    printk("[FILE-EXPLORER] Loaded %d entries from %s\n", count, path);
    
    return 0;
}

/**
 * sort_entries - Sort directory entries
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int sort_entries(file_explorer_window_t *win)
{
    if (!win || !win->entries) {
        return 0;
    }
    
    /* Simple bubble sort for demonstration */
    bool swapped;
    do {
        swapped = false;
        directory_entry_t *prev = NULL;
        directory_entry_t *curr = win->entries;
        
        while (curr && curr->next) {
            directory_entry_t *next = curr->next;
            bool should_swap = false;
            
            /* Group folders first */
            if (win->group_folders_first) {
                if (curr->file.file_type != FILE_TYPE_FOLDER && 
                    next->file.file_type == FILE_TYPE_FOLDER) {
                    should_swap = true;
                } else if (curr->file.file_type == next->file.file_type) {
                    /* Same type, sort by name */
                    if (strcmp(curr->file.name, next->file.name) > 0) {
                        should_swap = true;
                    }
                }
            } else {
                /* Sort all by name */
                if (strcmp(curr->file.name, next->file.name) > 0) {
                    should_swap = true;
                }
            }
            
            if (should_swap) {
                /* Swap entries */
                if (prev) {
                    prev->next = next;
                } else {
                    win->entries = next;
                }
                curr->next = next->next;
                next->next = curr;
                swapped = true;
            }
            
            prev = curr;
            curr = (swapped) ? next : curr->next;
        }
    } while (swapped);
    
    return 0;
}

/**
 * free_entries - Free directory entries
 * @entries: Entries to free
 */
static void free_entries(directory_entry_t *entries)
{
    directory_entry_t *curr = entries;
    while (curr) {
        directory_entry_t *next = curr->next;
        if (curr->file.thumbnail) {
            kfree(curr->file.thumbnail);
        }
        kfree(curr);
        curr = next;
    }
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

static void on_back_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    file_explorer_window_t *win = (file_explorer_window_t *)button->user_data;
    file_explorer_navigate_back(win);
}

static void on_forward_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    file_explorer_window_t *win = (file_explorer_window_t *)button->user_data;
    file_explorer_navigate_forward(win);
}

static void on_up_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    file_explorer_window_t *win = (file_explorer_window_t *)button->user_data;
    file_explorer_navigate_up(win);
}

static void on_home_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    file_explorer_window_t *win = (file_explorer_window_t *)button->user_data;
    file_explorer_navigate_home(win);
}

static void on_refresh_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    file_explorer_window_t *win = (file_explorer_window_t *)button->user_data;
    file_explorer_refresh(win);
}

static void on_address_activate(widget_t *widget, const char *path)
{
    if (!widget || !widget->user_data || !path) return;
    file_explorer_window_t *win = (file_explorer_window_t *)widget->user_data;
    file_explorer_navigate(win, path);
}

static void on_search_changed(widget_t *widget, const char *query)
{
    if (!widget || !widget->user_data) return;
    file_explorer_window_t *win = (file_explorer_window_t *)widget->user_data;
    if (strlen(query) > 2) {
        file_explorer_search(win, query);
    }
}

static void on_file_double_click(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    /* Handle file activation */
}

static void on_file_right_click(widget_t *widget, s32 x, s32 y)
{
    if (!widget || !widget->user_data) return;
    file_explorer_window_t *win = (file_explorer_window_t *)widget->user_data;
    if (win->on_context_menu) {
        win->on_context_menu(win, x, y);
    }
}

/*===========================================================================*/
/*                         UI UPDATES                                        */
/*===========================================================================*/

/**
 * update_file_view - Update file view
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int update_file_view(file_explorer_window_t *win)
{
    if (!win || !win->file_view) {
        return -EINVAL;
    }
    
    /* In real implementation, would render file icons/list */
    /* For now, just log */
    
    return 0;
}

/**
 * update_status_bar - Update status bar
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int update_status_bar(file_explorer_window_t *win)
{
    if (!win || !win->status_bar) {
        return -EINVAL;
    }
    
    /* Update item count */
    char status_text[64];
    snprintf(status_text, sizeof(status_text), "%d items", win->entry_count);
    
    if (win->selected_count > 0) {
        char size_str[32];
        file_get_size_string(0, size_str, sizeof(size_str));  /* Would calculate total */
        snprintf(status_text, sizeof(status_text), "%d items selected (%s)", 
                 win->selected_count, size_str);
    }
    
    /* Update status label */
    /* In real implementation, would update label text */
    
    return 0;
}

/**
 * update_preview - Update preview panel
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
static int update_preview(file_explorer_window_t *win)
{
    if (!win || !win->preview_panel) {
        return 0;
    }
    
    /* Update preview based on selected file */
    /* In real implementation, would render preview */
    
    return 0;
}

/*===========================================================================*/
/*                         BOOKMARKS                                         */
/*===========================================================================*/

/**
 * init_default_bookmarks - Initialize default bookmarks
 * @explorer: Pointer to file explorer structure
 */
static void init_default_bookmarks(file_explorer_t *explorer)
{
    if (!explorer) return;
    
    const char *default_bookmarks[][2] = {
        {"Home", "/home"},
        {"Desktop", "/home/Desktop"},
        {"Documents", "/home/Documents"},
        {"Downloads", "/home/Downloads"},
        {"Pictures", "/home/Pictures"},
        {"Music", "/home/Music"},
        {"Videos", "/home/Videos"},
        {"Root", "/"},
        {NULL, NULL}
    };
    
    for (u32 i = 0; default_bookmarks[i][0] != NULL; i++) {
        if (explorer->bookmark_count < MAX_BOOKMARKS) {
            bookmark_entry_t *bookmark = &explorer->bookmarks[explorer->bookmark_count++];
            bookmark->bookmark_id = explorer->bookmark_count;
            strncpy(bookmark->name, default_bookmarks[i][0], sizeof(bookmark->name) - 1);
            strncpy(bookmark->path, default_bookmarks[i][1], sizeof(bookmark->path) - 1);
            bookmark->is_system = true;
        }
    }
}

/**
 * file_explorer_add_bookmark - Add bookmark
 * @explorer: Pointer to file explorer structure
 * @name: Bookmark name
 * @path: Bookmark path
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_add_bookmark(file_explorer_t *explorer, const char *name, const char *path)
{
    if (!explorer || !name || !path) {
        return -EINVAL;
    }
    
    if (explorer->bookmark_count >= MAX_BOOKMARKS) {
        return -ENOSPC;
    }
    
    bookmark_entry_t *bookmark = &explorer->bookmarks[explorer->bookmark_count];
    bookmark->bookmark_id = explorer->bookmark_count + 1;
    strncpy(bookmark->name, name, sizeof(bookmark->name) - 1);
    strncpy(bookmark->path, path, sizeof(bookmark->path) - 1);
    bookmark->is_system = false;
    
    explorer->bookmark_count++;
    
    return 0;
}

/**
 * file_explorer_remove_bookmark - Remove bookmark
 * @explorer: Pointer to file explorer structure
 * @bookmark_id: Bookmark ID to remove
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_remove_bookmark(file_explorer_t *explorer, u32 bookmark_id)
{
    if (!explorer || bookmark_id == 0) {
        return -EINVAL;
    }
    
    for (u32 i = 0; i < explorer->bookmark_count; i++) {
        if (explorer->bookmarks[i].bookmark_id == bookmark_id) {
            if (explorer->bookmarks[i].is_system) {
                return -EPERM;  /* Cannot remove system bookmarks */
            }
            
            /* Shift remaining bookmarks */
            for (u32 j = i; j < explorer->bookmark_count - 1; j++) {
                explorer->bookmarks[j] = explorer->bookmarks[j + 1];
            }
            explorer->bookmark_count--;
            return 0;
        }
    }
    
    return -ENOENT;
}

/**
 * file_explorer_navigate_bookmark - Navigate to bookmark
 * @win: Explorer window
 * @bookmark_id: Bookmark ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_navigate_bookmark(file_explorer_window_t *win, u32 bookmark_id)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Find bookmark in global explorer */
    file_explorer_t *explorer = &g_file_explorer;
    for (u32 i = 0; i < explorer->bookmark_count; i++) {
        if (explorer->bookmarks[i].bookmark_id == bookmark_id) {
            return file_explorer_navigate(win, explorer->bookmarks[i].path);
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         SEARCH                                            */
/*===========================================================================*/

/**
 * file_explorer_search - Search for files
 * @win: Explorer window
 * @query: Search query
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_search(file_explorer_window_t *win, const char *query)
{
    if (!win || !query) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Searching for: %s\n", query);
    
    win->is_searching = true;
    strncpy(win->search_query, query, sizeof(win->search_query) - 1);
    
    /* In real implementation, would:
     * - Search current directory and subdirectories
     * - Match against file names
     * - Calculate relevance scores
     * - Sort by relevance
     * - Display results
     */
    
    /* Mock search results */
    win->search_result_count = 0;
    
    return 0;
}

/**
 * file_explorer_clear_search - Clear search
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_clear_search(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->is_searching = false;
    win->search_query[0] = '\0';
    win->search_result_count = 0;
    
    /* Reload current directory */
    load_directory(win, win->current_path);
    
    return 0;
}

/**
 * file_explorer_get_search_results - Get search results
 * @win: Explorer window
 * @count: Pointer to store result count
 *
 * Returns: Search results array, or NULL
 */
search_result_t *file_explorer_get_search_results(file_explorer_window_t *win, u32 *count)
{
    if (!win || !count) {
        return NULL;
    }
    
    *count = win->search_result_count;
    return win->search_results;
}

/*===========================================================================*/
/*                         VIEW SETTINGS                                     */
/*===========================================================================*/

/**
 * file_explorer_set_view_mode - Set view mode
 * @win: Explorer window
 * @mode: View mode
 *
 * Returns: 0 on success
 */
int file_explorer_set_view_mode(file_explorer_window_t *win, u32 mode)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->view_mode = mode;
    
    /* Update icon size based on mode */
    switch (mode) {
        case VIEW_MODE_ICONS_LARGE:
            win->icon_size = 64;
            break;
        case VIEW_MODE_ICONS_MEDIUM:
            win->icon_size = 48;
            break;
        case VIEW_MODE_ICONS_SMALL:
            win->icon_size = 32;
            break;
        default:
            break;
    }
    
    update_file_view(win);
    
    return 0;
}

/**
 * file_explorer_set_sort - Set sort options
 * @win: Explorer window
 * @sort_by: Sort by option
 * @sort_order: Sort order
 *
 * Returns: 0 on success
 */
int file_explorer_set_sort(file_explorer_window_t *win, u32 sort_by, u32 sort_order)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->sort_by = sort_by;
    win->sort_order = sort_order;
    
    sort_entries(win);
    update_file_view(win);
    
    return 0;
}

/**
 * file_explorer_toggle_hidden - Toggle hidden files
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_toggle_hidden(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->show_hidden = !win->show_hidden;
    load_directory(win, win->current_path);
    
    return 0;
}

/**
 * file_explorer_toggle_preview - Toggle preview panel
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_toggle_preview(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->show_previews = !win->show_previews;
    
    if (win->show_previews && !win->preview_panel) {
        create_preview_panel(win);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         THUMBNAIL MANAGEMENT                              */
/*===========================================================================*/

/**
 * file_explorer_generate_thumbnail - Generate thumbnail for file
 * @explorer: Pointer to file explorer structure
 * @file: File info
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_generate_thumbnail(file_explorer_t *explorer, file_info_t *file)
{
    (void)explorer; (void)file;
    
    /* In real implementation, would:
     * - Check file type (images, videos, documents)
     * - Generate appropriate thumbnail
     * - Cache thumbnail
     * - Return thumbnail data
     */
    
    return 0;
}

/**
 * file_explorer_clear_thumbnail_cache - Clear thumbnail cache
 * @explorer: Pointer to file explorer structure
 *
 * Returns: 0 on success
 */
int file_explorer_clear_thumbnail_cache(file_explorer_t *explorer)
{
    if (!explorer) {
        return -EINVAL;
    }
    
    /* In real implementation, would free all cached thumbnails */
    
    explorer->cache_count = 0;
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * file_get_type_name - Get file type name
 * @type: File type
 *
 * Returns: Type name string
 */
const char *file_get_type_name(u32 type)
{
    switch (type) {
        case FILE_TYPE_FILE:      return "File";
        case FILE_TYPE_FOLDER:    return "Folder";
        case FILE_TYPE_SYMLINK:   return "Symbolic Link";
        case FILE_TYPE_DEVICE:    return "Device";
        case FILE_TYPE_MOUNT:     return "Mount Point";
        default:                  return "Unknown";
    }
}

/**
 * file_get_category_name - Get file category name
 * @category: File category
 *
 * Returns: Category name string
 */
const char *file_get_category_name(u32 category)
{
    switch (category) {
        case FILE_CATEGORY_ALL:         return "All";
        case FILE_CATEGORY_DOCUMENTS:   return "Documents";
        case FILE_CATEGORY_IMAGES:      return "Images";
        case FILE_CATEGORY_AUDIO:       return "Audio";
        case FILE_CATEGORY_VIDEO:       return "Video";
        case FILE_CATEGORY_ARCHIVES:    return "Archives";
        case FILE_CATEGORY_CODE:        return "Code";
        case FILE_CATEGORY_SYSTEM:      return "System";
        default:                        return "Unknown";
    }
}

/**
 * file_get_size_string - Format file size
 * @size: Size in bytes
 * @buffer: Buffer to store formatted string
 * @buffer_size: Buffer size
 *
 * Returns: Formatted string pointer
 */
const char *file_get_size_string(u64 size, char *buffer, u32 buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return NULL;
    }
    
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    u32 unit_index = 0;
    double size_d = (double)size;
    
    while (size_d >= 1024.0 && unit_index < 4) {
        size_d /= 1024.0;
        unit_index++;
    }
    
    snprintf(buffer, buffer_size, "%.1f %s", size_d, units[unit_index]);
    
    return buffer;
}

/**
 * file_get_date_string - Format date
 * @timestamp: Timestamp
 * @buffer: Buffer to store formatted string
 * @buffer_size: Buffer size
 *
 * Returns: Formatted string pointer
 */
const char *file_get_date_string(u64 timestamp, char *buffer, u32 buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return NULL;
    }
    
    /* In real implementation, would format timestamp */
    snprintf(buffer, buffer_size, "%d", (u32)timestamp);
    
    return buffer;
}

/**
 * get_file_icon - Get icon for file type
 * @type: File type
 * @category: File category
 * @ext: File extension
 *
 * Returns: Icon path string
 */
static const char *get_file_icon(u32 type, u32 category, const char *ext)
{
    (void)category; (void)ext;
    
    switch (type) {
        case FILE_TYPE_FOLDER:  return "/usr/share/icons/folder.png";
        case FILE_TYPE_FILE:    return "/usr/share/icons/file.png";
        case FILE_TYPE_SYMLINK: return "/usr/share/icons/symlink.png";
        default:                return "/usr/share/icons/unknown.png";
    }
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * file_explorer_get_instance - Get global file explorer instance
 *
 * Returns: Pointer to global instance
 */
file_explorer_t *file_explorer_get_instance(void)
{
    return &g_file_explorer;
}
