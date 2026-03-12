/*
 * NEXUS OS - First-Time Onboarding Wizard Implementation
 * gui/onboarding/onboarding-wizard.c
 *
 * Welcome wizard for new users after first login
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "onboarding-wizard.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL ONBOARDING WIZARD INSTANCE                 */
/*===========================================================================*/

static onboarding_wizard_t g_onboarding;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* Page handlers */
static bool page_welcome_on_enter(onboarding_page_t *page);
static bool page_welcome_on_next(onboarding_page_t *page);
static bool page_theme_on_enter(onboarding_page_t *page);
static bool page_theme_on_next(onboarding_page_t *page);
static bool page_desktop_on_enter(onboarding_page_t *page);
static bool page_desktop_on_next(onboarding_page_t *page);
static bool page_taskbar_on_enter(onboarding_page_t *page);
static bool page_taskbar_on_next(onboarding_page_t *page);
static bool page_start_menu_on_enter(onboarding_page_t *page);
static bool page_start_menu_on_next(onboarding_page_t *page);
static bool page_windows_on_enter(onboarding_page_t *page);
static bool page_windows_on_next(onboarding_page_t *page);
static bool page_workspaces_on_enter(onboarding_page_t *page);
static bool page_workspaces_on_next(onboarding_page_t *page);
static bool page_files_on_enter(onboarding_page_t *page);
static bool page_files_on_next(onboarding_page_t *page);
static bool page_apps_on_enter(onboarding_page_t *page);
static bool page_apps_on_next(onboarding_page_t *page);
static bool page_settings_on_enter(onboarding_page_t *page);
static bool page_settings_on_next(onboarding_page_t *page);
static bool page_terminal_on_enter(onboarding_page_t *page);
static bool page_terminal_on_next(onboarding_page_t *page);
static bool page_shortcuts_on_enter(onboarding_page_t *page);
static bool page_shortcuts_on_next(onboarding_page_t *page);
static bool page_gestures_on_enter(onboarding_page_t *page);
static bool page_gestures_on_next(onboarding_page_t *page);
static bool page_notifications_on_enter(onboarding_page_t *page);
static bool page_notifications_on_next(onboarding_page_t *page);
static bool page_search_on_enter(onboarding_page_t *page);
static bool page_search_on_next(onboarding_page_t *page);
static bool page_complete_on_enter(onboarding_page_t *page);

/* UI Creation */
static int create_main_window(onboarding_wizard_t *wizard);
static int create_sidebar(onboarding_wizard_t *wizard);
static int create_content_area(onboarding_wizard_t *wizard);
static int create_header(onboarding_wizard_t *wizard);
static int create_illustration(onboarding_wizard_t *wizard);
static int create_description(onboarding_wizard_t *wizard);
static int create_buttons(onboarding_wizard_t *wizard);
static int create_progress_indicator(onboarding_wizard_t *wizard);

/* Updates */
static void update_progress(onboarding_wizard_t *wizard);
static void update_buttons(onboarding_wizard_t *wizard);
static void update_animation(onboarding_wizard_t *wizard);

/* Event handlers */
static void on_back_clicked(widget_t *button);
static void on_next_clicked(widget_t *button);
static void on_skip_clicked(widget_t *button);
static void on_finish_clicked(widget_t *button);
static void on_close_clicked(widget_t *button);

/*===========================================================================*/
/*                         ONBOARDING WIZARD INITIALIZATION                  */
/*===========================================================================*/

/**
 * onboarding_wizard_init - Initialize the onboarding wizard
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int onboarding_wizard_init(onboarding_wizard_t *wizard)
{
    if (!wizard) {
        return -EINVAL;
    }
    
    printk("[ONBOARD] ========================================\n");
    printk("[ONBOARD] NEXUS OS Onboarding Wizard v%s\n", ONBOARDING_VERSION);
    printk("[ONBOARD] ========================================\n");
    printk("[ONBOARD] Initializing onboarding wizard...\n");
    
    /* Clear structure */
    memset(wizard, 0, sizeof(onboarding_wizard_t));
    wizard->initialized = true;
    wizard->state = ONBOARDING_STATE_IDLE;
    wizard->current_page = 0;
    wizard->total_pages = 0;
    wizard->completed_pages = 0;
    wizard->selected_theme = ONBOARDING_THEME_DEFAULT;
    wizard->enable_animations = true;
    wizard->enable_sounds = true;
    wizard->enable_tips = true;
    wizard->show_tutorials = true;
    wizard->difficulty_level = 1; /* Normal */
    
    /* Load tips */
    printk("[ONBOARD] Loading tips...\n");
    
    quick_tip_t tips[] = {
        {
            .tip_id = 1,
            .category = "desktop",
            .title = "Desktop Icons",
            .content = "Right-click on the desktop to create new files, folders, or change desktop settings.",
            .icon = "desktop",
            .priority = 1,
            .is_interactive = true,
        },
        {
            .tip_id = 2,
            .category = "taskbar",
            .title = "Taskbar Tips",
            .content = "Pin your favorite apps to the taskbar for quick access. Right-click taskbar to customize.",
            .icon = "taskbar",
            .priority = 2,
            .is_interactive = true,
        },
        {
            .tip_id = 3,
            .category = "windows",
            .title = "Window Management",
            .content = "Drag windows to edges to snap them. Use Super+Arrow keys for keyboard snapping.",
            .icon = "windows",
            .priority = 3,
            .is_interactive = true,
        },
        {
            .tip_id = 4,
            .category = "workspaces",
            .title = "Virtual Desktops",
            .content = "Create multiple workspaces to organize your work. Use Ctrl+Alt+Arrow to switch.",
            .icon = "workspaces",
            .priority = 4,
            .is_interactive = true,
        },
        {
            .tip_id = 5,
            .category = "search",
            .title = "Universal Search",
            .content = "Press Super key to open search. Search for apps, files, settings, and web.",
            .icon = "search",
            .priority = 5,
            .is_interactive = true,
        },
        {
            .tip_id = 6,
            .category = "notifications",
            .title = "Notification Center",
            .content = "Click the bell icon to view notifications. Configure app notifications in Settings.",
            .icon = "notifications",
            .priority = 6,
            .is_interactive = true,
        },
        {
            .tip_id = 7,
            .category = "shortcuts",
            .title = "Keyboard Shortcuts",
            .content = "Press Super+H to view all keyboard shortcuts. Customize them in Settings.",
            .icon = "shortcuts",
            .priority = 7,
            .is_interactive = true,
        },
        {
            .tip_id = 8,
            .category = "terminal",
            .title = "Terminal Tips",
            .content = "Use Ctrl+Shift+T for new tab, Ctrl+Shift+W to close. Right-click for context menu.",
            .icon = "terminal",
            .priority = 8,
            .is_interactive = true,
        },
    };
    
    wizard->tip_count = sizeof(tips) / sizeof(tips[0]);
    for (u32 i = 0; i < wizard->tip_count && i < ONBOARDING_MAX_TIPS; i++) {
        wizard->tips[i] = tips[i];
    }
    
    /* Load keyboard shortcuts */
    printk("[ONBOARD] Loading keyboard shortcuts...\n");
    
    keyboard_shortcut_t shortcuts[] = {
        {
            .shortcut_id = 1,
            .category = "system",
            .description = "Open Start Menu",
            .keys = "Super",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 2,
            .category = "system",
            .description = "Open Search",
            .keys = "Super+S",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 3,
            .category = "windows",
            .description = "Close Window",
            .keys = "Alt+F4",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 4,
            .category = "windows",
            .description = "Maximize Window",
            .keys = "Super+Up",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 5,
            .category = "windows",
            .description = "Minimize Window",
            .keys = "Super+Down",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 6,
            .category = "windows",
            .description = "Snap Left",
            .keys = "Super+Left",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 7,
            .category = "windows",
            .description = "Snap Right",
            .keys = "Super+Right",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 8,
            .category = "workspaces",
            .description = "Switch Workspace Left",
            .keys = "Ctrl+Alt+Left",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 9,
            .category = "workspaces",
            .description = "Switch Workspace Right",
            .keys = "Ctrl+Alt+Right",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 10,
            .category = "terminal",
            .description = "New Terminal Tab",
            .keys = "Ctrl+Shift+T",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 11,
            .category = "terminal",
            .description = "Close Terminal Tab",
            .keys = "Ctrl+Shift+W",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 12,
            .category = "system",
            .description = "Lock Screen",
            .keys = "Super+L",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 13,
            .category = "system",
            .description = "Show Desktop",
            .keys = "Super+D",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 14,
            .category = "system",
            .description = "Screenshot",
            .keys = "PrintScreen",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
        {
            .shortcut_id = 15,
            .category = "system",
            .description = "Window Screenshot",
            .keys = "Alt+PrintScreen",
            .modifiers = 0,
            .key_code = 0,
            .is_customizable = true,
        },
    };
    
    wizard->shortcut_count = sizeof(shortcuts) / sizeof(shortcuts[0]);
    for (u32 i = 0; i < wizard->shortcut_count && i < ONBOARDING_MAX_SHORTCUTS; i++) {
        wizard->shortcuts[i] = shortcuts[i];
    }
    
    /* Load features */
    printk("[ONBOARD] Loading features...\n");
    
    feature_intro_t features[] = {
        {
            .feature_id = 1,
            .name = "App Store",
            .description = "Browse and install applications from the NEXUS App Store.",
            .icon = "app-store",
            .is_enabled = true,
            .is_new = false,
            .category = 1,
        },
        {
            .feature_id = 2,
            .name = "Virtual Desktops",
            .description = "Organize your work with multiple virtual desktops.",
            .icon = "workspaces",
            .is_enabled = true,
            .is_new = true,
            .category = 2,
        },
        {
            .feature_id = 3,
            .name = "Snap Layouts",
            .description = "Snap windows into predefined layouts for better multitasking.",
            .icon = "snap",
            .is_enabled = true,
            .is_new = true,
            .category = 2,
        },
        {
            .feature_id = 4,
            .name = "Night Light",
            .description = "Reduce blue light in the evening for better sleep.",
            .icon = "night-light",
            .is_enabled = false,
            .is_new = true,
            .category = 3,
        },
        {
            .feature_id = 5,
            .name = "Focus Mode",
            .description = "Minimize distractions by hiding notifications.",
            .icon = "focus",
            .is_enabled = false,
            .is_new = true,
            .category = 3,
        },
        {
            .feature_id = 6,
            .name = "Clipboard History",
            .description = "Access your clipboard history with Super+V.",
            .icon = "clipboard",
            .is_enabled = true,
            .is_new = true,
            .category = 4,
        },
    };
    
    wizard->feature_count = sizeof(features) / sizeof(features[0]);
    for (u32 i = 0; i < wizard->feature_count && i < 64; i++) {
        wizard->features[i] = features[i];
    }
    
    /* Create UI */
    printk("[ONBOARD] Creating UI...\n");
    
    if (create_main_window(wizard) != 0) {
        printk("[ONBOARD] Failed to create main window\n");
        return -1;
    }
    
    if (create_sidebar(wizard) != 0) {
        printk("[ONBOARD] Failed to create sidebar\n");
        return -1;
    }
    
    if (create_content_area(wizard) != 0) {
        printk("[ONBOARD] Failed to create content area\n");
        return -1;
    }
    
    if (create_header(wizard) != 0) {
        printk("[ONBOARD] Failed to create header\n");
        return -1;
    }
    
    if (create_illustration(wizard) != 0) {
        printk("[ONBOARD] Failed to create illustration\n");
        return -1;
    }
    
    if (create_description(wizard) != 0) {
        printk("[ONBOARD] Failed to create description\n");
        return -1;
    }
    
    if (create_buttons(wizard) != 0) {
        printk("[ONBOARD] Failed to create buttons\n");
        return -1;
    }
    
    if (create_progress_indicator(wizard) != 0) {
        printk("[ONBOARD] Failed to create progress indicator\n");
        return -1;
    }
    
    /* Register all onboarding pages */
    onboarding_page_t pages[] = {
        /* Page 0: Welcome */
        {
            .page_id = 0,
            .title = "Welcome to NEXUS OS",
            .subtitle = "Let's get you started",
            .description = "Welcome to your new NEXUS OS experience! This quick tour will help you get familiar with the key features and get you up and running in no time. The tour takes about 5 minutes.",
            .icon = "welcome",
            .illustration = "welcome.svg",
            .theme = ONBOARDING_THEME_COLORFUL,
            .is_skipable = true,
            .is_required = false,
            .on_enter = page_welcome_on_enter,
            .on_next = page_welcome_on_next,
        },
        
        /* Page 1: Theme Selection */
        {
            .page_id = 1,
            .title = "Choose Your Theme",
            .subtitle = "Personalize your experience",
            .description = "Select a visual theme that matches your style. You can always change this later in Settings.",
            .icon = "theme",
            .illustration = "themes.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = false,
            .on_enter = page_theme_on_enter,
            .on_next = page_theme_on_next,
        },
        
        /* Page 2: Desktop Overview */
        {
            .page_id = 2,
            .title = "Your Desktop",
            .subtitle = "Your personal workspace",
            .description = "The desktop is your main workspace. You can place files, folders, and app shortcuts here for quick access. Right-click for more options.",
            .icon = "desktop",
            .illustration = "desktop.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_desktop_on_enter,
            .on_next = page_desktop_on_next,
        },
        
        /* Page 3: Taskbar */
        {
            .page_id = 3,
            .title = "Taskbar & System Tray",
            .subtitle = "Quick access to your apps",
            .description = "The taskbar shows your open apps and provides quick access to favorites. The system tray shows system status like network, sound, and battery.",
            .icon = "taskbar",
            .illustration = "taskbar.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_taskbar_on_enter,
            .on_next = page_taskbar_on_next,
        },
        
        /* Page 4: Start Menu */
        {
            .page_id = 4,
            .title = "Start Menu",
            .subtitle = "Access all your apps",
            .description = "Click the NEXUS button or press Super to open the Start Menu. Find apps, search, access settings, and power options all in one place.",
            .icon = "start-menu",
            .illustration = "start-menu.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_start_menu_on_enter,
            .on_next = page_start_menu_on_next,
        },
        
        /* Page 5: Windows */
        {
            .page_id = 5,
            .title = "Window Management",
            .subtitle = "Organize your work",
            .description = "Drag windows to screen edges to snap them into place. Use the window controls to minimize, maximize, or close. Try Super+Arrow keys for keyboard snapping.",
            .icon = "windows",
            .illustration = "windows.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_windows_on_enter,
            .on_next = page_windows_on_next,
        },
        
        /* Page 6: Workspaces */
        {
            .page_id = 6,
            .title = "Virtual Desktops",
            .subtitle = "Organize by task",
            .description = "Create separate desktops for different tasks. Keep work on one, personal on another. Switch with Ctrl+Alt+Arrow or the workspace switcher.",
            .icon = "workspaces",
            .illustration = "workspaces.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_workspaces_on_enter,
            .on_next = page_workspaces_on_next,
        },
        
        /* Page 7: Files */
        {
            .page_id = 7,
            .title = "File Manager",
            .subtitle = "Browse your files",
            .description = "The File Manager helps you browse, organize, and manage your files. Access it from the taskbar or by clicking any folder on the desktop.",
            .icon = "files",
            .illustration = "files.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_files_on_enter,
            .on_next = page_files_on_next,
        },
        
        /* Page 8: Apps */
        {
            .page_id = 8,
            .title = "Applications & App Store",
            .subtitle = "Extend your system",
            .description = "The App Store offers thousands of applications. Browse categories, read reviews, and install with one click. Your apps stay updated automatically.",
            .icon = "apps",
            .illustration = "app-store.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_apps_on_enter,
            .on_next = page_apps_on_next,
        },
        
        /* Page 9: Settings */
        {
            .page_id = 9,
            .title = "System Settings",
            .subtitle = "Customize everything",
            .description = "Settings gives you complete control over your system. Customize appearance, configure hardware, manage accounts, and much more.",
            .icon = "settings",
            .illustration = "settings.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_settings_on_enter,
            .on_next = page_settings_on_next,
        },
        
        /* Page 10: Terminal */
        {
            .page_id = 10,
            .title = "Terminal",
            .subtitle = "Power user tools",
            .description = "The Terminal provides command-line access for advanced tasks. Use tabs for multiple sessions. Try Ctrl+Shift+T for new tab.",
            .icon = "terminal",
            .illustration = "terminal.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = false,
            .on_enter = page_terminal_on_enter,
            .on_next = page_terminal_on_next,
        },
        
        /* Page 11: Keyboard Shortcuts */
        {
            .page_id = 11,
            .title = "Keyboard Shortcuts",
            .subtitle = "Work faster",
            .description = "Keyboard shortcuts help you work more efficiently. Press Super+H to view all shortcuts. Customize them in Settings > Keyboard.",
            .icon = "shortcuts",
            .illustration = "shortcuts.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = false,
            .on_enter = page_shortcuts_on_enter,
            .on_next = page_shortcuts_on_next,
        },
        
        /* Page 12: Touchpad Gestures */
        {
            .page_id = 12,
            .title = "Touchpad Gestures",
            .subtitle = "Natural navigation",
            .description = "Use three-finger swipe to switch workspaces, four-finger pinch to show all windows. Configure gestures in Settings.",
            .icon = "gestures",
            .illustration = "gestures.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = false,
            .on_enter = page_gestures_on_enter,
            .on_next = page_gestures_on_next,
        },
        
        /* Page 13: Notifications */
        {
            .page_id = 13,
            .title = "Notifications & Calendar",
            .subtitle = "Stay informed",
            .description = "Click the date/time to see notifications and calendar. Configure which apps can send notifications in Settings.",
            .icon = "notifications",
            .illustration = "notifications.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_notifications_on_enter,
            .on_next = page_notifications_on_next,
        },
        
        /* Page 14: Search */
        {
            .page_id = 14,
            .title = "Universal Search",
            .subtitle = "Find anything instantly",
            .description = "Press Super to search for apps, files, settings, or web results. Type naturally and get instant results.",
            .icon = "search",
            .illustration = "search.svg",
            .theme = ONBOARDING_THEME_DEFAULT,
            .is_skipable = true,
            .is_required = true,
            .on_enter = page_search_on_enter,
            .on_next = page_search_on_next,
        },
        
        /* Page 15: Complete */
        {
            .page_id = 15,
            .title = "You're All Set!",
            .subtitle = "Ready to explore",
            .description = "Congratulations! You've completed the NEXUS OS tour. You're now ready to explore and make the most of your new operating system. Enjoy!",
            .icon = "complete",
            .illustration = "complete.svg",
            .theme = ONBOARDING_THEME_COLORFUL,
            .is_skipable = false,
            .is_required = false,
            .on_enter = page_complete_on_enter,
        },
    };
    
    /* Add all pages */
    u32 num_pages = sizeof(pages) / sizeof(pages[0]);
    for (u32 i = 0; i < num_pages; i++) {
        onboarding_wizard_add_page(wizard, &pages[i]);
    }
    
    printk("[ONBOARD] Onboarding wizard initialized (%d pages)\n", wizard->total_pages);
    printk("[ONBOARD] ========================================\n");
    
    return 0;
}

/**
 * onboarding_wizard_run - Start the onboarding wizard
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int onboarding_wizard_run(onboarding_wizard_t *wizard)
{
    if (!wizard || !wizard->initialized) {
        return -EINVAL;
    }
    
    printk("[ONBOARD] Starting onboarding wizard...\n");
    
    wizard->active = true;
    wizard->visible = true;
    wizard->state = ONBOARDING_STATE_RUNNING;
    wizard->start_time = get_time_ms();
    
    /* Show main window */
    if (wizard->main_window) {
        window_show(wizard->main_window);
    }
    
    /* Go to first page */
    onboarding_wizard_goto_page(wizard, 0);
    
    printk("[ONBOARD] Onboarding wizard started\n");
    
    return 0;
}

/**
 * onboarding_wizard_shutdown - Shutdown the onboarding wizard
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int onboarding_wizard_shutdown(onboarding_wizard_t *wizard)
{
    if (!wizard || !wizard->initialized) {
        return -EINVAL;
    }
    
    printk("[ONBOARD] Shutting down onboarding wizard...\n");
    
    wizard->active = false;
    wizard->visible = false;
    
    /* Hide main window */
    if (wizard->main_window) {
        window_hide(wizard->main_window);
    }
    
    printk("[ONBOARD] Onboarding wizard shutdown complete\n");
    
    return 0;
}

/**
 * onboarding_wizard_complete - Mark onboarding as completed
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success
 */
int onboarding_wizard_complete(onboarding_wizard_t *wizard)
{
    if (!wizard) return -EINVAL;
    
    wizard->completed = true;
    wizard->state = ONBOARDING_STATE_COMPLETED;
    wizard->total_time = get_time_ms() - wizard->start_time;
    
    printk("[ONBOARD] Onboarding completed in %d ms\n", (u32)wizard->total_time);
    
    if (wizard->on_completed) {
        wizard->on_completed(wizard);
    }
    
    return 0;
}

/**
 * onboarding_wizard_skip - Skip the onboarding
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success
 */
int onboarding_wizard_skip(onboarding_wizard_t *wizard)
{
    if (!wizard) return -EINVAL;
    
    wizard->skipped = true;
    wizard->state = ONBOARDING_STATE_SKIPPED;
    
    printk("[ONBOARD] Onboarding skipped by user\n");
    
    if (wizard->on_skipped) {
        wizard->on_skipped(wizard);
    }
    
    return 0;
}

/**
 * onboarding_wizard_is_active - Check if onboarding is active
 * @wizard: Pointer to wizard structure
 *
 * Returns: true if active, false otherwise
 */
bool onboarding_wizard_is_active(onboarding_wizard_t *wizard)
{
    return wizard && wizard->active;
}

/**
 * onboarding_wizard_is_completed - Check if onboarding is completed
 * @wizard: Pointer to wizard structure
 *
 * Returns: true if completed, false otherwise
 */
bool onboarding_wizard_is_completed(onboarding_wizard_t *wizard)
{
    return wizard && wizard->completed;
}

/*===========================================================================*/
/*                         PAGE NAVIGATION                                   */
/*===========================================================================*/

/**
 * onboarding_wizard_add_page - Add a page to the wizard
 * @wizard: Pointer to wizard structure
 * @page: Pointer to page structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int onboarding_wizard_add_page(onboarding_wizard_t *wizard, onboarding_page_t *page)
{
    if (!wizard || !page) return -EINVAL;
    
    if (wizard->total_pages >= ONBOARDING_MAX_PAGES) {
        return -1;
    }
    
    memcpy(&wizard->pages[wizard->total_pages], page, sizeof(onboarding_page_t));
    wizard->total_pages++;
    
    return 0;
}

/**
 * onboarding_wizard_goto_page - Navigate to a specific page
 * @wizard: Pointer to wizard structure
 * @page_id: Page ID to navigate to
 *
 * Returns: 0 on success, negative error code on failure
 */
int onboarding_wizard_goto_page(onboarding_wizard_t *wizard, u32 page_id)
{
    if (!wizard || page_id >= wizard->total_pages) {
        return -EINVAL;
    }
    
    onboarding_page_t *current = &wizard->pages[wizard->current_page];
    onboarding_page_t *next = &wizard->pages[page_id];
    
    printk("[ONBOARD] Navigating to page %d: %s\n", page_id, next->title);
    
    /* Call on_leave for current page */
    if (current && current->on_leave) {
        current->page_times[wizard->current_page] = get_time_ms();
        if (!current->on_leave(current)) {
            printk("[ONBOARD] Page %d on_leave returned false\n", wizard->current_page);
        }
    }
    
    /* Update current page */
    wizard->current_page = page_id;
    wizard->completed_pages = page_id;
    
    /* Update UI */
    update_progress(wizard);
    update_buttons(wizard);
    
    /* Call on_enter for new page */
    if (next->on_enter) {
        if (!next->on_enter(next)) {
            printk("[ONBOARD] Page %d on_enter failed\n", page_id);
        }
    }
    
    /* Update header */
    if (wizard->title_label) {
        widget_set_text(wizard->title_label, next->title);
    }
    
    if (wizard->subtitle_label) {
        widget_set_text(wizard->subtitle_label, next->subtitle);
    }
    
    if (wizard->description_label) {
        widget_set_text(wizard->description_label, next->description);
    }
    
    /* Call callback */
    if (wizard->on_page_changed) {
        wizard->on_page_changed(wizard, page_id);
    }
    
    return 0;
}

/**
 * onboarding_wizard_next_page - Go to next page
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int onboarding_wizard_next_page(onboarding_wizard_t *wizard)
{
    if (!wizard) return -EINVAL;
    
    onboarding_page_t *current = &wizard->pages[wizard->current_page];
    
    /* Call on_next handler */
    if (current->on_next) {
        if (!current->on_next(current)) {
            return -1;
        }
    }
    
    /* Go to next page */
    if (wizard->current_page < wizard->total_pages - 1) {
        return onboarding_wizard_goto_page(wizard, wizard->current_page + 1);
    }
    
    return 0;
}

/**
 * onboarding_wizard_previous_page - Go to previous page
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int onboarding_wizard_previous_page(onboarding_wizard_t *wizard)
{
    if (!wizard) return -EINVAL;
    
    onboarding_page_t *current = &wizard->pages[wizard->current_page];
    
    /* Call on_back handler */
    if (current->on_back) {
        if (!current->on_back(current)) {
            printk("[ONBOARD] Page %d on_back returned false\n", wizard->current_page);
        }
    }
    
    /* Go to previous page */
    if (wizard->current_page > 0) {
        return onboarding_wizard_goto_page(wizard, wizard->current_page - 1);
    }
    
    return 0;
}

/**
 * onboarding_wizard_get_current_page - Get current page
 * @wizard: Pointer to wizard structure
 *
 * Returns: Pointer to current page, or NULL on error
 */
onboarding_page_t *onboarding_wizard_get_current_page(onboarding_wizard_t *wizard)
{
    if (!wizard) return NULL;
    return &wizard->pages[wizard->current_page];
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_main_window - Create the main onboarding window
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_main_window(onboarding_wizard_t *wizard)
{
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "NEXUS OS - Getting Started");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_MODAL | WINDOW_FLAG_RESIZABLE;
    props.bounds.x = (1920 - 1000) / 2;
    props.bounds.y = (1080 - 700) / 2;
    props.bounds.width = 1000;
    props.bounds.height = 700;
    props.background = color_from_rgba(30, 30, 45, 255);
    
    wizard->main_window = window_create(&props);
    if (!wizard->main_window) {
        return -1;
    }
    
    wizard->main_widget = panel_create(NULL, 0, 0, 1000, 700);
    if (!wizard->main_widget) {
        return -1;
    }
    
    return 0;
}

/**
 * create_sidebar - Create the sidebar with page indicators
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_sidebar(onboarding_wizard_t *wizard)
{
    wizard->sidebar = panel_create(wizard->main_widget, 0, 0, 250, 620);
    if (!wizard->sidebar) {
        return -1;
    }
    
    widget_set_colors(wizard->sidebar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(35, 35, 50, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* Create page indicators */
    s32 y = 20;
    for (u32 i = 0; i < wizard->total_pages && i < 16; i++) {
        char label[8];
        snprintf(label, sizeof(label), "%d", i + 1);
        
        widget_t *indicator = label_create(wizard->sidebar, label, 20, y, 30, 30);
        if (indicator) {
            widget_set_font(indicator, 0, 14);
            widget_set_colors(indicator,
                            color_from_rgba(100, 100, 110, 255),
                            color_from_rgba(35, 35, 50, 0),
                            color_from_rgba(0, 0, 0, 0));
        }
        y += 35;
    }
    
    return 0;
}

/**
 * create_content_area - Create the main content area
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_content_area(onboarding_wizard_t *wizard)
{
    wizard->content_area = panel_create(wizard->main_widget, 250, 0, 750, 620);
    if (!wizard->content_area) {
        return -1;
    }
    
    widget_set_colors(wizard->content_area,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(45, 45, 60, 255),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_header - Create the header with title and subtitle
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_header(onboarding_wizard_t *wizard)
{
    wizard->header_widget = panel_create(wizard->content_area, 20, 10, 710, 100);
    if (!wizard->header_widget) {
        return -1;
    }
    
    wizard->title_label = label_create(wizard->header_widget, "Welcome", 0, 0, 710, 40);
    if (!wizard->title_label) return -1;
    
    widget_set_font(wizard->title_label, 0, 28);
    widget_set_colors(wizard->title_label,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    wizard->subtitle_label = label_create(wizard->header_widget, "Let's get started", 0, 45, 710, 30);
    if (!wizard->subtitle_label) return -1;
    
    widget_set_font(wizard->subtitle_label, 0, 16);
    widget_set_colors(wizard->subtitle_label,
                      color_from_rgba(180, 180, 190, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_illustration - Create the illustration area
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_illustration(onboarding_wizard_t *wizard)
{
    wizard->illustration_widget = panel_create(wizard->content_area, 20, 120, 710, 250);
    if (!wizard->illustration_widget) {
        return -1;
    }
    
    widget_set_colors(wizard->illustration_widget,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(55, 55, 75, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    return 0;
}

/**
 * create_description - Create the description area
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_description(onboarding_wizard_t *wizard)
{
    wizard->description_label = label_create(wizard->content_area, "", 20, 380, 710, 150);
    if (!wizard->description_label) {
        return -1;
    }
    
    widget_set_font(wizard->description_label, 0, 14);
    widget_set_colors(wizard->description_label,
                      color_from_rgba(200, 200, 210, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_buttons - Create navigation buttons
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_buttons(onboarding_wizard_t *wizard)
{
    /* Back button */
    wizard->back_button = button_create(wizard->main_widget, "Back", 260, 630, 100, 40);
    if (!wizard->back_button) return -1;
    
    wizard->back_button->on_click = on_back_clicked;
    wizard->back_button->user_data = wizard;
    widget_set_colors(wizard->back_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(60, 60, 75, 255),
                      color_from_rgba(80, 80, 95, 255));
    
    /* Next button */
    wizard->next_button = button_create(wizard->main_widget, "Next", 680, 630, 100, 40);
    if (!wizard->next_button) return -1;
    
    wizard->next_button->on_click = on_next_clicked;
    wizard->next_button->user_data = wizard;
    widget_set_colors(wizard->next_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(0, 120, 215, 255),
                      color_from_rgba(0, 100, 180, 255));
    
    /* Skip button */
    wizard->skip_button = button_create(wizard->main_widget, "Skip Tour", 790, 630, 100, 40);
    if (!wizard->skip_button) return -1;
    
    wizard->skip_button->on_click = on_skip_clicked;
    wizard->skip_button->user_data = wizard;
    widget_set_colors(wizard->skip_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(60, 60, 75, 255),
                      color_from_rgba(80, 80, 95, 255));
    
    /* Finish button (hidden initially) */
    wizard->finish_button = button_create(wizard->main_widget, "Finish", 680, 630, 100, 40);
    if (!wizard->finish_button) return -1;
    
    wizard->finish_button->on_click = on_finish_clicked;
    wizard->finish_button->user_data = wizard;
    widget_set_colors(wizard->finish_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 180, 100, 255),
                      color_from_rgba(30, 150, 80, 255));
    
    widget_hide(wizard->finish_button);
    
    /* Close button (hidden initially) */
    wizard->close_button = button_create(wizard->main_widget, "Close", 790, 630, 100, 40);
    if (!wizard->close_button) return -1;
    
    wizard->close_button->on_click = on_close_clicked;
    wizard->close_button->user_data = wizard;
    widget_set_colors(wizard->close_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(60, 60, 75, 255),
                      color_from_rgba(80, 80, 95, 255));
    
    widget_hide(wizard->close_button);
    
    return 0;
}

/**
 * create_progress_indicator - Create the progress indicator
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_progress_indicator(onboarding_wizard_t *wizard)
{
    wizard->progress_indicator = progressbar_create(wizard->main_widget, 260, 605, 630, 15);
    if (!wizard->progress_indicator) {
        return -1;
    }
    
    wizard->progress_label = label_create(wizard->main_widget, "0%", 260, 585, 630, 15);
    if (!wizard->progress_label) {
        return -1;
    }
    
    widget_set_font(wizard->progress_label, 0, 10);
    widget_set_colors(wizard->progress_label,
                      color_from_rgba(180, 180, 190, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/*===========================================================================*/
/*                         UPDATES                                           */
/*===========================================================================*/

/**
 * update_progress - Update the progress indicator
 * @wizard: Pointer to wizard structure
 */
static void update_progress(onboarding_wizard_t *wizard)
{
    if (!wizard) return;
    
    u32 progress = (wizard->current_page * 100) / wizard->total_pages;
    
    char progress_text[64];
    snprintf(progress_text, sizeof(progress_text), "Step %d of %d (%d%%)",
             wizard->current_page + 1, wizard->total_pages, progress);
    
    if (wizard->progress_label) {
        widget_set_text(wizard->progress_label, progress_text);
    }
}

/**
 * update_buttons - Update button states
 * @wizard: Pointer to wizard structure
 */
static void update_buttons(onboarding_wizard_t *wizard)
{
    if (!wizard) return;
    
    /* Back button */
    if (wizard->back_button) {
        if (wizard->current_page == 0) {
            widget_disable(wizard->back_button);
        } else {
            widget_enable(wizard->back_button);
        }
    }
    
    /* Next/Finish buttons */
    bool is_last_page = (wizard->current_page >= wizard->total_pages - 1);
    
    if (wizard->next_button) {
        if (is_last_page) {
            widget_hide(wizard->next_button);
        } else {
            widget_show(wizard->next_button);
        }
    }
    
    if (wizard->finish_button) {
        if (is_last_page) {
            widget_show(wizard->finish_button);
        } else {
            widget_hide(wizard->finish_button);
        }
    }
    
    /* Skip button */
    if (wizard->skip_button) {
        if (is_last_page) {
            widget_hide(wizard->skip_button);
        } else {
            widget_show(wizard->skip_button);
        }
    }
    
    /* Close button */
    if (wizard->close_button) {
        if (is_last_page) {
            widget_show(wizard->close_button);
        } else {
            widget_hide(wizard->close_button);
        }
    }
}

/**
 * update_animation - Update animation state
 * @wizard: Pointer to wizard structure
 */
static void update_animation(onboarding_wizard_t *wizard)
{
    if (!wizard || wizard->animation_state == 0) return;
    
    u64 elapsed = get_time_ms() - wizard->animation_start;
    wizard->animation_progress = (float)elapsed / 300.0f;
    
    if (wizard->animation_progress >= 1.0f) {
        wizard->animation_progress = 1.0f;
        wizard->animation_state = 0;
    }
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

/**
 * on_back_clicked - Handle back button click
 * @button: Button widget
 */
static void on_back_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    onboarding_wizard_t *wizard = (onboarding_wizard_t *)button->user_data;
    onboarding_wizard_previous_page(wizard);
}

/**
 * on_next_clicked - Handle next button click
 * @button: Button widget
 */
static void on_next_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    onboarding_wizard_t *wizard = (onboarding_wizard_t *)button->user_data;
    onboarding_wizard_next_page(wizard);
}

/**
 * on_skip_clicked - Handle skip button click
 * @button: Button widget
 */
static void on_skip_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    onboarding_wizard_t *wizard = (onboarding_wizard_t *)button->user_data;
    onboarding_wizard_skip(wizard);
    onboarding_wizard_complete(wizard);
}

/**
 * on_finish_clicked - Handle finish button click
 * @button: Button widget
 */
static void on_finish_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    onboarding_wizard_t *wizard = (onboarding_wizard_t *)button->user_data;
    onboarding_wizard_complete(wizard);
}

/**
 * on_close_clicked - Handle close button click
 * @button: Button widget
 */
static void on_close_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    onboarding_wizard_t *wizard = (onboarding_wizard_t *)button->user_data;
    onboarding_wizard_shutdown(wizard);
}

/*===========================================================================*/
/*                         PAGE IMPLEMENTATIONS                              */
/*===========================================================================*/

/* Page 0: Welcome */
static bool page_welcome_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Welcome page\n");
    return true;
}

static bool page_welcome_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 1: Theme */
static bool page_theme_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Theme selection page\n");
    return true;
}

static bool page_theme_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 2: Desktop */
static bool page_desktop_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Desktop overview page\n");
    return true;
}

static bool page_desktop_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 3: Taskbar */
static bool page_taskbar_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Taskbar page\n");
    return true;
}

static bool page_taskbar_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 4: Start Menu */
static bool page_start_menu_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Start menu page\n");
    return true;
}

static bool page_start_menu_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 5: Windows */
static bool page_windows_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Windows management page\n");
    return true;
}

static bool page_windows_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 6: Workspaces */
static bool page_workspaces_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Workspaces page\n");
    return true;
}

static bool page_workspaces_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 7: Files */
static bool page_files_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Files page\n");
    return true;
}

static bool page_files_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 8: Apps */
static bool page_apps_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Apps page\n");
    return true;
}

static bool page_apps_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 9: Settings */
static bool page_settings_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Settings page\n");
    return true;
}

static bool page_settings_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 10: Terminal */
static bool page_terminal_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Terminal page\n");
    return true;
}

static bool page_terminal_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 11: Shortcuts */
static bool page_shortcuts_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Shortcuts page\n");
    return true;
}

static bool page_shortcuts_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 12: Gestures */
static bool page_gestures_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Gestures page\n");
    return true;
}

static bool page_gestures_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 13: Notifications */
static bool page_notifications_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Notifications page\n");
    return true;
}

static bool page_notifications_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 14: Search */
static bool page_search_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Search page\n");
    return true;
}

static bool page_search_on_next(onboarding_page_t *page)
{
    return true;
}

/* Page 15: Complete */
static bool page_complete_on_enter(onboarding_page_t *page)
{
    printk("[ONBOARD] Complete page\n");
    return true;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * onboarding_get_total_pages - Get total number of pages
 * @wizard: Pointer to wizard structure
 *
 * Returns: Total page count
 */
u32 onboarding_get_total_pages(onboarding_wizard_t *wizard)
{
    return wizard ? wizard->total_pages : 0;
}

/**
 * onboarding_get_progress_percent - Get progress percentage
 * @wizard: Pointer to wizard structure
 *
 * Returns: Progress percentage (0-100)
 */
u32 onboarding_get_progress_percent(onboarding_wizard_t *wizard)
{
    if (!wizard || wizard->total_pages == 0) return 0;
    return (wizard->current_page * 100) / wizard->total_pages;
}

/**
 * onboarding_get_page_type_name - Get page type name
 * @type: Page type
 *
 * Returns: Page type name string
 */
const char *onboarding_get_page_type_name(u32 type)
{
    (void)type;
    return "Onboarding";
}

/**
 * onboarding_get_theme_name - Get theme name
 * @theme: Theme ID
 *
 * Returns: Theme name string
 */
const char *onboarding_get_theme_name(u32 theme)
{
    switch (theme) {
        case ONBOARDING_THEME_DEFAULT:  return "Default";
        case ONBOARDING_THEME_DARK:     return "Dark";
        case ONBOARDING_THEME_LIGHT:    return "Light";
        case ONBOARDING_THEME_COLORFUL: return "Colorful";
        case ONBOARDING_THEME_MINIMAL:  return "Minimal";
        default:                        return "Unknown";
    }
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * onboarding_wizard_get_instance - Get the global onboarding wizard instance
 *
 * Returns: Pointer to global onboarding wizard instance
 */
onboarding_wizard_t *onboarding_wizard_get_instance(void)
{
    return &g_onboarding;
}
