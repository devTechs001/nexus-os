/*
 * NEXUS OS - Graphical Setup Wizard Implementation (Part 1: Core)
 * gui/setup/setup-wizard.c
 *
 * Complete graphical setup wizard for NEXUS OS installation
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "setup-wizard.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL WIZARD INSTANCE                            */
/*===========================================================================*/

static setup_wizard_t g_wizard;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* Page handlers - Welcome */
static bool page_welcome_on_enter(setup_page_t *page);
static bool page_welcome_on_next(setup_page_t *page);

/* Page handlers - Language */
static bool page_language_on_enter(setup_page_t *page);
static bool page_language_on_next(setup_page_t *page);
static bool page_language_validate(setup_page_t *page);

/* Page handlers - Keyboard */
static bool page_keyboard_on_enter(setup_page_t *page);
static bool page_keyboard_on_next(setup_page_t *page);
static bool page_keyboard_validate(setup_page_t *page);

/* Page handlers - License */
static bool page_license_on_enter(setup_page_t *page);
static bool page_license_on_next(setup_page_t *page);

/* Page handlers - Install Type */
static bool page_install_type_on_enter(setup_page_t *page);
static bool page_install_type_on_next(setup_page_t *page);
static bool page_install_type_validate(setup_page_t *page);

/* Page handlers - Disk Select */
static bool page_disk_select_on_enter(setup_page_t *page);
static bool page_disk_select_on_next(setup_page_t *page);
static bool page_disk_select_validate(setup_page_t *page);

/* Page handlers - Partition */
static bool page_partition_on_enter(setup_page_t *page);
static bool page_partition_on_next(setup_page_t *page);
static bool page_partition_validate(setup_page_t *page);

/* Page handlers - Network */
static bool page_network_on_enter(setup_page_t *page);
static bool page_network_on_next(setup_page_t *page);

/* Page handlers - User */
static bool page_user_on_enter(setup_page_t *page);
static bool page_user_on_next(setup_page_t *page);
static bool page_user_validate(setup_page_t *page);

/* Page handlers - Timezone */
static bool page_timezone_on_enter(setup_page_t *page);
static bool page_timezone_on_next(setup_page_t *page);
static bool page_timezone_validate(setup_page_t *page);

/* Page handlers - Confirm */
static bool page_confirm_on_enter(setup_page_t *page);
static bool page_confirm_on_next(setup_page_t *page);

/* Page handlers - Install */
static bool page_install_on_enter(setup_page_t *page);

/* Page handlers - Complete */
static bool page_complete_on_enter(setup_page_t *page);

/* UI Creation */
static int create_main_window(setup_wizard_t *wizard);
static int create_sidebar(setup_wizard_t *wizard);
static int create_content_area(setup_wizard_t *wizard);
static int create_header(setup_wizard_t *wizard);
static int create_buttons(setup_wizard_t *wizard);
static int create_status_bar(setup_wizard_t *wizard);
static int create_progress_bar(setup_wizard_t *wizard);

/* UI Updates */
static void update_progress(setup_wizard_t *wizard);
static void update_buttons(setup_wizard_t *wizard);
static void update_status(setup_wizard_t *wizard, const char *msg);

/* Event handlers */
static void on_back_clicked(widget_t *button);
static void on_next_clicked(widget_t *button);
static void on_cancel_clicked(widget_t *button);
static void on_help_clicked(widget_t *button);

/*===========================================================================*/
/*                         WIZARD INITIALIZATION                             */
/*===========================================================================*/

/**
 * setup_wizard_init - Initialize the setup wizard
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_init(setup_wizard_t *wizard)
{
    if (!wizard) {
        return -EINVAL;
    }

    printk("[SETUP] ========================================\n");
    printk("[SETUP] NEXUS OS Setup Wizard v%s\n", SETUP_WIZARD_VERSION);
    printk("[SETUP] ========================================\n");
    printk("[SETUP] Initializing setup wizard...\n");

    /* Clear wizard structure */
    memset(wizard, 0, sizeof(setup_wizard_t));
    wizard->initialized = true;
    wizard->current_page = 0;
    wizard->total_pages = 0;
    wizard->installation_progress = 0;
    wizard->current_stage = INSTALL_STAGE_NONE;
    wizard->last_error = 0;
    wizard->selected_language = 0;
    wizard->selected_keyboard = 0;
    wizard->selected_timezone = 0;
    wizard->selected_disk = 0;
    wizard->selected_network = 0;
    
    /* Initialize default system configuration */
    strcpy(wizard->system.language, "en_US");
    strcpy(wizard->system.locale, "en_US.UTF-8");
    strcpy(wizard->system.keymap, "us");
    strcpy(wizard->system.timezone, "UTC");
    strcpy(wizard->system.hostname, "nexus-os");
    strcpy(wizard->system.ntp_server, "pool.ntp.org");
    wizard->system.enable_auto_updates = true;
    wizard->system.enable_ssh = false;
    
    /* Initialize default installation options */
    wizard->options.install_mode = INSTALL_MODE_NEW;
    wizard->options.format_target = true;
    wizard->options.install_grub = true;
    wizard->options.enable_trim = true;
    wizard->options.enable_swap = true;
    wizard->options.swap_size_mb = 4096; /* 4GB default swap */
    wizard->options.efi_size_mb = 512;
    wizard->options.root_size_mb = 30720; /* 30GB root */
    strcpy(wizard->options.grub_device, "/dev/sda");
    
    /* Initialize user account */
    wizard->user.is_admin = true;
    wizard->user.require_password = true;
    strcpy(wizard->user.shell, "/bin/bash");
    
    /* Initialize log buffer */
    memset(wizard->log_buffer, 0, SETUP_LOG_BUFFER_SIZE);
    wizard->log_offset = 0;
    wizard->log_lines = 0;
    
    /* Initialize language, timezone, and keyboard lists */
    setup_init_languages(wizard);
    setup_init_timezones(wizard);
    setup_init_keyboards(wizard);
    
    /* Detect hardware */
    printk("[SETUP] Detecting hardware...\n");
    setup_detect_disks(wizard);
    setup_detect_network(wizard);
    
    /* Create main window */
    printk("[SETUP] Creating UI...\n");
    if (create_main_window(wizard) != 0) {
        setup_log_error(wizard, -1, "Failed to create main window");
        return -1;
    }
    
    /* Create sidebar */
    if (create_sidebar(wizard) != 0) {
        setup_log_error(wizard, -1, "Failed to create sidebar");
        return -1;
    }
    
    /* Create content area */
    if (create_content_area(wizard) != 0) {
        setup_log_error(wizard, -1, "Failed to create content area");
        return -1;
    }
    
    /* Create header */
    if (create_header(wizard) != 0) {
        setup_log_error(wizard, -1, "Failed to create header");
        return -1;
    }
    
    /* Create buttons */
    if (create_buttons(wizard) != 0) {
        setup_log_error(wizard, -1, "Failed to create buttons");
        return -1;
    }
    
    /* Create progress bar */
    if (create_progress_bar(wizard) != 0) {
        setup_log_error(wizard, -1, "Failed to create progress bar");
        return -1;
    }
    
    /* Create status bar */
    if (create_status_bar(wizard) != 0) {
        setup_log_error(wizard, -1, "Failed to create status bar");
        return -1;
    }
    
    /* Register all wizard pages */
    setup_page_t pages[] = {
        /* Page 0: Welcome */
        {
            .page_id = 0,
            .title = "Welcome to NEXUS OS",
            .description = "Welcome to the NEXUS OS installation wizard. "
                          "This wizard will guide you through the installation process. "
                          "Please ensure your computer is plugged in and has sufficient battery.",
            .icon = "welcome",
            .on_enter = page_welcome_on_enter,
            .on_next = page_welcome_on_next,
            .is_valid = true,
        },
        
        /* Page 1: Language Selection */
        {
            .page_id = 1,
            .title = "Select Language",
            .description = "Choose your preferred language for the installation and system.",
            .icon = "language",
            .on_enter = page_language_on_enter,
            .on_next = page_language_on_next,
            .validate = page_language_validate,
        },
        
        /* Page 2: Keyboard Layout */
        {
            .page_id = 2,
            .title = "Keyboard Layout",
            .description = "Select your keyboard layout. You can test your selection below.",
            .icon = "keyboard",
            .on_enter = page_keyboard_on_enter,
            .on_next = page_keyboard_on_next,
            .validate = page_keyboard_validate,
        },
        
        /* Page 3: License Agreement */
        {
            .page_id = 3,
            .title = "License Agreement",
            .description = "Please read and accept the license agreement to continue.",
            .icon = "license",
            .on_enter = page_license_on_enter,
            .on_next = page_license_on_next,
        },
        
        /* Page 4: Installation Type */
        {
            .page_id = 4,
            .title = "Installation Type",
            .description = "Choose how you want to install NEXUS OS. You can erase the disk "
                          "or manually partition for advanced configurations.",
            .icon = "install-type",
            .on_enter = page_install_type_on_enter,
            .on_next = page_install_type_on_next,
            .validate = page_install_type_validate,
        },
        
        /* Page 5: Disk Selection */
        {
            .page_id = 5,
            .title = "Select Disk",
            .description = "Select the disk where you want to install NEXUS OS.",
            .icon = "disk",
            .on_enter = page_disk_select_on_enter,
            .on_next = page_disk_select_on_next,
            .validate = page_disk_select_validate,
        },
        
        /* Page 6: Partitioning */
        {
            .page_id = 6,
            .title = "Partition Disk",
            .description = "Create or modify partitions on the selected disk. You can create "
                          "separate partitions for root, home, and swap.",
            .icon = "partition",
            .on_enter = page_partition_on_enter,
            .on_next = page_partition_on_next,
            .validate = page_partition_validate,
        },
        
        /* Page 7: Network Configuration */
        {
            .page_id = 7,
            .title = "Network Configuration",
            .description = "Configure your network connection. This is optional but recommended "
                          "for downloading updates during installation.",
            .icon = "network",
            .on_enter = page_network_on_enter,
            .on_next = page_network_on_next,
        },
        
        /* Page 8: User Account */
        {
            .page_id = 8,
            .title = "Create User Account",
            .description = "Create your user account. This account will have administrator "
                          "privileges.",
            .icon = "user",
            .on_enter = page_user_on_enter,
            .on_next = page_user_on_next,
            .validate = page_user_validate,
        },
        
        /* Page 9: Timezone */
        {
            .page_id = 9,
            .title = "Select Timezone",
            .description = "Select your timezone to set the system clock correctly.",
            .icon = "timezone",
            .on_enter = page_timezone_on_enter,
            .on_next = page_timezone_on_next,
            .validate = page_timezone_validate,
        },
        
        /* Page 10: Confirmation */
        {
            .page_id = 10,
            .title = "Confirm Installation",
            .description = "Review your installation settings. Click Install to begin.",
            .icon = "confirm",
            .on_enter = page_confirm_on_enter,
            .on_next = page_confirm_on_next,
        },
        
        /* Page 11: Installation Progress */
        {
            .page_id = 11,
            .title = "Installing NEXUS OS",
            .description = "Please wait while NEXUS OS is being installed. This may take several minutes.",
            .icon = "install",
            .on_enter = page_install_on_enter,
        },
        
        /* Page 12: Installation Complete */
        {
            .page_id = 12,
            .title = "Installation Complete",
            .description = "NEXUS OS has been successfully installed. You can now restart your computer.",
            .icon = "complete",
            .on_enter = page_complete_on_enter,
        },
    };
    
    /* Add all pages to wizard */
    u32 num_pages = sizeof(pages) / sizeof(pages[0]);
    for (u32 i = 0; i < num_pages; i++) {
        setup_wizard_add_page(wizard, &pages[i]);
    }
    
    setup_log(wizard, "Setup wizard initialized (%d pages)", wizard->total_pages);
    setup_log(wizard, "Detected %d disk(s) and %d network interface(s)", 
              wizard->disk_count, wizard->network_count);
    
    printk("[SETUP] Setup wizard initialized successfully\n");
    printk("[SETUP] ========================================\n");
    
    return 0;
}

/**
 * setup_wizard_run - Start the setup wizard
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_run(setup_wizard_t *wizard)
{
    if (!wizard || !wizard->initialized) {
        return -EINVAL;
    }
    
    printk("[SETUP] Starting setup wizard...\n");
    
    wizard->running = true;
    wizard->start_time = get_time_ms();
    wizard->stage_start_time = get_time_ms();
    
    /* Show main window */
    if (wizard->main_window) {
        window_show(wizard->main_window);
    }
    
    /* Go to first page */
    setup_wizard_goto_page(wizard, 0);
    
    setup_log(wizard, "Setup wizard started");
    
    return 0;
}

/**
 * setup_wizard_shutdown - Shutdown the setup wizard
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_shutdown(setup_wizard_t *wizard)
{
    if (!wizard || !wizard->initialized) {
        return -EINVAL;
    }
    
    printk("[SETUP] Shutting down setup wizard...\n");
    
    wizard->running = false;
    wizard->completed = false;
    
    /* Hide main window */
    if (wizard->main_window) {
        window_hide(wizard->main_window);
    }
    
    /* Clear wizard structure but keep initialized flag */
    memset(wizard->log_buffer, 0, SETUP_LOG_BUFFER_SIZE);
    
    setup_log(wizard, "Setup wizard shutdown complete");
    
    return 0;
}

/**
 * setup_wizard_cancel - Cancel the setup wizard
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_cancel(setup_wizard_t *wizard)
{
    if (!wizard) {
        return -EINVAL;
    }
    
    setup_log(wizard, "Setup wizard cancelled by user");
    wizard->cancelled = true;
    wizard->running = false;
    
    return 0;
}

/**
 * setup_wizard_set_error - Set wizard error state
 * @wizard: Pointer to wizard structure
 * @error: Error code
 * @msg: Error message
 *
 * Returns: 0 on success
 */
int setup_wizard_set_error(setup_wizard_t *wizard, s32 error, const char *msg)
{
    if (!wizard || !msg) {
        return -EINVAL;
    }
    
    wizard->has_error = true;
    wizard->last_error = error;
    strncpy(wizard->error_message, msg, sizeof(wizard->error_message) - 1);
    wizard->error_message[sizeof(wizard->error_message) - 1] = '\0';
    
    setup_log_error(wizard, error, "%s", msg);
    
    return 0;
}

/*===========================================================================*/
/*                         PAGE NAVIGATION                                   */
/*===========================================================================*/

/**
 * setup_wizard_add_page - Add a page to the wizard
 * @wizard: Pointer to wizard structure
 * @page: Pointer to page structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_add_page(setup_wizard_t *wizard, setup_page_t *page)
{
    if (!wizard || !page) {
        return -EINVAL;
    }
    
    if (wizard->total_pages >= SETUP_MAX_PAGES) {
        setup_log_error(wizard, -1, "Maximum page count reached");
        return -1;
    }
    
    memcpy(&wizard->pages[wizard->total_pages], page, sizeof(setup_page_t));
    wizard->total_pages++;
    
    return 0;
}

/**
 * setup_wizard_goto_page - Navigate to a specific page
 * @wizard: Pointer to wizard structure
 * @page_id: Page ID to navigate to
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_goto_page(setup_wizard_t *wizard, u32 page_id)
{
    if (!wizard || page_id >= wizard->total_pages) {
        return -EINVAL;
    }
    
    setup_page_t *current = &wizard->pages[wizard->current_page];
    setup_page_t *next = &wizard->pages[page_id];
    
    setup_log(wizard, "Navigating to page %d: %s", page_id, next->title);
    
    /* Call on_leave for current page */
    if (current && current->on_leave) {
        if (!current->on_leave(current)) {
            setup_log_warning(wizard, "Page %d on_leave returned false", wizard->current_page);
        }
    }
    
    /* Update current page */
    wizard->current_page = page_id;
    wizard->stage_start_time = get_time_ms();
    
    /* Update UI */
    update_progress(wizard);
    update_buttons(wizard);
    update_status(wizard, next->description);
    
    /* Call on_enter for new page */
    if (next->on_enter) {
        if (!next->on_enter(next)) {
            setup_log_error(wizard, -1, "Page %d on_enter failed", page_id);
        }
    }
    
    /* Update header */
    if (wizard->header_label) {
        widget_set_text(wizard->header_label, next->title);
    }
    
    if (wizard->description_label) {
        widget_set_text(wizard->description_label, next->description);
    }
    
    setup_log(wizard, "Page %d/%d: %s", page_id + 1, wizard->total_pages, next->title);
    
    return 0;
}

/**
 * setup_wizard_next_page - Go to next page
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_next_page(setup_wizard_t *wizard)
{
    if (!wizard) {
        return -EINVAL;
    }
    
    setup_page_t *current = &wizard->pages[wizard->current_page];
    
    /* Validate current page */
    if (current->validate) {
        if (!current->validate(current)) {
            setup_log_warning(wizard, "Page %d validation failed", wizard->current_page);
            return -1;
        }
    }
    
    /* Call on_next handler */
    if (current->on_next) {
        if (!current->on_next(current)) {
            setup_log_error(wizard, -1, "Page %d on_next failed", wizard->current_page);
            return -1;
        }
    }
    
    /* Go to next page */
    if (wizard->current_page < wizard->total_pages - 1) {
        return setup_wizard_goto_page(wizard, wizard->current_page + 1);
    }
    
    return 0;
}

/**
 * setup_wizard_previous_page - Go to previous page
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int setup_wizard_previous_page(setup_wizard_t *wizard)
{
    if (!wizard) {
        return -EINVAL;
    }
    
    setup_page_t *current = &wizard->pages[wizard->current_page];
    
    /* Call on_back handler */
    if (current->on_back) {
        if (!current->on_back(current)) {
            setup_log_warning(wizard, "Page %d on_back returned false", wizard->current_page);
        }
    }
    
    /* Go to previous page */
    if (wizard->current_page > 0) {
        return setup_wizard_goto_page(wizard, wizard->current_page - 1);
    }
    
    return 0;
}

/**
 * setup_wizard_get_current_page - Get current page
 * @wizard: Pointer to wizard structure
 *
 * Returns: Pointer to current page, or NULL on error
 */
setup_page_t *setup_wizard_get_current_page(setup_wizard_t *wizard)
{
    if (!wizard) {
        return NULL;
    }
    return &wizard->pages[wizard->current_page];
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_main_window - Create the main wizard window
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_main_window(setup_wizard_t *wizard)
{
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "NEXUS OS Setup");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_MODAL | WINDOW_FLAG_RESIZABLE;
    props.bounds.x = (1920 - 900) / 2;  /* Center on 1920x1080 */
    props.bounds.y = (1080 - 650) / 2;
    props.bounds.width = 900;
    props.bounds.height = 650;
    props.background = color_from_rgba(30, 30, 40, 255);
    
    wizard->main_window = window_create(&props);
    if (!wizard->main_window) {
        return -1;
    }
    
    /* Create main widget container */
    wizard->main_widget = panel_create(NULL, 0, 0, 900, 650);
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
static int create_sidebar(setup_wizard_t *wizard)
{
    /* Create sidebar panel */
    wizard->sidebar = panel_create(wizard->main_widget, 0, 0, 220, 570);
    if (!wizard->sidebar) {
        return -1;
    }
    
    widget_set_colors(wizard->sidebar, 
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(35, 35, 50, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* Create page indicators */
    s32 y = 20;
    for (u32 i = 0; i < wizard->total_pages && i < 15; i++) {
        char label[64];
        snprintf(label, sizeof(label), "%d. %s", i + 1, wizard->pages[i].title);
        
        widget_t *indicator = label_create(wizard->sidebar, label, 15, y, 190, 25);
        if (indicator) {
            widget_set_font(indicator, 0, 11);
            widget_set_colors(indicator,
                            color_from_rgba(150, 150, 160, 255),
                            color_from_rgba(35, 35, 50, 0),
                            color_from_rgba(0, 0, 0, 0));
        }
        y += 30;
    }
    
    return 0;
}

/**
 * create_content_area - Create the main content area
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_content_area(setup_wizard_t *wizard)
{
    /* Create content panel */
    wizard->content_area = panel_create(wizard->main_widget, 220, 0, 680, 520);
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
 * create_header - Create the header with title and description
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_header(setup_wizard_t *wizard)
{
    /* Create header label */
    wizard->header_label = label_create(wizard->content_area, "Welcome", 20, 10, 640, 35);
    if (!wizard->header_label) {
        return -1;
    }
    
    widget_set_font(wizard->header_label, 0, 24);
    widget_set_colors(wizard->header_label,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(45, 45, 60, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create description label */
    wizard->description_label = label_create(wizard->content_area, 
                                              "Welcome to NEXUS OS Setup",
                                              20, 50, 640, 60);
    if (!wizard->description_label) {
        return -1;
    }
    
    widget_set_font(wizard->description_label, 0, 12);
    widget_set_colors(wizard->description_label,
                      color_from_rgba(180, 180, 190, 255),
                      color_from_rgba(45, 45, 60, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_buttons - Create navigation buttons
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_buttons(setup_wizard_t *wizard)
{
    /* Create back button */
    wizard->back_button = button_create(wizard->main_widget, "Back", 230, 580, 100, 40);
    if (!wizard->back_button) {
        return -1;
    }
    
    wizard->back_button->on_click = on_back_clicked;
    wizard->back_button->user_data = wizard;
    widget_set_colors(wizard->back_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(60, 60, 75, 255),
                      color_from_rgba(80, 80, 95, 255));
    
    /* Create next button */
    wizard->next_button = button_create(wizard->main_widget, "Next", 560, 580, 100, 40);
    if (!wizard->next_button) {
        return -1;
    }
    
    wizard->next_button->on_click = on_next_clicked;
    wizard->next_button->user_data = wizard;
    widget_set_colors(wizard->next_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(0, 120, 215, 255),
                      color_from_rgba(0, 100, 180, 255));
    
    /* Create cancel button */
    wizard->cancel_button = button_create(wizard->main_widget, "Cancel", 670, 580, 100, 40);
    if (!wizard->cancel_button) {
        return -1;
    }
    
    wizard->cancel_button->on_click = on_cancel_clicked;
    wizard->cancel_button->user_data = wizard;
    widget_set_colors(wizard->cancel_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(60, 60, 75, 255),
                      color_from_rgba(80, 80, 95, 255));
    
    /* Create help button */
    wizard->help_button = button_create(wizard->main_widget, "Help", 780, 580, 80, 40);
    if (!wizard->help_button) {
        return -1;
    }
    
    wizard->help_button->on_click = on_help_clicked;
    wizard->help_button->user_data = wizard;
    widget_set_colors(wizard->help_button,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(60, 60, 75, 255),
                      color_from_rgba(80, 80, 95, 255));
    
    return 0;
}

/**
 * create_progress_bar - Create the progress bar
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_progress_bar(setup_wizard_t *wizard)
{
    /* Create progress bar */
    wizard->progress_bar = progressbar_create(wizard->main_widget, 230, 555, 630, 15);
    if (!wizard->progress_bar) {
        return -1;
    }
    
    /* Create progress label */
    wizard->progress_label = label_create(wizard->main_widget, "0%", 230, 535, 630, 15);
    if (!wizard->progress_label) {
        return -1;
    }
    
    widget_set_font(wizard->progress_label, 0, 10);
    widget_set_colors(wizard->progress_label,
                      color_from_rgba(180, 180, 190, 255),
                      color_from_rgba(45, 45, 60, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_status_bar - Create the status bar
 * @wizard: Pointer to wizard structure
 *
 * Returns: 0 on success, negative error code on failure
 */
static int create_status_bar(setup_wizard_t *wizard)
{
    /* Create status bar panel */
    wizard->status_bar = panel_create(wizard->main_widget, 0, 620, 900, 30);
    if (!wizard->status_bar) {
        return -1;
    }
    
    widget_set_colors(wizard->status_bar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(25, 25, 35, 255),
                      color_from_rgba(50, 50, 65, 255));
    
    /* Create status label */
    widget_t *status_label = label_create(wizard->status_bar, "Ready", 10, 5, 600, 20);
    if (!status_label) {
        return -1;
    }
    
    widget_set_font(status_label, 0, 10);
    widget_set_colors(status_label,
                      color_from_rgba(150, 150, 160, 255),
                      color_from_rgba(25, 25, 35, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    /* Create version label */
    widget_t *version_label = label_create(wizard->status_bar, 
                                            "NEXUS OS v" NEXUS_VERSION_STRING,
                                            700, 5, 190, 20);
    if (!version_label) {
        return -1;
    }
    
    widget_set_font(version_label, 0, 10);
    widget_set_colors(version_label,
                      color_from_rgba(100, 100, 110, 255),
                      color_from_rgba(25, 25, 35, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/*===========================================================================*/
/*                         UI UPDATES                                        */
/*===========================================================================*/

/**
 * update_progress - Update the progress bar
 * @wizard: Pointer to wizard structure
 */
static void update_progress(setup_wizard_t *wizard)
{
    if (!wizard) return;
    
    u32 progress = (wizard->current_page * 100) / wizard->total_pages;
    wizard->installation_progress = progress;
    
    /* Update progress label */
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
static void update_buttons(setup_wizard_t *wizard)
{
    if (!wizard) return;
    
    /* Back button: disabled on first page */
    if (wizard->back_button) {
        if (wizard->current_page == 0) {
            widget_disable(wizard->back_button);
        } else {
            widget_enable(wizard->back_button);
        }
    }
    
    /* Next button: hidden on installation page */
    if (wizard->next_button) {
        if (wizard->current_page == 11) {
            widget_hide(wizard->next_button);
        } else {
            widget_show(wizard->next_button);
        }
    }
    
    /* Cancel button: hidden during installation */
    if (wizard->cancel_button) {
        if (wizard->current_page == 11 || wizard->current_page == 12) {
            widget_hide(wizard->cancel_button);
        } else {
            widget_show(wizard->cancel_button);
        }
    }
}

/**
 * update_status - Update status bar message
 * @wizard: Pointer to wizard structure
 * @msg: Status message
 */
static void update_status(setup_wizard_t *wizard, const char *msg)
{
    if (!wizard || !msg) return;
    
    strncpy(wizard->status_message, msg, sizeof(wizard->status_message) - 1);
    wizard->status_message[sizeof(wizard->status_message) - 1] = '\0';
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                   */
/*===========================================================================*/

/**
 * on_back_clicked - Handle back button click
 * @button: Button widget
 */
static void on_back_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    setup_wizard_t *wizard = (setup_wizard_t *)button->user_data;
    setup_wizard_previous_page(wizard);
}

/**
 * on_next_clicked - Handle next button click
 * @button: Button widget
 */
static void on_next_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    setup_wizard_t *wizard = (setup_wizard_t *)button->user_data;
    setup_wizard_next_page(wizard);
}

/**
 * on_cancel_clicked - Handle cancel button click
 * @button: Button widget
 */
static void on_cancel_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    setup_wizard_t *wizard = (setup_wizard_t *)button->user_data;
    
    /* Show confirmation dialog (would be implemented in full UI) */
    setup_wizard_cancel(wizard);
}

/**
 * on_help_clicked - Handle help button click
 * @button: Button widget
 */
static void on_help_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    setup_wizard_t *wizard = (setup_wizard_t *)button->user_data;
    setup_page_t *page = setup_wizard_get_current_page(wizard);
    
    if (page) {
        setup_log(wizard, "Help requested for: %s", page->title);
        /* Show help dialog (would be implemented in full UI) */
    }
}
