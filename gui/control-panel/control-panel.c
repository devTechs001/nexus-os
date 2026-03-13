/*
 * NEXUS OS - Control Panel
 * gui/control-panel/control-panel.c
 *
 * System control panel with all settings categories
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         CONTROL PANEL CONFIGURATION                       */
/*===========================================================================*/

#define CP_MAX_CATEGORIES         24
#define CP_MAX_ITEMS              256

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 item_id;
    char name[128];
    char description[256];
    char icon[64];
    u32 type;               /* Toggle, Slider, Select, Button, Link */
    char value[256];
    char options[10][64];
    u32 option_count;
    void (*on_change)(u32 item_id, const char *value);
} control_item_t;

typedef struct {
    u32 category_id;
    char name[64];
    char description[128];
    char icon[64];
    control_item_t items[CP_MAX_ITEMS];
    u32 item_count;
    bool is_visible;
} control_category_t;

typedef struct {
    bool is_open;
    rect_t window_rect;
    control_category_t categories[CP_MAX_CATEGORIES];
    u32 category_count;
    u32 selected_category;
    u32 selected_item;
    char search_text[128];
    char view_mode;         /* 0=category, 1=all, 2=favorites */
    u32 favorites[CP_MAX_CATEGORIES];
    u32 favorite_count;
    u64 last_opened;
    u32 open_count;
    void (*on_item_change)(u32 category, u32 item, const char *value);
} control_panel_t;

static control_panel_t g_cp;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int control_panel_init(void)
{
    printk("[CP] Initializing control panel...\n");
    
    memset(&g_cp, 0, sizeof(control_panel_t));
    g_cp.window_rect.width = 1000;
    g_cp.window_rect.height = 700;
    
    control_category_t *cat;
    control_item_t *item;
    
    /* System Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 1;
    strcpy(cat->name, "System");
    strcpy(cat->description, "System information and updates");
    strcpy(cat->icon, "system");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 1;
    strcpy(item->name, "System Info");
    strcpy(item->description, "View system specifications");
    strcpy(item->icon, "info");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 2;
    strcpy(item->name, "Check for Updates");
    strcpy(item->description, "Download and install updates");
    strcpy(item->icon, "update");
    item->type = 4;  /* Button */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 3;
    strcpy(item->name, "Date & Time");
    strcpy(item->description, "Set system date and time");
    strcpy(item->icon, "time");
    item->type = 4;  /* Link */
    
    /* Display Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 2;
    strcpy(cat->name, "Display");
    strcpy(cat->description, "Display and graphics settings");
    strcpy(cat->icon, "display");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 4;
    strcpy(item->name, "Resolution");
    strcpy(item->description, "Screen resolution");
    strcpy(item->icon, "resolution");
    item->type = 3;  /* Select */
    strcpy(item->options[0], "1920x1080");
    strcpy(item->options[1], "2560x1440");
    strcpy(item->options[2], "3840x2160");
    item->option_count = 3;
    strcpy(item->value, "1920x1080");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 5;
    strcpy(item->name, "Brightness");
    strcpy(item->description, "Screen brightness");
    strcpy(item->icon, "brightness");
    item->type = 2;  /* Slider */
    strcpy(item->value, "80");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 6;
    strcpy(item->name, "Night Light");
    strcpy(item->description, "Reduce blue light");
    strcpy(item->icon, "night");
    item->type = 1;  /* Toggle */
    strcpy(item->value, "off");
    
    /* Sound Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 3;
    strcpy(cat->name, "Sound");
    strcpy(cat->description, "Audio and volume settings");
    strcpy(cat->icon, "sound");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 7;
    strcpy(item->name, "Master Volume");
    strcpy(item->description, "System volume");
    strcpy(item->icon, "volume");
    item->type = 2;  /* Slider */
    strcpy(item->value, "60");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 8;
    strcpy(item->name, "Output Device");
    strcpy(item->description, "Audio output device");
    strcpy(item->icon, "output");
    item->type = 3;  /* Select */
    strcpy(item->options[0], "Speakers");
    strcpy(item->options[1], "Headphones");
    strcpy(item->options[2], "Bluetooth");
    item->option_count = 3;
    strcpy(item->value, "Speakers");
    
    /* Network Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 4;
    strcpy(cat->name, "Network");
    strcpy(cat->description, "Network and internet settings");
    strcpy(cat->icon, "network");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 9;
    strcpy(item->name, "WiFi");
    strcpy(item->description, "Wireless network");
    strcpy(item->icon, "wifi");
    item->type = 1;  /* Toggle */
    strcpy(item->value, "on");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 10;
    strcpy(item->name, "Bluetooth");
    strcpy(item->description, "Bluetooth connectivity");
    strcpy(item->icon, "bluetooth");
    item->type = 1;  /* Toggle */
    strcpy(item->value, "off");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 11;
    strcpy(item->name, "Proxy");
    strcpy(item->description, "Proxy server settings");
    strcpy(item->icon, "proxy");
    item->type = 4;  /* Link */
    
    /* Security Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 5;
    strcpy(cat->name, "Security");
    strcpy(cat->description, "Security and privacy settings");
    strcpy(cat->icon, "security");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 12;
    strcpy(item->name, "Firewall");
    strcpy(item->description, "Network firewall");
    strcpy(item->icon, "firewall");
    item->type = 1;  /* Toggle */
    strcpy(item->value, "on");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 13;
    strcpy(item->name, "Windows Security");
    strcpy(item->description, "Antivirus and protection");
    strcpy(item->icon, "shield");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 14;
    strcpy(item->name, "Privacy");
    strcpy(item->description, "Privacy settings");
    strcpy(item->icon, "privacy");
    item->type = 4;  /* Link */
    
    /* Accounts Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 6;
    strcpy(cat->name, "Accounts");
    strcpy(cat->description, "User accounts and sign-in");
    strcpy(cat->icon, "accounts");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 15;
    strcpy(item->name, "Your Account");
    strcpy(item->description, "Manage your account");
    strcpy(item->icon, "user");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 16;
    strcpy(item->name, "Sign-in Options");
    strcpy(item->description, "Password, PIN, biometric");
    strcpy(item->icon, "login");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 17;
    strcpy(item->name, "Family & Other Users");
    strcpy(item->description, "Manage other accounts");
    strcpy(item->icon, "family");
    item->type = 4;  /* Link */
    
    /* Apps Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 7;
    strcpy(cat->name, "Apps");
    strcpy(cat->description, "Installed applications");
    strcpy(cat->icon, "apps");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 18;
    strcpy(item->name, "Installed Apps");
    strcpy(item->description, "View and manage apps");
    strcpy(item->icon, "list");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 19;
    strcpy(item->name, "Default Apps");
    strcpy(item->description, "Set default applications");
    strcpy(item->icon, "default");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 20;
    strcpy(item->name, "Startup Apps");
    strcpy(item->description, "Manage startup applications");
    strcpy(item->icon, "startup");
    item->type = 4;  /* Link */
    
    /* Devices Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 8;
    strcpy(cat->name, "Devices");
    strcpy(cat->description, "Connected devices");
    strcpy(cat->icon, "devices");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 21;
    strcpy(item->name, "Bluetooth Devices");
    strcpy(item->description, "Paired Bluetooth devices");
    strcpy(item->icon, "bluetooth");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 22;
    strcpy(item->name, "Printers & Scanners");
    strcpy(item->description, "Connected printers");
    strcpy(item->icon, "printer");
    item->type = 4;  /* Link */
    
    item = &cat->items[cat->item_count++];
    item->item_id = 23;
    strcpy(item->name, "USB Devices");
    strcpy(item->description, "Connected USB devices");
    strcpy(item->icon, "usb");
    item->type = 4;  /* Link */
    
    /* Storage Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 9;
    strcpy(cat->name, "Storage");
    strcpy(cat->description, "Storage and disk management");
    strcpy(cat->icon, "storage");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 24;
    strcpy(item->name, "Storage Sense");
    strcpy(item->description, "Automatic cleanup");
    strcpy(item->icon, "sense");
    item->type = 1;  /* Toggle */
    strcpy(item->value, "on");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 25;
    strcpy(item->name, "Disk Management");
    strcpy(item->description, "Manage disks and volumes");
    strcpy(item->icon, "disk");
    item->type = 4;  /* Link */
    
    /* Gaming Category */
    cat = &g_cp.categories[g_cp.category_count++];
    cat->category_id = 10;
    strcpy(cat->name, "Gaming");
    strcpy(cat->description, "Gaming settings and optimizations");
    strcpy(cat->icon, "games");
    cat->is_visible = true;
    
    item = &cat->items[cat->item_count++];
    item->item_id = 26;
    strcpy(item->name, "Game Mode");
    strcpy(item->description, "Optimize for gaming");
    strcpy(item->icon, "mode");
    item->type = 1;  /* Toggle */
    strcpy(item->value, "on");
    
    item = &cat->items[cat->item_count++];
    item->item_id = 27;
    strcpy(item->name, "GPU Settings");
    strcpy(item->description, "Graphics processor settings");
    strcpy(item->icon, "gpu");
    item->type = 4;  /* Link */
    
    printk("[CP] Control panel initialized (%d categories)\n", 
           g_cp.category_count);
    
    return 0;
}

/*===========================================================================*/
/*                         CONTROL PANEL OPERATIONS                          */
/*===========================================================================*/

int control_panel_open(void)
{
    g_cp.is_open = true;
    g_cp.last_opened = ktime_get_sec();
    g_cp.open_count++;
    
    printk("[CP] Opened\n");
    return 0;
}

int control_panel_close(void)
{
    g_cp.is_open = false;
    printk("[CP] Closed\n");
    return 0;
}

int control_panel_select_category(u32 category_id)
{
    if (category_id >= g_cp.category_count) return -EINVAL;
    
    g_cp.selected_category = category_id;
    g_cp.selected_item = 0;
    
    printk("[CP] Selected category: %s\n", 
           g_cp.categories[category_id].name);
    
    return 0;
}

int control_panel_change_item(u32 item_id, const char *value)
{
    for (u32 c = 0; c < g_cp.category_count; c++) {
        control_category_t *cat = &g_cp.categories[c];
        for (u32 i = 0; i < cat->item_count; i++) {
            if (cat->items[i].item_id == item_id) {
                strncpy(cat->items[i].value, value, sizeof(cat->items[i].value) - 1);
                
                printk("[CP] Changed %s: %s\n", cat->items[i].name, value);
                
                if (cat->items[i].on_change) {
                    cat->items[i].on_change(item_id, value);
                }
                
                if (g_cp.on_item_change) {
                    g_cp.on_item_change(c, i, value);
                }
                
                return 0;
            }
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         SEARCH                                            */
/*===========================================================================*/

int control_panel_search(const char *query)
{
    if (!query) return -EINVAL;
    
    strncpy(g_cp.search_text, query, sizeof(g_cp.search_text) - 1);
    
    printk("[CP] Search: %s\n", query);
    
    /* Would filter categories and items */
    return 0;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

int control_panel_render(void *surface)
{
    if (!g_cp.is_open || !surface) return -EINVAL;
    
    /* Render window frame */
    /* Render sidebar with categories */
    /* Render main panel with items */
    /* Render search bar */
    
    (void)surface;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

control_panel_t *control_panel_get(void)
{
    return &g_cp;
}

int control_panel_get_category_count(void)
{
    return g_cp.category_count;
}

control_category_t *control_panel_get_category(u32 index)
{
    if (index >= g_cp.category_count) return NULL;
    return &g_cp.categories[index];
}
