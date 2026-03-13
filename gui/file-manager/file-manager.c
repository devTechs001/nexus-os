/*
 * NEXUS OS - File Manager
 * gui/file-manager/file-manager.c
 *
 * File browser with preview, archive support, network shares
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         FILE MANAGER CONFIGURATION                        */
/*===========================================================================*/

#define FM_MAX_FILES              10000
#define FM_MAX_BOOKMARKS          32
#define FM_MAX_HISTORY            100
#define FM_MAX_SELECTED           1000

/*===========================================================================*/
/*                         FILE TYPES                                        */
/*===========================================================================*/

#define FILE_TYPE_UNKNOWN         0
#define FILE_TYPE_FILE            1
#define FILE_TYPE_DIRECTORY       2
#define FILE_TYPE_LINK            3
#define FILE_TYPE_ARCHIVE         4
#define FILE_TYPE_IMAGE           5
#define FILE_TYPE_VIDEO           6
#define FILE_TYPE_AUDIO           7
#define FILE_TYPE_DOCUMENT        8
#define FILE_TYPE_EXECUTABLE      9

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    char name[256];
    char path[512];
    u32 type;
    u64 size;
    u64 modified;
    u64 created;
    char mime_type[64];
    char icon[64];
    bool is_hidden;
    bool is_readonly;
    bool is_selected;
} file_info_t;

typedef struct {
    char path[512];
    char name[64];
    char icon[64];
} bookmark_t;

typedef struct {
    bool is_open;
    rect_t window_rect;
    char current_path[512];
    file_info_t *files;
    u32 file_count;
    u32 selected_count;
    u32 scroll_offset;
    u32 view_mode;  /* 0=icons, 1=list, 2=details */
    bookmark_t bookmarks[FM_MAX_BOOKMARKS];
    u32 bookmark_count;
    char history[FM_MAX_HISTORY][512];
    u32 history_index;
    u32 history_count;
    char clipboard_path[512];
    bool clipboard_cut;
    void (*on_file_open)(file_info_t *);
    void (*on_selection_change)(file_info_t **, u32);
} file_manager_t;

static file_manager_t g_fm;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int file_manager_init(void)
{
    printk("[FILE_MANAGER] Initializing file manager...\n");
    
    memset(&g_fm, 0, sizeof(file_manager_t));
    g_fm.window_rect.width = 800;
    g_fm.window_rect.height = 600;
    strcpy(g_fm.current_path, "/");
    g_fm.view_mode = 1;  /* List view */
    
    /* Default bookmarks */
    bookmark_t *bm;
    
    bm = &g_fm.bookmarks[g_fm.bookmark_count++];
    strcpy(bm->path, "/home");
    strcpy(bm->name, "Home");
    strcpy(bm->icon, "home");
    
    bm = &g_fm.bookmarks[g_fm.bookmark_count++];
    strcpy(bm->path, "/documents");
    strcpy(bm->name, "Documents");
    strcpy(bm->icon, "documents");
    
    bm = &g_fm.bookmarks[g_fm.bookmark_count++];
    strcpy(bm->path, "/downloads");
    strcpy(bm->name, "Downloads");
    strcpy(bm->icon, "downloads");
    
    bm = &g_fm.bookmarks[g_fm.bookmark_count++];
    strcpy(bm->path, "/pictures");
    strcpy(bm->name, "Pictures");
    strcpy(bm->icon, "pictures");
    
    bm = &g_fm.bookmarks[g_fm.bookmark_count++];
    strcpy(bm->path, "/music");
    strcpy(bm->name, "Music");
    strcpy(bm->icon, "music");
    
    /* Allocate file list */
    g_fm.files = kmalloc(sizeof(file_info_t) * 100);
    
    printk("[FILE_MANAGER] File manager initialized\n");
    return 0;
}

int file_manager_shutdown(void)
{
    printk("[FILE_MANAGER] Shutting down file manager...\n");
    
    if (g_fm.files) {
        kfree(g_fm.files);
    }
    
    g_fm.is_open = false;
    return 0;
}

/*===========================================================================*/
/*                         NAVIGATION                                        */
/*===========================================================================*/

int file_manager_navigate(const char *path)
{
    if (!path) return -EINVAL;
    
    /* Save to history */
    if (g_fm.history_count < FM_MAX_HISTORY) {
        strcpy(g_fm.history[g_fm.history_count++], g_fm.current_path);
        g_fm.history_index = g_fm.history_count;
    }
    
    strncpy(g_fm.current_path, path, sizeof(g_fm.current_path) - 1);
    
    printk("[FILE_MANAGER] Navigated to: %s\n", path);
    
    /* In real implementation, would read directory contents */
    /* Mock: create sample files */
    if (g_fm.files) {
        g_fm.file_count = 0;
        
        /* Add some mock files */
        file_info_t *file;
        
        file = &g_fm.files[g_fm.file_count++];
        strcpy(file->name, "Documents");
        strcpy(file->path, "/Documents");
        file->type = FILE_TYPE_DIRECTORY;
        strcpy(file->icon, "folder");
        
        file = &g_fm.files[g_fm.file_count++];
        strcpy(file->name, "Downloads");
        strcpy(file->path, "/Downloads");
        file->type = FILE_TYPE_DIRECTORY;
        strcpy(file->icon, "folder");
        
        file = &g_fm.files[g_fm.file_count++];
        strcpy(file->name, "readme.txt");
        strcpy(file->path, "/readme.txt");
        file->type = FILE_TYPE_DOCUMENT;
        file->size = 1024;
        strcpy(file->icon, "text");
    }
    
    return 0;
}

int file_manager_navigate_up(void)
{
    /* Go to parent directory */
    char *last_slash = strrchr(g_fm.current_path, '/');
    if (last_slash && last_slash != g_fm.current_path) {
        *last_slash = '\0';
        return file_manager_navigate(g_fm.current_path);
    }
    return 0;
}

int file_manager_navigate_back(void)
{
    if (g_fm.history_index > 0) {
        g_fm.history_index--;
        return file_manager_navigate(g_fm.history[g_fm.history_index]);
    }
    return -ENOENT;
}

int file_manager_navigate_forward(void)
{
    if (g_fm.history_index < g_fm.history_count - 1) {
        g_fm.history_index++;
        return file_manager_navigate(g_fm.history[g_fm.history_index]);
    }
    return -ENOENT;
}

/*===========================================================================*/
/*                         FILE OPERATIONS                                   */
/*===========================================================================*/

int file_manager_open_file(file_info_t *file)
{
    if (!file) return -EINVAL;
    
    printk("[FILE_MANAGER] Opening: %s\n", file->path);
    
    if (file->type == FILE_TYPE_DIRECTORY) {
        return file_manager_navigate(file->path);
    }
    
    if (g_fm.on_file_open) {
        g_fm.on_file_open(file);
    }
    
    return 0;
}

int file_manager_create_directory(const char *name)
{
    if (!name) return -EINVAL;
    
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_fm.current_path, name);
    
    printk("[FILE_MANAGER] Creating directory: %s\n", path);
    
    /* In real implementation, would create directory */
    return 0;
}

int file_manager_delete(const char *path)
{
    if (!path) return -EINVAL;
    
    printk("[FILE_MANAGER] Deleting: %s\n", path);
    
    /* In real implementation, would delete file/directory */
    return 0;
}

int file_manager_rename(const char *old_path, const char *new_name)
{
    if (!old_path || !new_name) return -EINVAL;
    
    printk("[FILE_MANAGER] Renaming: %s -> %s\n", old_path, new_name);
    
    /* In real implementation, would rename file */
    return 0;
}

/*===========================================================================*/
/*                         CLIPBOARD                                         */
/*===========================================================================*/

int file_manager_copy(const char *path)
{
    if (!path) return -EINVAL;
    
    strncpy(g_fm.clipboard_path, path, sizeof(g_fm.clipboard_path) - 1);
    g_fm.clipboard_cut = false;
    
    printk("[FILE_MANAGER] Copy: %s\n", path);
    return 0;
}

int file_manager_cut(const char *path)
{
    if (!path) return -EINVAL;
    
    strncpy(g_fm.clipboard_path, path, sizeof(g_fm.clipboard_path) - 1);
    g_fm.clipboard_cut = true;
    
    printk("[FILE_MANAGER] Cut: %s\n", path);
    return 0;
}

int file_manager_paste(const char *dest_path)
{
    if (!dest_path) return -EINVAL;
    
    printk("[FILE_MANAGER] Paste %s to %s\n", 
           g_fm.clipboard_cut ? "cut" : "copy",
           dest_path);
    
    /* In real implementation, would copy/move file */
    g_fm.clipboard_path[0] = '\0';
    g_fm.clipboard_cut = false;
    
    return 0;
}

/*===========================================================================*/
/*                         SELECTION                                         */
/*===========================================================================*/

int file_manager_select(file_info_t *file, bool selected)
{
    if (!file) return -EINVAL;
    
    file->is_selected = selected;
    
    /* Update selected count */
    g_fm.selected_count = 0;
    for (u32 i = 0; i < g_fm.file_count; i++) {
        if (g_fm.files[i].is_selected) {
            g_fm.selected_count++;
        }
    }
    
    if (g_fm.on_selection_change) {
        g_fm.on_selection_change(&file, 1);
    }
    
    return 0;
}

int file_manager_select_all(void)
{
    for (u32 i = 0; i < g_fm.file_count; i++) {
        g_fm.files[i].is_selected = true;
    }
    g_fm.selected_count = g_fm.file_count;
    
    return 0;
}

int file_manager_select_none(void)
{
    for (u32 i = 0; i < g_fm.file_count; i++) {
        g_fm.files[i].is_selected = false;
    }
    g_fm.selected_count = 0;
    
    return 0;
}

/*===========================================================================*/
/*                         VIEW MODES                                        */
/*===========================================================================*/

int file_manager_set_view_mode(u32 mode)
{
    if (mode > 2) return -EINVAL;
    
    g_fm.view_mode = mode;
    
    const char *mode_name;
    switch (mode) {
        case 0: mode_name = "Icons"; break;
        case 1: mode_name = "List"; break;
        case 2: mode_name = "Details"; break;
        default: mode_name = "Unknown"; break;
    }
    
    printk("[FILE_MANAGER] View mode: %s\n", mode_name);
    return 0;
}

/*===========================================================================*/
/*                         BOOKMARKS                                         */
/*===========================================================================*/

int file_manager_add_bookmark(const char *path, const char *name, const char *icon)
{
    if (g_fm.bookmark_count >= FM_MAX_BOOKMARKS) {
        return -ENOMEM;
    }
    
    bookmark_t *bm = &g_fm.bookmarks[g_fm.bookmark_count++];
    strncpy(bm->path, path, sizeof(bm->path) - 1);
    strncpy(bm->name, name, sizeof(bm->name) - 1);
    strncpy(bm->icon, icon, sizeof(bm->icon) - 1);
    
    printk("[FILE_MANAGER] Added bookmark: %s -> %s\n", path, name);
    return 0;
}

int file_manager_remove_bookmark(u32 index)
{
    if (index >= g_fm.bookmark_count) {
        return -EINVAL;
    }
    
    /* Shift bookmarks */
    for (u32 i = index; i < g_fm.bookmark_count - 1; i++) {
        g_fm.bookmarks[i] = g_fm.bookmarks[i + 1];
    }
    g_fm.bookmark_count--;
    
    return 0;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

int file_manager_render(void *surface)
{
    if (!g_fm.is_open || !surface) return -EINVAL;
    
    /* Render window frame */
    /* Render navigation bar */
    /* Render bookmarks sidebar */
    /* Render file list */
    /* Render status bar */
    
    (void)surface;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

file_manager_t *file_manager_get(void)
{
    return &g_fm;
}

const char *file_manager_get_type_name(u32 type)
{
    switch (type) {
        case FILE_TYPE_FILE:       return "File";
        case FILE_TYPE_DIRECTORY:  return "Folder";
        case FILE_TYPE_LINK:       return "Link";
        case FILE_TYPE_ARCHIVE:    return "Archive";
        case FILE_TYPE_IMAGE:      return "Image";
        case FILE_TYPE_VIDEO:      return "Video";
        case FILE_TYPE_AUDIO:      return "Audio";
        case FILE_TYPE_DOCUMENT:   return "Document";
        case FILE_TYPE_EXECUTABLE: return "Executable";
        default:                   return "Unknown";
    }
}
