/*
 * NEXUS OS - App Launcher
 * gui/app-launcher/app-launcher.c
 *
 * Application launcher with search, categories, favorites, recent apps
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         APP LAUNCHER CONFIGURATION                        */
/*===========================================================================*/

#define LAUNCHER_MAX_APPS           512
#define LAUNCHER_MAX_CATEGORIES     16
#define LAUNCHER_MAX_FAVORITES      32
#define LAUNCHER_MAX_RECENT         20
#define LAUNCHER_MAX_SEARCH_RESULTS 50

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 app_id;
    char name[128];
    char description[256];
    char exec_path[512];
    char icon_path[256];
    char category[64];
    char keywords[256];       /* Search keywords */
    u32 launch_count;
    u64 last_launch;
    u64 install_time;
    bool is_favorite;
    bool is_hidden;
    bool is_system;
    float rating;
    u32 rating_count;
} app_info_t;

typedef struct {
    char name[64];
    char icon[64];
    u32 app_count;
    u32 app_ids[LAUNCHER_MAX_APPS];
} app_category_t;

typedef struct {
    bool is_open;
    rect_t geometry;
    char search_text[128];
    app_info_t *filtered_apps;
    u32 filtered_count;
    u32 selected_index;
    u32 scroll_offset;
    u32 current_category;
    app_category_t categories[LAUNCHER_MAX_CATEGORIES];
    u32 category_count;
    u32 favorite_ids[LAUNCHER_MAX_FAVORITES];
    u32 favorite_count;
    u32 recent_ids[LAUNCHER_MAX_RECENT];
    u32 recent_count;
    app_info_t *all_apps;
    u32 app_count;
    u32 view_mode;          /* 0=grid, 1=list */
    u32 icon_size;
    void (*on_app_launch)(app_info_t *);
} app_launcher_t;

static app_launcher_t g_launcher;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int app_launcher_init(void)
{
    printk("[LAUNCHER] Initializing app launcher...\n");
    
    memset(&g_launcher, 0, sizeof(app_launcher_t));
    g_launcher.geometry.x = 100;
    g_launcher.geometry.y = 100;
    g_launcher.geometry.width = 800;
    g_launcher.geometry.height = 600;
    g_launcher.view_mode = 0;  /* Grid view */
    g_launcher.icon_size = 64;
    
    /* Define categories */
    app_category_t *cat;
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "Internet");
    strcpy(cat->icon, "internet");
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "Office");
    strcpy(cat->icon, "office");
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "Multimedia");
    strcpy(cat->icon, "multimedia");
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "Games");
    strcpy(cat->icon, "games");
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "Development");
    strcpy(cat->icon, "development");
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "System");
    strcpy(cat->icon, "system");
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "Utilities");
    strcpy(cat->icon, "utilities");
    
    cat = &g_launcher.categories[g_launcher.category_count++];
    strcpy(cat->name, "Settings");
    strcpy(cat->icon, "settings");
    
    /* Allocate app array */
    g_launcher.all_apps = kmalloc(sizeof(app_info_t) * LAUNCHER_MAX_APPS);
    if (!g_launcher.all_apps) {
        return -ENOMEM;
    }
    g_launcher.filtered_apps = g_launcher.all_apps;
    
    /* Register default applications */
    app_info_t *app;
    u32 idx = 0;
    
    /* Web Browser */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "Web Browser");
    strcpy(app->description, "Fast and secure web browser");
    strcpy(app->exec_path, "/apps/browser");
    strcpy(app->icon_path, "/icons/browser.png");
    strcpy(app->category, "Internet");
    strcpy(app->keywords, "web internet browse www http");
    app->is_favorite = true;
    app->rating = 4.5f;
    
    /* Terminal */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "Terminal");
    strcpy(app->description, "Command line terminal emulator");
    strcpy(app->exec_path, "/apps/terminal");
    strcpy(app->icon_path, "/icons/terminal.png");
    strcpy(app->category, "System");
    strcpy(app->keywords, "terminal shell command line bash console");
    app->is_favorite = true;
    app->is_system = true;
    app->rating = 5.0f;
    
    /* File Manager */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "File Manager");
    strcpy(app->description, "Browse and manage files");
    strcpy(app->exec_path, "/apps/filemanager");
    strcpy(app->icon_path, "/icons/files.png");
    strcpy(app->category, "System");
    strcpy(app->keywords, "file manager explorer folder directory");
    app->is_favorite = true;
    app->is_system = true;
    
    /* Text Editor */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "Text Editor");
    strcpy(app->description, "Simple text editor");
    strcpy(app->exec_path, "/apps/editor");
    strcpy(app->icon_path, "/icons/editor.png");
    strcpy(app->category, "Office");
    strcpy(app->keywords, "text editor write code notepad");
    app->rating = 4.0f;
    
    /* Settings */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "Settings");
    strcpy(app->description, "System settings and preferences");
    strcpy(app->exec_path, "/apps/settings");
    strcpy(app->icon_path, "/icons/settings.png");
    strcpy(app->category, "Settings");
    strcpy(app->keywords, "settings preferences configuration options");
    app->is_favorite = true;
    app->is_system = true;
    
    /* App Store */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "App Store");
    strcpy(app->description, "Browse and install applications");
    strcpy(app->exec_path, "/apps/appstore");
    strcpy(app->icon_path, "/icons/appstore.png");
    strcpy(app->category, "System");
    strcpy(app->keywords, "app store install software package");
    app->is_system = true;
    
    /* System Monitor */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "System Monitor");
    strcpy(app->description, "Monitor system resources");
    strcpy(app->exec_path, "/apps/sysmonitor");
    strcpy(app->icon_path, "/icons/monitor.png");
    strcpy(app->category, "Utilities");
    strcpy(app->keywords, "system monitor resources cpu memory disk network");
    app->rating = 4.5f;
    
    /* Calculator */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "Calculator");
    strcpy(app->description, "Scientific calculator");
    strcpy(app->exec_path, "/apps/calculator");
    strcpy(app->icon_path, "/icons/calculator.png");
    strcpy(app->category, "Utilities");
    strcpy(app->keywords, "calculator math compute arithmetic");
    
    /* Image Viewer */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "Image Viewer");
    strcpy(app->description, "View and edit images");
    strcpy(app->exec_path, "/apps/imageviewer");
    strcpy(app->icon_path, "/icons/image.png");
    strcpy(app->category, "Multimedia");
    strcpy(app->keywords, "image photo picture viewer editor");
    
    /* Music Player */
    app = &g_launcher.all_apps[idx++];
    app->app_id = idx;
    strcpy(app->name, "Music Player");
    strcpy(app->description, "Play music and audio files");
    strcpy(app->exec_path, "/apps/musicplayer");
    strcpy(app->icon_path, "/icons/music.png");
    strcpy(app->category, "Multimedia");
    strcpy(app->keywords, "music audio player mp3 playlist");
    
    g_launcher.app_count = idx;
    g_launcher.filtered_count = idx;
    
    /* Add some apps to favorites */
    g_launcher.favorite_ids[0] = 1;  /* Browser */
    g_launcher.favorite_ids[1] = 2;  /* Terminal */
    g_launcher.favorite_ids[2] = 3;  /* File Manager */
    g_launcher.favorite_ids[3] = 5;  /* Settings */
    g_launcher.favorite_count = 4;
    
    printk("[LAUNCHER] App launcher initialized (%d apps, %d categories)\n",
           g_launcher.app_count, g_launcher.category_count);
    
    return 0;
}

int app_launcher_shutdown(void)
{
    printk("[LAUNCHER] Shutting down app launcher...\n");
    
    if (g_launcher.all_apps) {
        kfree(g_launcher.all_apps);
    }
    
    g_launcher.is_open = false;
    return 0;
}

/*===========================================================================*/
/*                         LAUNCHER OPERATIONS                               */
/*===========================================================================*/

int app_launcher_open(void)
{
    g_launcher.is_open = true;
    g_launcher.selected_index = 0;
    g_launcher.scroll_offset = 0;
    g_launcher.search_text[0] = '\0';
    g_launcher.filtered_apps = g_launcher.all_apps;
    g_launcher.filtered_count = g_launcher.app_count;
    
    printk("[LAUNCHER] Opened\n");
    return 0;
}

int app_launcher_close(void)
{
    g_launcher.is_open = false;
    printk("[LAUNCHER] Closed\n");
    return 0;
}

int app_launcher_toggle(void)
{
    if (g_launcher.is_open) {
        return app_launcher_close();
    } else {
        return app_launcher_open();
    }
}

/*===========================================================================*/
/*                         SEARCH FUNCTIONALITY                              */
/*===========================================================================*/

int app_launcher_search(const char *query)
{
    if (!query) return -EINVAL;
    
    strncpy(g_launcher.search_text, query, sizeof(g_launcher.search_text) - 1);
    
    if (strlen(query) == 0) {
        g_launcher.filtered_apps = g_launcher.all_apps;
        g_launcher.filtered_count = g_launcher.app_count;
        g_launcher.selected_index = 0;
        g_launcher.scroll_offset = 0;
        return g_launcher.app_count;
    }
    
    /* Filter applications */
    u32 count = 0;
    
    for (u32 i = 0; i < g_launcher.app_count && count < LAUNCHER_MAX_SEARCH_RESULTS; i++) {
        app_info_t *app = &g_launcher.all_apps[i];
        
        /* Search in name */
        if (strcasestr(app->name, query)) {
            g_launcher.filtered_apps[count++] = *app;
            continue;
        }
        
        /* Search in description */
        if (strcasestr(app->description, query)) {
            g_launcher.filtered_apps[count++] = *app;
            continue;
        }
        
        /* Search in category */
        if (strcasestr(app->category, query)) {
            g_launcher.filtered_apps[count++] = *app;
            continue;
        }
        
        /* Search in keywords */
        if (strcasestr(app->keywords, query)) {
            g_launcher.filtered_apps[count++] = *app;
        }
    }
    
    g_launcher.filtered_count = count;
    g_launcher.selected_index = 0;
    g_launcher.scroll_offset = 0;
    
    return count;
}

/*===========================================================================*/
/*                         NAVIGATION                                        */
/*===========================================================================*/

int app_launcher_select_next(void)
{
    if (g_launcher.filtered_count == 0) return 0;
    
    g_launcher.selected_index++;
    if (g_launcher.selected_index >= g_launcher.filtered_count) {
        g_launcher.selected_index = 0;
    }
    
    /* Adjust scroll */
    u32 visible_items = 6;  /* Depends on icon size and window height */
    if (g_launcher.selected_index >= g_launcher.scroll_offset + visible_items) {
        g_launcher.scroll_offset = g_launcher.selected_index - visible_items + 1;
    }
    
    return 0;
}

int app_launcher_select_prev(void)
{
    if (g_launcher.filtered_count == 0) return 0;
    
    if (g_launcher.selected_index == 0) {
        g_launcher.selected_index = g_launcher.filtered_count - 1;
    } else {
        g_launcher.selected_index--;
    }
    
    /* Adjust scroll */
    if (g_launcher.selected_index < g_launcher.scroll_offset) {
        g_launcher.scroll_offset = g_launcher.selected_index;
    }
    
    return 0;
}

int app_launcher_launch_selected(void)
{
    if (g_launcher.filtered_count == 0) return -ENOENT;
    
    app_info_t *app = &g_launcher.filtered_apps[g_launcher.selected_index];
    
    /* Update launch stats */
    app->launch_count++;
    app->last_launch = ktime_get_sec();
    
    /* Add to recent */
    bool found = false;
    for (u32 i = 0; i < g_launcher.recent_count; i++) {
        if (g_launcher.recent_ids[i] == app->app_id) {
            found = true;
            break;
        }
    }
    
    if (!found && g_launcher.recent_count < LAUNCHER_MAX_RECENT) {
        g_launcher.recent_ids[g_launcher.recent_count++] = app->app_id;
    }
    
    printk("[LAUNCHER] Launching: %s\n", app->name);
    
    if (g_launcher.on_app_launch) {
        g_launcher.on_app_launch(app);
    }
    
    app_launcher_close();
    return 0;
}

/*===========================================================================*/
/*                         FAVORITES MANAGEMENT                              */
/*===========================================================================*/

int app_launcher_toggle_favorite(u32 app_id)
{
    for (u32 i = 0; i < g_launcher.app_count; i++) {
        if (g_launcher.all_apps[i].app_id == app_id) {
            app_info_t *app = &g_launcher.all_apps[i];
            app->is_favorite = !app->is_favorite;
            
            if (app->is_favorite) {
                if (g_launcher.favorite_count < LAUNCHER_MAX_FAVORITES) {
                    g_launcher.favorite_ids[g_launcher.favorite_count++] = app_id;
                }
            } else {
                for (u32 j = 0; j < g_launcher.favorite_count; j++) {
                    if (g_launcher.favorite_ids[j] == app_id) {
                        for (u32 k = j; k < g_launcher.favorite_count - 1; k++) {
                            g_launcher.favorite_ids[k] = g_launcher.favorite_ids[k + 1];
                        }
                        g_launcher.favorite_count--;
                        break;
                    }
                }
            }
            
            return 0;
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         CATEGORY FILTERING                                */
/*===========================================================================*/

int app_launcher_filter_category(u32 category_index)
{
    if (category_index >= g_launcher.category_count) return -EINVAL;
    
    g_launcher.current_category = category_index;
    app_category_t *cat = &g_launcher.categories[category_index];
    
    /* Filter apps by category */
    u32 count = 0;
    
    for (u32 i = 0; i < g_launcher.app_count; i++) {
        if (strcmp(g_launcher.all_apps[i].category, cat->name) == 0) {
            g_launcher.filtered_apps[count++] = g_launcher.all_apps[i];
        }
    }
    
    g_launcher.filtered_count = count;
    g_launcher.selected_index = 0;
    g_launcher.scroll_offset = 0;
    
    printk("[LAUNCHER] Filtered by category: %s (%d apps)\n", cat->name, count);
    return count;
}

/*===========================================================================*/
/*                         VIEW MODES                                        */
/*===========================================================================*/

int app_launcher_set_view_mode(u32 mode)
{
    if (mode > 1) return -EINVAL;
    
    g_launcher.view_mode = mode;
    
    if (mode == 0) {
        g_launcher.icon_size = 64;  /* Grid view - larger icons */
    } else {
        g_launcher.icon_size = 32;  /* List view - smaller icons */
    }
    
    printk("[LAUNCHER] View mode: %s\n", mode == 0 ? "Grid" : "List");
    return 0;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

int app_launcher_render(void *surface)
{
    if (!g_launcher.is_open || !surface) return -EINVAL;
    
    /* Render launcher background */
    /* Render search bar */
    /* Render category tabs */
    /* Render app grid/list */
    /* Render favorites section */
    /* Render recent apps section */
    /* Render selected item highlight */
    
    (void)surface;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

app_info_t *app_launcher_get_app(u32 app_id)
{
    for (u32 i = 0; i < g_launcher.app_count; i++) {
        if (g_launcher.all_apps[i].app_id == app_id) {
            return &g_launcher.all_apps[i];
        }
    }
    return NULL;
}

int app_launcher_get_recent_apps(app_info_t *apps, u32 count)
{
    if (!apps || count == 0) return -EINVAL;
    
    u32 found = 0;
    for (u32 i = 0; i < g_launcher.recent_count && found < count; i++) {
        app_info_t *app = app_launcher_get_app(g_launcher.recent_ids[i]);
        if (app) {
            apps[found++] = *app;
        }
    }
    
    return found;
}

app_launcher_t *app_launcher_get(void)
{
    return &g_launcher;
}
