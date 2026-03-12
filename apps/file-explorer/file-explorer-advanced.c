/*
 * NEXUS OS - Advanced File Explorer Implementation
 * apps/file-explorer/file-explorer-advanced.c
 *
 * Modern, powerful file manager with advanced features
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "file-explorer.h"
#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../gui/window-manager/window-manager.h"
#include "../../gui/desktop-environment/desktop-env.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         TAB MANAGEMENT                                    */
/*===========================================================================*/

/**
 * file_explorer_new_tab - Create new tab
 * @win: Explorer window
 * @path: Initial path
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_new_tab(file_explorer_window_t *win, const char *path)
{
    if (!win || win->tab_count >= MAX_TABS) {
        return win->tab_count >= MAX_TABS ? -ENOSPC : -EINVAL;
    }
    
    strncpy(win->tab_paths[win->tab_count], path, MAX_PATH_LENGTH - 1);
    win->tab_count++;
    
    /* Switch to new tab */
    win->active_tab = win->tab_count - 1;
    
    /* Navigate to path */
    file_explorer_navigate(win, path);
    
    /* Update tab bar */
    if (win->tab_bar) {
        /* In real implementation, would update tab bar UI */
    }
    
    if (win->on_tab_changed) {
        win->on_tab_changed(win, win->active_tab);
    }
    
    return 0;
}

/**
 * file_explorer_close_tab - Close tab
 * @win: Explorer window
 * @tab_index: Tab index
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_close_tab(file_explorer_window_t *win, u32 tab_index)
{
    if (!win || tab_index >= win->tab_count) {
        return -EINVAL;
    }
    
    /* Can't close last tab */
    if (win->tab_count <= 1) {
        return -EPERM;
    }
    
    /* Shift tabs */
    for (u32 i = tab_index; i < win->tab_count - 1; i++) {
        strncpy(win->tab_paths[i], win->tab_paths[i + 1], MAX_PATH_LENGTH - 1);
    }
    win->tab_count--;
    
    /* Adjust active tab */
    if (win->active_tab >= win->tab_count) {
        win->active_tab = win->tab_count - 1;
    }
    
    /* Navigate to active tab's path */
    file_explorer_navigate(win, win->tab_paths[win->active_tab]);
    
    if (win->on_tab_changed) {
        win->on_tab_changed(win, win->active_tab);
    }
    
    return 0;
}

/**
 * file_explorer_switch_tab - Switch to tab
 * @win: Explorer window
 * @tab_index: Tab index
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_switch_tab(file_explorer_window_t *win, u32 tab_index)
{
    if (!win || tab_index >= win->tab_count) {
        return -EINVAL;
    }
    
    win->active_tab = tab_index;
    file_explorer_navigate(win, win->tab_paths[tab_index]);
    
    if (win->on_tab_changed) {
        win->on_tab_changed(win, tab_index);
    }
    
    return 0;
}

/**
 * file_explorer_duplicate_tab - Duplicate current tab
 * @win: Explorer window
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_duplicate_tab(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    return file_explorer_new_tab(win, win->current_path);
}

/*===========================================================================*/
/*                         DUAL PANE MODE                                    */
/*===========================================================================*/

/**
 * file_explorer_toggle_dual_pane - Toggle dual pane mode
 * @win: Explorer window
 *
 * Returns: 0 on success, negative error code on failure
 */
int file_explorer_toggle_dual_pane(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->dual_pane_mode = !win->dual_pane_mode;
    
    if (win->dual_pane_mode) {
        /* Create second pane */
        /* In real implementation, would create second file view */
    }
    
    return 0;
}

/**
 * file_explorer_swap_panes - Swap panes in dual pane mode
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_swap_panes(file_explorer_window_t *win)
{
    if (!win || !win->dual_pane_mode) {
        return -EINVAL;
    }
    
    /* Swap paths between panes */
    /* In real implementation, would swap pane contents */
    
    return 0;
}

/**
 * file_explorer_copy_to_pane - Copy selected files to other pane
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_copy_to_pane(file_explorer_window_t *win)
{
    if (!win || !win->dual_pane_mode || !win->paired_window) {
        return -EINVAL;
    }
    
    /* Copy selected files to paired window's path */
    for (u32 i = 0; i < win->selected_count; i++) {
        file_explorer_copy_file(win, win->selected[i], win->paired_window->current_path);
    }
    
    return 0;
}

/**
 * file_explorer_move_to_pane - Move selected files to other pane
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_move_to_pane(file_explorer_window_t *win)
{
    if (!win || !win->dual_pane_mode || !win->paired_window) {
        return -EINVAL;
    }
    
    /* Move selected files to paired window's path */
    for (u32 i = 0; i < win->selected_count; i++) {
        file_explorer_move_file(win, win->selected[i], win->paired_window->current_path);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         ADVANCED SELECTION                                */
/*===========================================================================*/

/**
 * file_explorer_select_all - Select all files
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_select_all(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Select all entries */
    win->selected_count = win->entry_count;
    
    /* In real implementation, would mark all as selected */
    
    if (win->on_selection_changed) {
        win->on_selection_changed(win);
    }
    
    update_status_bar(win);
    
    return 0;
}

/**
 * file_explorer_invert_selection - Invert selection
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_invert_selection(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* Invert selection for all entries */
    /* In real implementation, would toggle selection state */
    
    if (win->on_selection_changed) {
        win->on_selection_changed(win);
    }
    
    update_status_bar(win);
    
    return 0;
}

/**
 * file_explorer_open_with - Open file with specific application
 * @win: Explorer window
 * @file: File to open
 * @app: Application path
 *
 * Returns: 0 on success
 */
int file_explorer_open_with(file_explorer_window_t *win, file_info_t *file, const char *app)
{
    if (!win || !file || !app) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Opening %s with %s\n", file->path, app);
    
    /* In real implementation, would launch application with file */
    
    return 0;
}

/**
 * file_explorer_delete_permanent - Delete file permanently (skip trash)
 * @win: Explorer window
 * @file: File to delete
 *
 * Returns: 0 on success
 */
int file_explorer_delete_permanent(file_explorer_window_t *win, file_info_t *file)
{
    if (!win || !file) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Permanently deleting: %s\n", file->path);
    
    /* In real implementation, would permanently delete file */
    
    return 0;
}

/*===========================================================================*/
/*                         ADVANCED FILE OPERATIONS                          */
/*===========================================================================*/

/**
 * file_explorer_create_symlink - Create symbolic link
 * @win: Explorer window
 * @target: Target file/folder
 * @name: Link name
 *
 * Returns: 0 on success
 */
int file_explorer_create_symlink(file_explorer_window_t *win, file_info_t *target, const char *name)
{
    if (!win || !target || !name) {
        return -EINVAL;
    }
    
    char link_path[MAX_PATH_LENGTH];
    snprintf(link_path, sizeof(link_path), "%s/%s", win->current_path, name);
    
    printk("[FILE-EXPLORER] Creating symlink: %s -> %s\n", link_path, target->path);
    
    /* In real implementation, would create symbolic link */
    
    return 0;
}

/**
 * file_explorer_compress_files - Compress files to archive
 * @win: Explorer window
 * @files: Files to compress
 * @count: File count
 * @output: Output archive path
 *
 * Returns: 0 on success
 */
int file_explorer_compress_files(file_explorer_window_t *win, file_info_t **files, u32 count, const char *output)
{
    if (!win || !files || count == 0 || !output) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Compressing %d files to %s\n", count, output);
    
    /* In real implementation, would create archive */
    
    return 0;
}

/**
 * file_explorer_extract_archive - Extract archive
 * @win: Explorer window
 * @archive: Archive file
 * @dest: Destination path
 *
 * Returns: 0 on success
 */
int file_explorer_extract_archive(file_explorer_window_t *win, file_info_t *archive, const char *dest)
{
    if (!win || !archive || !dest) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Extracting %s to %s\n", archive->path, dest);
    
    /* In real implementation, would extract archive */
    
    return 0;
}

/*===========================================================================*/
/*                         BULK OPERATIONS                                   */
/*===========================================================================*/

/**
 * file_explorer_bulk_rename - Bulk rename files
 * @win: Explorer window
 * @pattern: Rename pattern (e.g., "IMG_###.jpg")
 *
 * Returns: 0 on success
 */
int file_explorer_bulk_rename(file_explorer_window_t *win, const char *pattern)
{
    if (!win || !pattern) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Bulk renaming %d files with pattern: %s\n", 
           win->selected_count, pattern);
    
    /* In real implementation, would rename files according to pattern */
    /* Support for:
     * - Sequential numbering (###)
     * - Date/time stamps
     * - Original name preservation
     * - Case conversion
     * - Find/replace
     */
    
    return 0;
}

/**
 * file_explorer_bulk_convert - Bulk convert files
 * @win: Explorer window
 * @format: Target format
 *
 * Returns: 0 on success
 */
int file_explorer_bulk_convert(file_explorer_window_t *win, const char *format)
{
    if (!win || !format) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Bulk converting %d files to %s\n", 
           win->selected_count, format);
    
    /* In real implementation, would convert files (e.g., images) */
    
    return 0;
}

/*===========================================================================*/
/*                         CONTEXT MENU                                      */
/*===========================================================================*/

/**
 * file_explorer_show_context_menu - Show context menu
 * @win: Explorer window
 * @x: X position
 * @y: Y position
 * @file: File under cursor (NULL for empty area)
 *
 * Returns: 0 on success
 */
int file_explorer_show_context_menu(file_explorer_window_t *win, s32 x, s32 y, file_info_t *file)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->context_menu_x = x;
    win->context_menu_y = y;
    win->context_file = file;
    
    /* Build context menu based on selection */
    desktop_menu_t *menu = &win->context_menu;
    memset(menu, 0, sizeof(desktop_menu_t));
    
    menu->menu_id = 1;
    menu->menu_type = MENU_TYPE_CONTEXT;
    menu->x = x;
    menu->y = y;
    menu->visible = true;
    menu->item_count = 0;
    
    if (file) {
        /* File context menu */
        /* Open/Open With */
        menu_item_t *open_item = create_menu_item("Open", "/usr/share/icons/open.png", "Enter");
        open_item->on_click = NULL; /* Would set callback */
        desktop_add_menu_item(menu, open_item);
        
        /* Open With submenu */
        menu_item_t *open_with = create_menu_item("Open With", "/usr/share/icons/apps.png", "");
        open_with->item_type = 2; /* Submenu */
        open_with->submenu = NULL; /* Would create submenu */
        desktop_add_menu_item(menu, open_with);
        
        desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
        
        /* File operations */
        desktop_add_menu_item(menu, create_menu_item("Cut", "/usr/share/icons/cut.png", "Ctrl+X"));
        desktop_add_menu_item(menu, create_menu_item("Copy", "/usr/share/icons/copy.png", "Ctrl+C"));
        desktop_add_menu_item(menu, create_menu_item("Paste", "/usr/share/icons/paste.png", "Ctrl+V"));
        
        desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
        
        /* More operations */
        desktop_add_menu_item(menu, create_menu_item("Rename", "/usr/share/icons/rename.png", "F2"));
        desktop_add_menu_item(menu, create_menu_item("Delete", "/usr/share/icons/delete.png", "Del"));
        
        desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
        
        /* Properties */
        desktop_add_menu_item(menu, create_menu_item("Properties", "/usr/share/icons/properties.png", "Alt+Enter"));
        
    } else {
        /* Empty area context menu */
        desktop_add_menu_item(menu, create_menu_item("New Folder", "/usr/share/icons/folder-new.png", "Ctrl+Shift+N"));
        desktop_add_menu_item(menu, create_menu_item("New File", "/usr/share/icons/file-new.png", "Ctrl+N"));
        
        desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
        
        desktop_add_menu_item(menu, create_menu_item("Paste", "/usr/share/icons/paste.png", "Ctrl+V"));
        
        desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
        
        desktop_add_menu_item(menu, create_menu_item("Select All", "/usr/share/icons/select-all.png", "Ctrl+A"));
        
        desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
        
        desktop_add_menu_item(menu, create_menu_item("Refresh", "/usr/share/icons/refresh.png", "F5"));
        desktop_add_menu_item(menu, create_menu_item("Properties", "/usr/share/icons/properties.png", ""));
    }
    
    win->active_context_menu = menu;
    
    if (win->on_context_menu) {
        win->on_context_menu(win, x, y);
    }
    
    return 0;
}

/**
 * file_explorer_show_desktop_context_menu - Show desktop context menu
 * @env: Desktop environment
 * @x: X position
 * @y: Y position
 *
 * Returns: 0 on success
 */
int file_explorer_show_desktop_context_menu(desktop_environment_t *env, s32 x, s32 y)
{
    if (!env) {
        return -EINVAL;
    }
    
    desktop_menu_t *menu = &env->context_menu;
    memset(menu, 0, sizeof(desktop_menu_t));
    
    menu->menu_id = 2;
    menu->menu_type = MENU_TYPE_CONTEXT;
    menu->x = x;
    menu->y = y;
    menu->visible = true;
    
    /* Desktop context menu items */
    desktop_add_menu_item(menu, create_menu_item("New Folder", "/usr/share/icons/folder-new.png", ""));
    desktop_add_menu_item(menu, create_menu_item("New Document", "/usr/share/icons/file-new.png", ""));
    
    desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
    
    /* View submenu */
    menu_item_t *view_menu = create_menu_item("View", "/usr/share/icons/view.png", "");
    view_menu->item_type = 2; /* Submenu */
    /* Would add view options submenu */
    desktop_add_menu_item(menu, view_menu);
    
    /* Sort by submenu */
    menu_item_t *sort_menu = create_menu_item("Sort By", "/usr/share/icons/sort.png", "");
    sort_menu->item_type = 2; /* Submenu */
    /* Would add sort options submenu */
    desktop_add_menu_item(menu, sort_menu);
    
    desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
    
    /* Desktop actions */
    desktop_add_menu_item(menu, create_menu_item("Show Desktop Icons", "", ""));
    desktop_add_menu_item(menu, create_menu_item("Change Background", "/usr/share/icons/background.png", ""));
    
    desktop_add_menu_item(menu, create_menu_item("", "", "")); /* Separator */
    
    desktop_add_menu_item(menu, create_menu_item("Properties", "/usr/share/icons/properties.png", ""));
    
    env->active_menu = menu;
    
    return 0;
}

/**
 * file_explorer_add_context_action - Add context menu action
 * @name: Action name
 * @icon: Icon path
 * @shortcut: Keyboard shortcut
 * @callback: Callback function
 *
 * Returns: 0 on success
 */
int file_explorer_add_context_action(const char *name, const char *icon, const char *shortcut, void (*callback)(void *))
{
    (void)name; (void)icon; (void)shortcut; (void)callback;
    /* In real implementation, would register custom context action */
    return 0;
}

/**
 * file_explorer_create_submenu - Create submenu
 * @menu: Parent menu
 * @label: Submenu label
 *
 * Returns: 0 on success
 */
int file_explorer_create_submenu(desktop_menu_t *menu, const char *label)
{
    if (!menu || !label) {
        return -EINVAL;
    }
    
    /* In real implementation, would create submenu structure */
    return 0;
}

/*===========================================================================*/
/*                         SCROLL BEHAVIOR                                   */
/*===========================================================================*/

/**
 * file_explorer_scroll_to - Scroll to offset
 * @win: Explorer window
 * @offset: Scroll offset
 *
 * Returns: 0 on success
 */
int file_explorer_scroll_to(file_explorer_window_t *win, u32 offset)
{
    if (!win) {
        return -EINVAL;
    }
    
    if (offset > win->scroll_max) {
        offset = win->scroll_max;
    }
    
    win->scroll_offset = offset;
    win->scroll_velocity = 0;
    
    /* In real implementation, would update scroll position */
    
    return 0;
}

/**
 * file_explorer_scroll_smooth - Smooth scroll
 * @win: Explorer window
 * @delta: Scroll delta
 *
 * Returns: 0 on success
 */
int file_explorer_scroll_smooth(file_explorer_window_t *win, float delta)
{
    if (!win) {
        return -EINVAL;
    }
    
    if (win->smooth_scroll) {
        /* Accumulate velocity for smooth scrolling */
        win->scroll_velocity += delta * 0.5f;
        
        /* Clamp velocity */
        if (win->scroll_velocity > 50.0f) win->scroll_velocity = 50.0f;
        if (win->scroll_velocity < -50.0f) win->scroll_velocity = -50.0f;
    } else {
        /* Direct scroll */
        s32 new_offset = (s32)win->scroll_offset + (s32)delta;
        if (new_offset < 0) new_offset = 0;
        if (new_offset > (s32)win->scroll_max) new_offset = win->scroll_max;
        win->scroll_offset = new_offset;
    }
    
    return 0;
}

/**
 * file_explorer_scroll_to_file - Scroll to file
 * @win: Explorer window
 * @file: File to scroll to
 *
 * Returns: 0 on success
 */
int file_explorer_scroll_to_file(file_explorer_window_t *win, file_info_t *file)
{
    if (!win || !file) {
        return -EINVAL;
    }
    
    /* Find file index and scroll to it */
    /* In real implementation, would calculate scroll position */
    
    return 0;
}

/**
 * file_explorer_center_selection - Center selection in view
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_center_selection(file_explorer_window_t *win)
{
    if (!win || win->selected_count == 0) {
        return -EINVAL;
    }
    
    /* Scroll to center the first selected item */
    /* In real implementation, would calculate center position */
    
    return 0;
}

/*===========================================================================*/
/*                         ADVANCED SEARCH                                   */
/*===========================================================================*/

/**
 * file_explorer_advanced_search - Advanced search with options
 * @win: Explorer window
 * @query: Search query
 * @flags: Search flags
 *
 * Returns: 0 on success
 */
int file_explorer_advanced_search(file_explorer_window_t *win, const char *query, u32 flags)
{
    if (!win || !query) {
        return -EINVAL;
    }
    
    win->is_searching = true;
    strncpy(win->search_query, query, sizeof(win->search_query) - 1);
    
    /* Apply search options */
    bool search_subfolders = (flags & 0x01) != 0;
    bool case_sensitive = (flags & 0x02) != 0;
    bool use_regex = (flags & 0x04) != 0;
    
    printk("[FILE-EXPLORER] Advanced search: %s (subfolders=%d, case=%d, regex=%d)\n",
           query, search_subfolders, case_sensitive, use_regex);
    
    /* In real implementation, would perform advanced search */
    
    return 0;
}

/**
 * file_explorer_save_search - Save search
 * @win: Explorer window
 * @name: Search name
 *
 * Returns: 0 on success
 */
int file_explorer_save_search(file_explorer_window_t *win, const char *name)
{
    if (!win || !name) {
        return -EINVAL;
    }
    
    /* In real implementation, would save search criteria */
    
    return 0;
}

/**
 * file_explorer_load_search - Load saved search
 * @win: Explorer window
 * @name: Search name
 *
 * Returns: 0 on success
 */
int file_explorer_load_search(file_explorer_window_t *win, const char *name)
{
    if (!win || !name) {
        return -EINVAL;
    }
    
    /* In real implementation, would load and execute saved search */
    
    return 0;
}

/*===========================================================================*/
/*                         ADVANCED VIEW OPTIONS                             */
/*===========================================================================*/

/**
 * file_explorer_toggle_tree_view - Toggle tree view in sidebar
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_toggle_tree_view(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->tree_view = !win->tree_view;
    
    /* Show/hide tree panel */
    if (win->tree_view && !win->tree_panel) {
        /* Create tree panel */
    }
    
    return 0;
}

/**
 * file_explorer_toggle_fullscreen - Toggle fullscreen mode
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_toggle_fullscreen(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->fullscreen_mode = !win->fullscreen_mode;
    
    /* In real implementation, would toggle window fullscreen */
    
    return 0;
}

/**
 * file_explorer_toggle_focus_mode - Toggle focus mode
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_toggle_focus_mode(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->focus_mode = !win->focus_mode;
    
    /* In focus mode, hide all UI except file view */
    
    return 0;
}

/**
 * file_explorer_set_column_width - Set column width
 * @win: Explorer window
 * @column: Column index
 * @width: New width
 *
 * Returns: 0 on success
 */
int file_explorer_set_column_width(file_explorer_window_t *win, u32 column, u32 width)
{
    if (!win || column >= win->column_count) {
        return -EINVAL;
    }
    
    win->columns[column].width = width;
    
    return 0;
}

/**
 * file_explorer_reorder_columns - Reorder columns
 * @win: Explorer window
 * @from: From index
 * @to: To index
 *
 * Returns: 0 on success
 */
int file_explorer_reorder_columns(file_explorer_window_t *win, u32 from, u32 to)
{
    if (!win || from >= win->column_count || to >= win->column_count) {
        return -EINVAL;
    }
    
    /* Swap columns */
    /* In real implementation, would reorder column array */
    
    return 0;
}

/*===========================================================================*/
/*                         FILTERS                                           */
/*===========================================================================*/

/**
 * file_explorer_set_filter - Set file filter
 * @win: Explorer window
 * @category: Category filter
 * @pattern: Pattern filter
 *
 * Returns: 0 on success
 */
int file_explorer_set_filter(file_explorer_window_t *win, u32 category, const char *pattern)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->filter_category = category;
    if (pattern) {
        strncpy(win->filter_pattern, pattern, sizeof(win->filter_pattern) - 1);
    }
    
    /* Re-load directory with filter */
    load_directory(win, win->current_path);
    
    return 0;
}

/**
 * file_explorer_set_size_filter - Set size filter
 * @win: Explorer window
 * @min: Minimum size
 * @max: Maximum size
 *
 * Returns: 0 on success
 */
int file_explorer_set_size_filter(file_explorer_window_t *win, u64 min, u64 max)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->filter_min_size = min;
    win->filter_max_size = max;
    
    load_directory(win, win->current_path);
    
    return 0;
}

/**
 * file_explorer_set_date_filter - Set date filter
 * @win: Explorer window
 * @from: From date
 * @to: To date
 *
 * Returns: 0 on success
 */
int file_explorer_set_date_filter(file_explorer_window_t *win, u64 from, u64 to)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->filter_date_from = from;
    win->filter_date_to = to;
    
    load_directory(win, win->current_path);
    
    return 0;
}

/**
 * file_explorer_clear_filters - Clear all filters
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_clear_filters(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    win->filter_category = FILE_CATEGORY_ALL;
    win->filter_pattern[0] = '\0';
    win->filter_min_size = 0;
    win->filter_max_size = 0;
    win->filter_date_from = 0;
    win->filter_date_to = 0;
    
    load_directory(win, win->current_path);
    
    return 0;
}

/*===========================================================================*/
/*                         PROPERTIES                                        */
/*===========================================================================*/

/**
 * file_explorer_show_properties - Show file properties
 * @win: Explorer window
 * @file: File to show properties for
 *
 * Returns: 0 on success
 */
int file_explorer_show_properties(file_explorer_window_t *win, file_info_t *file)
{
    if (!win || !file) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Showing properties for: %s\n", file->path);
    
    /* In real implementation, would show properties dialog */
    /* Would display:
     * - File name and type
     * - Size (with human-readable format)
     * - Created/Modified/Accessed dates
     * - Permissions
     * - Owner/Group
     * - MIME type
     * - Checksums (MD5, SHA256)
     */
    
    return 0;
}

/**
 * file_explorer_show_folder_size - Calculate and show folder size
 * @win: Explorer window
 * @folder: Folder to calculate
 *
 * Returns: 0 on success
 */
int file_explorer_show_folder_size(file_explorer_window_t *win, file_info_t *folder)
{
    if (!win || !folder) {
        return -EINVAL;
    }
    
    printk("[FILE-EXPLORER] Calculating folder size: %s\n", folder->path);
    
    /* In real implementation, would recursively calculate folder size */
    
    return 0;
}

/*===========================================================================*/
/*                         NAVIGATION TREE                                   */
/*===========================================================================*/

/**
 * file_explorer_navigate_tree - Navigate using tree view
 * @win: Explorer window
 * @path: Path to navigate to
 *
 * Returns: 0 on success
 */
int file_explorer_navigate_tree(file_explorer_window_t *win, const char *path)
{
    if (!win || !path) {
        return -EINVAL;
    }
    
    return file_explorer_navigate(win, path);
}

/*===========================================================================*/
/*                         THUMBNAIL PREFETCH                                */
/*===========================================================================*/

/**
 * file_explorer_prefetch_thumbnails - Prefetch thumbnails for visible files
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int file_explorer_prefetch_thumbnails(file_explorer_window_t *win)
{
    if (!win) {
        return -EINVAL;
    }
    
    /* In real implementation, would generate thumbnails for visible files */
    /* Would use background threads for non-blocking operation */
    
    return 0;
}

/*===========================================================================*/
/*                         NAVIGATION                                        */
/*===========================================================================*/

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
/*                         STATUS BAR UPDATE                                 */
/*===========================================================================*/

/**
 * update_status_bar - Update status bar
 * @win: Explorer window
 *
 * Returns: 0 on success
 */
int update_status_bar(file_explorer_window_t *win)
{
    if (!win || !win->status_bar) {
        return -EINVAL;
    }
    
    /* Update item count */
    char status_text[128];
    if (win->selected_count > 0) {
        char size_str[32];
        file_get_size_string(0, size_str, sizeof(size_str));
        snprintf(status_text, sizeof(status_text), "%d items selected (%s) | %d total items",
                 win->selected_count, size_str, win->entry_count);
    } else {
        snprintf(status_text, sizeof(status_text), "%d items", win->entry_count);
    }
    
    /* In real implementation, would update status label */
    
    return 0;
}

/*===========================================================================*/
/*                         LOAD DIRECTORY                                    */
/*===========================================================================*/

/**
 * load_directory - Load directory contents
 * @win: Explorer window
 * @path: Directory path
 *
 * Returns: 0 on success, negative error code on failure
 */
int load_directory(file_explorer_window_t *win, const char *path)
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
     * - Get file info
     * - Apply filters
     * - Sort entries
     * - Generate thumbnails
     */
    
    /* Mock entries */
    directory_entry_t *entries = NULL;
    u32 count = 0;
    
    /* Add mock folders */
    const char *mock_folders[] = {"Documents", "Downloads", "Pictures", "Music", "Videos", NULL};
    for (u32 i = 0; mock_folders[i] != NULL; i++) {
        directory_entry_t *entry = kzalloc(sizeof(directory_entry_t));
        if (!entry) continue;
        
        strncpy(entry->file.name, mock_folders[i], MAX_FILENAME_LENGTH - 1);
        snprintf(entry->file.path, MAX_PATH_LENGTH, "%s/%s", path, mock_folders[i]);
        entry->file.file_type = FILE_TYPE_FOLDER;
        entry->file.file_category = FILE_CATEGORY_ALL;
        strcpy(entry->file.icon_path, "/usr/share/icons/folder.png");
        
        entry->next = entries;
        entries = entry;
        count++;
    }
    
    /* Add mock files */
    const char *mock_files[] = {"readme.txt", "config.json", "data.csv", NULL};
    for (u32 i = 0; mock_files[i] != NULL; i++) {
        directory_entry_t *entry = kzalloc(sizeof(directory_entry_t));
        if (!entry) continue;
        
        strncpy(entry->file.name, mock_files[i], MAX_FILENAME_LENGTH - 1);
        snprintf(entry->file.path, MAX_PATH_LENGTH, "%s/%s", path, mock_files[i]);
        entry->file.file_type = FILE_TYPE_FILE;
        entry->file.size = 1024 * (i + 1);
        strcpy(entry->file.icon_path, "/usr/share/icons/file.png");
        
        entry->next = entries;
        entries = entry;
        count++;
    }
    
    win->entries = entries;
    win->entry_count = count;
    
    /* Sort entries */
    sort_entries(win);
    
    /* Reset scroll */
    win->scroll_offset = 0;
    win->scroll_max = count * (win->icon_size + 20);
    
    /* Update UI */
    update_file_view(win);
    update_status_bar(win);
    
    /* Prefetch thumbnails */
    file_explorer_prefetch_thumbnails(win);
    
    printk("[FILE-EXPLORER] Loaded %d entries from %s\n", count, path);
    
    return 0;
}
