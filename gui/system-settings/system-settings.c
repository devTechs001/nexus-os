/*
 * NEXUS OS - System Settings
 * gui/system-settings/system-settings.c
 *
 * System configuration panels
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         SETTINGS CONFIGURATION                            */
/*===========================================================================*/

#define SETTINGS_MAX_CATEGORIES   16
#define SETTINGS_MAX_ITEMS        256

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 id;
    char name[64];
    char icon[64];
    u32 type;  /* 0=toggle, 1=select, 2=slider, 3=text, 4=button */
    u32 value;
    char text_value[256];
    char options[10][64];
    u32 option_count;
    void (*on_change)(u32 id, u32 value, const char *text);
} setting_item_t;

typedef struct {
    char name[64];
    char icon[64];
    setting_item_t items[SETTINGS_MAX_ITEMS];
    u32 item_count;
} setting_category_t;

typedef struct {
    bool is_open;
    rect_t window_rect;
    setting_category_t categories[SETTINGS_MAX_CATEGORIES];
    u32 category_count;
    u32 selected_category;
    u32 selected_item;
    char search_text[128];
    void (*on_setting_change)(u32 category, u32 item, u32 value);
} system_settings_t;

static system_settings_t g_settings;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int system_settings_init(void)
{
    printk("[SETTINGS] Initializing system settings...\n");
    
    memset(&g_settings, 0, sizeof(system_settings_t));
    g_settings.window_rect.width = 900;
    g_settings.window_rect.height = 650;
    
    setting_category_t *cat;
    setting_item_t *item;
    
    /* Display Settings */
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Display");
    strcpy(cat->icon, "display");
    
    item = &cat->items[cat->item_count++];
    item->id = 1;
    strcpy(item->name, "Brightness");
    strcpy(item->icon, "brightness");
    item->type = 2;  /* Slider */
    item->value = 80;
    
    item = &cat->items[cat->item_count++];
    item->id = 2;
    strcpy(item->name, "Night Light");
    strcpy(item->icon, "night");
    item->type = 0;  /* Toggle */
    item->value = 0;
    
    item = &cat->items[cat->item_count++];
    item->id = 3;
    strcpy(item->name, "Resolution");
    strcpy(item->icon, "resolution");
    item->type = 1;  /* Select */
    strcpy(item->options[0], "1920x1080");
    strcpy(item->options[1], "2560x1440");
    strcpy(item->options[2], "3840x2160");
    item->option_count = 3;
    item->value = 0;
    
    /* Sound Settings */
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Sound");
    strcpy(cat->icon, "sound");
    
    item = &cat->items[cat->item_count++];
    item->id = 1;
    strcpy(item->name, "Master Volume");
    strcpy(item->icon, "volume");
    item->type = 2;
    item->value = 60;
    
    item = &cat->items[cat->item_count++];
    item->id = 2;
    strcpy(item->name, "Output Device");
    strcpy(item->icon, "output");
    item->type = 1;
    strcpy(item->options[0], "Built-in Speakers");
    strcpy(item->options[1], "Headphones");
    strcpy(item->options[2], "Bluetooth");
    item->option_count = 3;
    
    /* Network Settings */
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Network");
    strcpy(cat->icon, "network");
    
    item = &cat->items[cat->item_count++];
    item->id = 1;
    strcpy(item->name, "WiFi");
    strcpy(item->icon, "wifi");
    item->type = 0;
    item->value = 1;
    
    item = &cat->items[cat->item_count++];
    item->id = 2;
    strcpy(item->name, "Bluetooth");
    strcpy(item->icon, "bluetooth");
    item->type = 0;
    item->value = 0;
    
    /* System Settings */
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "System");
    strcpy(cat->icon, "system");
    
    item = &cat->items[cat->item_count++];
    item->id = 1;
    strcpy(item->name, "Language");
    strcpy(item->icon, "language");
    item->type = 1;
    strcpy(item->options[0], "English");
    strcpy(item->options[1], "Spanish");
    strcpy(item->options[2], "French");
    strcpy(item->options[3], "German");
    item->option_count = 4;
    
    item = &cat->items[cat->item_count++];
    item->id = 2;
    strcpy(item->name, "Date & Time");
    strcpy(item->icon, "time");
    item->type = 4;  /* Button */
    
    item = &cat->items[cat->item_count++];
    item->id = 3;
    strcpy(item->name, "About");
    strcpy(item->icon, "info");
    item->type = 4;
    
    /* Security Settings */
    cat = &g_settings.categories[g_settings.category_count++];
    strcpy(cat->name, "Security");
    strcpy(cat->icon, "security");
    
    item = &cat->items[cat->item_count++];
    item->id = 1;
    strcpy(item->name, "Firewall");
    strcpy(item->icon, "firewall");
    item->type = 0;
    item->value = 1;
    
    item = &cat->items[cat->item_count++];
    item->id = 2;
    strcpy(item->name, "Encryption");
    strcpy(item->icon, "encryption");
    item->type = 4;
    
    printk("[SETTINGS] System settings initialized (%d categories)\n",
           g_settings.category_count);
    
    return 0;
}

int system_settings_shutdown(void)
{
    printk("[SETTINGS] Shutting down system settings...\n");
    g_settings.is_open = false;
    return 0;
}

/*===========================================================================*/
/*                         SETTINGS OPERATIONS                               */
/*===========================================================================*/

int system_settings_open(void)
{
    g_settings.is_open = true;
    g_settings.selected_category = 0;
    g_settings.selected_item = 0;
    printk("[SETTINGS] Opened\n");
    return 0;
}

int system_settings_close(void)
{
    g_settings.is_open = false;
    printk("[SETTINGS] Closed\n");
    return 0;
}

/*===========================================================================*/
/*                         NAVIGATION                                        */
/*===========================================================================*/

int system_settings_select_category(u32 index)
{
    if (index >= g_settings.category_count) {
        return -EINVAL;
    }
    
    g_settings.selected_category = index;
    g_settings.selected_item = 0;
    
    printk("[SETTINGS] Selected category: %s\n",
           g_settings.categories[index].name);
    
    return 0;
}

int system_settings_next_category(void)
{
    g_settings.selected_category++;
    if (g_settings.selected_category >= g_settings.category_count) {
        g_settings.selected_category = 0;
    }
    g_settings.selected_item = 0;
    return 0;
}

int system_settings_prev_category(void)
{
    if (g_settings.selected_category == 0) {
        g_settings.selected_category = g_settings.category_count - 1;
    } else {
        g_settings.selected_category--;
    }
    g_settings.selected_item = 0;
    return 0;
}

/*===========================================================================*/
/*                         SETTING MODIFICATION                              */
/*===========================================================================*/

int system_settings_set_value(u32 category, u32 item, u32 value)
{
    if (category >= g_settings.category_count) return -EINVAL;
    if (item >= g_settings.categories[category].item_count) return -EINVAL;
    
    setting_item_t *si = &g_settings.categories[category].items[item];
    si->value = value;
    
    printk("[SETTINGS] %s/%s = %d\n",
           g_settings.categories[category].name, si->name, value);
    
    if (si->on_change) {
        si->on_change(si->id, value, si->text_value);
    }
    
    if (g_settings.on_setting_change) {
        g_settings.on_setting_change(category, item, value);
    }
    
    return 0;
}

int system_settings_toggle(u32 category, u32 item)
{
    if (category >= g_settings.category_count) return -EINVAL;
    if (item >= g_settings.categories[category].item_count) return -EINVAL;
    
    setting_item_t *si = &g_settings.categories[category].items[item];
    if (si->type != 0) return -EINVAL;  /* Not a toggle */
    
    si->value = !si->value;
    
    printk("[SETTINGS] %s/%s = %s\n",
           g_settings.categories[category].name, si->name,
           si->value ? "ON" : "OFF");
    
    return system_settings_set_value(category, item, si->value);
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

int system_settings_render(void *surface)
{
    if (!g_settings.is_open || !surface) return -EINVAL;
    
    /* Render window */
    /* Render category sidebar */
    /* Render settings panel */
    /* Render search bar */
    
    (void)surface;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

system_settings_t *system_settings_get(void)
{
    return &g_settings;
}
