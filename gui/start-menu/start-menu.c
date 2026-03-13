/*
 * NEXUS OS - Start Menu Application Implementation
 * gui/start-menu/start-menu.c
 *
 * Modern start menu with search, pinned apps, and system actions
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "start-menu.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL START MENU INSTANCE                        */
/*===========================================================================*/

static start_menu_t g_start_menu;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int create_start_menu_ui(start_menu_t *menu);
static int create_search_box(start_menu_t *menu);
static int create_pinned_section(start_menu_t *menu);
static int create_app_list(start_menu_t *menu);
static int create_user_section(start_menu_t *menu);
static int create_system_buttons(start_menu_t *menu);

static void on_search_changed(widget_t *widget, const char *text);
static void on_app_clicked(widget_t *widget);
static void on_settings_clicked(widget_t *button);
static void on_shutdown_clicked(widget_t *button);
static void on_restart_clicked(widget_t *button);
static void on_logout_clicked(widget_t *button);
static void on_lock_clicked(widget_t *button);

static int load_system_apps(start_menu_t *menu);
static int search_apps(start_menu_t *menu, const char *query);

/*===========================================================================*/
/*                         START MENU LIFECYCLE                              */
/*===========================================================================*/

/**
 * start_menu_init - Initialize start menu
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int start_menu_init(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    printk("[START-MENU] ========================================\n");
    printk("[START-MENU] NEXUS OS Start Menu v%s\n", START_MENU_VERSION);
    printk("[START-MENU] ========================================\n");
    printk("[START-MENU] Initializing start menu...\n");
    
    /* Clear structure */
    memset(menu, 0, sizeof(start_menu_t));
    menu->initialized = true;
    menu->visible = false;
    
    /* Set default user */
    strcpy(menu->username, "User");
    strcpy(menu->user_icon, "/usr/share/icons/user.png");
    
    /* Create UI */
    if (create_start_menu_ui(menu) != 0) {
        printk("[START-MENU] Failed to create UI\n");
        return -1;
    }
    
    /* Load system apps */
    load_system_apps(menu);
    
    printk("[START-MENU] Start menu initialized\n");
    printk("[START-MENU] ========================================\n");
    
    return 0;
}

/**
 * start_menu_show - Show start menu
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int start_menu_show(start_menu_t *menu)
{
    if (!menu || !menu->initialized) {
        return -EINVAL;
    }
    
    if (menu->main_window) {
        window_show(menu->main_window);
        menu->visible = true;
    }
    
    return 0;
}

/**
 * start_menu_hide - Hide start menu
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int start_menu_hide(start_menu_t *menu)
{
    if (!menu || !menu->initialized) {
        return -EINVAL;
    }
    
    if (menu->main_window) {
        window_hide(menu->main_window);
        menu->visible = false;
    }
    
    return 0;
}

/**
 * start_menu_toggle - Toggle start menu visibility
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_toggle(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    if (menu->visible) {
        return start_menu_hide(menu);
    } else {
        return start_menu_show(menu);
    }
}

/**
 * start_menu_shutdown - Shutdown start menu
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_shutdown(start_menu_t *menu)
{
    if (!menu || !menu->initialized) {
        return -EINVAL;
    }
    
    if (menu->main_window) {
        window_hide(menu->main_window);
    }
    
    menu->initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_start_menu_ui - Create start menu UI
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_start_menu_ui(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    /* Create main window */
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "Start Menu");
    props.type = WINDOW_TYPE_POPUP;
    props.flags = WINDOW_FLAG_BORDERLESS;
    props.bounds.x = 10;
    props.bounds.y = 1080 - START_MENU_HEIGHT - 60;
    props.bounds.width = START_MENU_WIDTH;
    props.bounds.height = START_MENU_HEIGHT;
    props.background = color_from_rgba(30, 30, 45, 250);
    
    menu->main_window = window_create(&props);
    if (!menu->main_window) {
        return -1;
    }
    
    /* Create main widget */
    menu->main_widget = panel_create(NULL, 0, 0, START_MENU_WIDTH, START_MENU_HEIGHT);
    if (!menu->main_widget) {
        return -1;
    }
    
    widget_set_colors(menu->main_widget,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(30, 30, 45, 250),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create sections */
    create_search_box(menu);
    create_user_section(menu);
    create_pinned_section(menu);
    create_app_list(menu);
    create_system_buttons(menu);
    
    return 0;
}

/**
 * create_search_box - Create search box
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
static int create_search_box(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    menu->search_box = edit_create(menu->main_widget, "Search apps...", 10, 10, 380, 40);
    if (!menu->search_box) {
        return -1;
    }
    
    widget_set_colors(menu->search_box,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 55, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Set callback */
    /* In real implementation, would set on_text_changed */
    
    return 0;
}

/**
 * create_user_section - Create user section
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
static int create_user_section(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    /* User icon */
    widget_t *user_icon = panel_create(menu->main_widget, 10, 60, 50, 50);
    if (!user_icon) {
        return -1;
    }
    
    widget_set_colors(user_icon,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(60, 60, 80, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Username */
    widget_t *username_label = label_create(menu->main_widget, menu->username, 70, 70, 200, 30);
    if (!username_label) {
        return -1;
    }
    
    widget_set_font(username_label, 0, 14);
    widget_set_colors(username_label,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_pinned_section - Create pinned apps section
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
static int create_pinned_section(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    /* Section title */
    widget_t *title = label_create(menu->main_widget, "Pinned", 10, 120, 100, 25);
    if (!title) {
        return -1;
    }
    
    widget_set_font(title, 0, 12);
    widget_set_colors(title,
                      color_from_rgba(180, 180, 190, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Pinned apps would be added here */
    
    return 0;
}

/**
 * create_app_list - Create app list section
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
static int create_app_list(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    /* Section title */
    widget_t *title = label_create(menu->main_widget, "All Apps", 10, 280, 100, 25);
    if (!title) {
        return -1;
    }
    
    widget_set_font(title, 0, 12);
    widget_set_colors(title,
                      color_from_rgba(180, 180, 190, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    /* App list would be added here */
    
    return 0;
}

/**
 * create_system_buttons - Create system buttons
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
static int create_system_buttons(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    /* Settings button */
    menu->settings_button = button_create(menu->main_widget, "⚙ Settings", 10, 500, 120, 35);
    if (menu->settings_button) {
        menu->settings_button->on_click = on_settings_clicked;
        menu->settings_button->user_data = menu;
        widget_set_colors(menu->settings_button,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(60, 60, 80, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    /* Lock button */
    menu->lock_button = button_create(menu->main_widget, "🔒 Lock", 140, 500, 80, 35);
    if (menu->lock_button) {
        menu->lock_button->on_click = on_lock_clicked;
        menu->lock_button->user_data = menu;
        widget_set_colors(menu->lock_button,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(60, 60, 80, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    /* Logout button */
    menu->logout_button = button_create(menu->main_widget, "🚪 Logout", 230, 500, 80, 35);
    if (menu->logout_button) {
        menu->logout_button->on_click = on_logout_clicked;
        menu->logout_button->user_data = menu;
        widget_set_colors(menu->logout_button,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(60, 60, 80, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    /* Restart button */
    menu->restart_button = button_create(menu->main_widget, "⟳ Restart", 320, 500, 70, 35);
    if (menu->restart_button) {
        menu->restart_button->on_click = on_restart_clicked;
        menu->restart_button->user_data = menu;
        widget_set_colors(menu->restart_button,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(60, 60, 80, 255),
                          color_from_rgba(80, 80, 100, 255));
    }
    
    /* Shutdown button */
    menu->shutdown_button = button_create(menu->main_widget, "⏻", 20, 545, 370, 40);
    if (menu->shutdown_button) {
        menu->shutdown_button->on_click = on_shutdown_clicked;
        menu->shutdown_button->user_data = menu;
        widget_set_colors(menu->shutdown_button,
                          color_from_rgba(255, 255, 255, 255),
                          color_from_rgba(180, 50, 50, 255),
                          color_from_rgba(200, 60, 60, 255));
    }
    
    return 0;
}

/*===========================================================================*/
/*                         APP MANAGEMENT                                    */
/*===========================================================================*/

/**
 * load_system_apps - Load system applications
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
static int load_system_apps(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    /* Define system apps */
    app_entry_t system_apps[] = {
        {
            .app_id = 1,
            .name = "File Explorer",
            .description = "Browse files and folders",
            .icon_path = "/usr/share/icons/file-explorer.png",
            .executable = "/usr/bin/file-explorer",
            .category = "System",
            .is_pinned = true,
            .is_system = true
        },
        {
            .app_id = 2,
            .name = "Terminal",
            .description = "Command line interface",
            .icon_path = "/usr/share/icons/terminal.png",
            .executable = "/usr/bin/terminal",
            .category = "System",
            .is_pinned = true,
            .is_system = true
        },
        {
            .app_id = 3,
            .name = "Settings",
            .description = "System settings",
            .icon_path = "/usr/share/icons/settings.png",
            .executable = "/usr/bin/settings",
            .category = "System",
            .is_pinned = true,
            .is_system = true
        },
        {
            .app_id = 4,
            .name = "App Store",
            .description = "Install applications",
            .icon_path = "/usr/share/icons/app-store.png",
            .executable = "/usr/bin/app-store",
            .category = "System",
            .is_pinned = true,
            .is_system = true
        },
        {
            .app_id = 5,
            .name = "Text Editor",
            .description = "Edit text files",
            .icon_path = "/usr/share/icons/text-editor.png",
            .executable = "/usr/bin/text-editor",
            .category = "Utilities",
            .is_pinned = false,
            .is_system = true
        },
        {
            .app_id = 6,
            .name = "Storage Manager",
            .description = "Manage storage devices",
            .icon_path = "/usr/share/icons/storage.png",
            .executable = "/usr/bin/storage-manager",
            .category = "System",
            .is_pinned = false,
            .is_system = true
        }
    };
    
    /* Add apps to menu */
    for (u32 i = 0; i < sizeof(system_apps) / sizeof(system_apps[0]); i++) {
        start_menu_add_app(menu, &system_apps[i]);
        
        /* Add to pinned if marked */
        if (system_apps[i].is_pinned && menu->pinned_count < START_MENU_MAX_PINNED) {
            menu->pinned_apps[menu->pinned_count++] = system_apps[i];
        }
    }
    
    return 0;
}

/**
 * start_menu_add_app - Add application to start menu
 * @menu: Pointer to start menu structure
 * @app: Application entry
 *
 * Returns: 0 on success, negative error code on failure
 */
int start_menu_add_app(start_menu_t *menu, app_entry_t *app)
{
    if (!menu || !app) {
        return -EINVAL;
    }
    
    /* Find appropriate section */
    for (u32 i = 0; i < menu->section_count; i++) {
        if (strcmp(menu->sections[i].name, app->category) == 0) {
            /* Add to existing section */
            /* In real implementation, would add to section's app list */
            return 0;
        }
    }
    
    /* Create new section if needed */
    if (menu->section_count < 8) {
        start_menu_section_t *section = &menu->sections[menu->section_count++];
        strncpy(section->name, app->category, sizeof(section->name) - 1);
        section->section_id = menu->section_count;
        section->is_expanded = true;
    }
    
    return 0;
}

/**
 * start_menu_launch_app - Launch application
 * @menu: Pointer to start menu structure
 * @app_id: Application ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int start_menu_launch_app(start_menu_t *menu, u32 app_id)
{
    if (!menu) {
        return -EINVAL;
    }
    
    printk("[START-MENU] Launching app %d\n", app_id);
    
    /* In real implementation, would launch application */
    
    /* Hide start menu */
    start_menu_hide(menu);
    
    if (menu->on_app_launched) {
        /* Would pass app entry to callback */
        menu->on_app_launched(NULL);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         SEARCH                                            */
/*===========================================================================*/

/**
 * start_menu_search - Search applications
 * @menu: Pointer to start menu structure
 * @query: Search query
 *
 * Returns: 0 on success, negative error code on failure
 */
int start_menu_search(start_menu_t *menu, const char *query)
{
    if (!menu || !query) {
        return -EINVAL;
    }
    
    strncpy(menu->search_query, query, sizeof(menu->search_query) - 1);
    
    return search_apps(menu, query);
}

/**
 * search_apps - Search applications internally
 * @menu: Pointer to start menu structure
 * @query: Search query
 *
 * Returns: Number of results
 */
static int search_apps(start_menu_t *menu, const char *query)
{
    if (!menu || !query) {
        return 0;
    }
    
    menu->search_result_count = 0;
    
    /* In real implementation, would search through apps */
    
    return menu->search_result_count;
}

/**
 * start_menu_clear_search - Clear search
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_clear_search(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    menu->search_query[0] = '\0';
    menu->search_result_count = 0;
    
    return 0;
}

/*===========================================================================*/
/*                         SYSTEM ACTIONS                                    */
/*===========================================================================*/

/**
 * start_menu_lock - Lock system
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_lock(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    printk("[START-MENU] Locking system\n");
    
    start_menu_hide(menu);
    
    /* In real implementation, would lock session */
    
    return 0;
}

/**
 * start_menu_logout - Logout user
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_logout(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    printk("[START-MENU] Logging out\n");
    
    start_menu_hide(menu);
    
    /* In real implementation, would end session */
    
    return 0;
}

/**
 * start_menu_shutdown - Shutdown system
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_shutdown(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    printk("[START-MENU] Shutting down system\n");
    
    start_menu_hide(menu);
    
    /* In real implementation, would shutdown system */
    
    if (menu->on_shutdown_clicked) {
        menu->on_shutdown_clicked();
    }
    
    return 0;
}

/**
 * start_menu_restart - Restart system
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_restart(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    printk("[START-MENU] Restarting system\n");
    
    start_menu_hide(menu);
    
    /* In real implementation, would restart system */
    
    return 0;
}

/**
 * start_menu_open_settings - Open settings
 * @menu: Pointer to start menu structure
 *
 * Returns: 0 on success
 */
int start_menu_open_settings(start_menu_t *menu)
{
    if (!menu) {
        return -EINVAL;
    }
    
    printk("[START-MENU] Opening settings\n");
    
    start_menu_hide(menu);
    
    if (menu->on_settings_clicked) {
        menu->on_settings_clicked();
    }
    
    return 0;
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

static void on_search_changed(widget_t *widget, const char *text)
{
    if (!widget || !widget->user_data || !text) return;
    
    start_menu_t *menu = (start_menu_t *)widget->user_data;
    start_menu_search(menu, text);
}

static void on_app_clicked(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    
    /* In real implementation, would get app_id from widget */
    start_menu_t *menu = (start_menu_t *)widget->user_data;
    start_menu_launch_app(menu, 0);
}

static void on_settings_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    start_menu_t *menu = (start_menu_t *)button->user_data;
    start_menu_open_settings(menu);
}

static void on_shutdown_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    start_menu_t *menu = (start_menu_t *)button->user_data;
    start_menu_shutdown(menu);
}

static void on_restart_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    start_menu_t *menu = (start_menu_t *)button->user_data;
    start_menu_restart(menu);
}

static void on_logout_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    start_menu_t *menu = (start_menu_t *)button->user_data;
    start_menu_logout(menu);
}

static void on_lock_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    start_menu_t *menu = (start_menu_t *)button->user_data;
    start_menu_lock(menu);
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * start_menu_get_instance - Get global start menu instance
 *
 * Returns: Pointer to global instance
 */
start_menu_t *start_menu_get_instance(void)
{
    return &g_start_menu;
}
