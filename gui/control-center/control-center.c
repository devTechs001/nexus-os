/*
 * NEXUS OS - Control Center
 * gui/control-center/control-center.c
 *
 * Quick settings, system controls, user profile
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         CONTROL CENTER CONFIGURATION                      */
/*===========================================================================*/

#define CC_MAX_QUICK_SETTINGS     16
#define CC_MAX_SLIDERS            8
#define CC_WIDTH                   350
#define CC_HEIGHT                  500

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 id;
    char name[64];
    char icon[64];
    bool enabled;
    void (*on_toggle)(bool);
} quick_setting_t;

typedef struct {
    u32 id;
    char name[64];
    char icon[64];
    u32 value;
    u32 min_value;
    u32 max_value;
    void (*on_change)(u32);
} control_slider_t;

typedef struct {
    bool is_open;
    rect_t geometry;
    quick_setting_t quick_settings[CC_MAX_QUICK_SETTINGS];
    u32 quick_setting_count;
    control_slider_t sliders[CC_MAX_SLIDERS];
    u32 slider_count;
    char user_name[64];
    char user_avatar[256];
    void (*on_settings_change)(const char *key, u32 value);
} control_center_t;

static control_center_t g_cc;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int control_center_init(void)
{
    printk("[CONTROL_CENTER] Initializing control center...\n");
    
    memset(&g_cc, 0, sizeof(control_center_t));
    g_cc.geometry.width = CC_WIDTH;
    g_cc.geometry.height = CC_HEIGHT;
    strcpy(g_cc.user_name, "User");
    
    /* Quick settings */
    quick_setting_t *qs;
    
    qs = &g_cc.quick_settings[g_cc.quick_setting_count++];
    qs->id = 1;
    strcpy(qs->name, "WiFi");
    strcpy(qs->icon, "wifi");
    qs->enabled = true;
    
    qs = &g_cc.quick_settings[g_cc.quick_setting_count++];
    qs->id = 2;
    strcpy(qs->name, "Bluetooth");
    strcpy(qs->icon, "bluetooth");
    qs->enabled = false;
    
    qs = &g_cc.quick_settings[g_cc.quick_setting_count++];
    qs->id = 3;
    strcpy(qs->name, "Airplane Mode");
    strcpy(qs->icon, "airplane");
    qs->enabled = false;
    
    qs = &g_cc.quick_settings[g_cc.quick_setting_count++];
    qs->id = 4;
    strcpy(qs->name, "Do Not Disturb");
    strcpy(qs->icon, "dnd");
    qs->enabled = false;
    
    qs = &g_cc.quick_settings[g_cc.quick_setting_count++];
    qs->id = 5;
    strcpy(qs->name, "Night Light");
    strcpy(qs->icon, "night");
    qs->enabled = false;
    
    qs = &g_cc.quick_settings[g_cc.quick_setting_count++];
    qs->id = 6;
    strcpy(qs->name, "Location");
    strcpy(qs->icon, "location");
    qs->enabled = true;
    
    /* Sliders */
    control_slider_t *slider;
    
    slider = &g_cc.sliders[g_cc.slider_count++];
    slider->id = 1;
    strcpy(slider->name, "Brightness");
    strcpy(slider->icon, "brightness");
    slider->value = 80;
    slider->min_value = 0;
    slider->max_value = 100;
    
    slider = &g_cc.sliders[g_cc.slider_count++];
    slider->id = 2;
    strcpy(slider->name, "Volume");
    strcpy(slider->icon, "volume");
    slider->value = 60;
    slider->min_value = 0;
    slider->max_value = 100;
    
    printk("[CONTROL_CENTER] Control center initialized\n");
    return 0;
}

int control_center_shutdown(void)
{
    printk("[CONTROL_CENTER] Shutting down control center...\n");
    g_cc.is_open = false;
    return 0;
}

/*===========================================================================*/
/*                         CONTROL CENTER OPERATIONS                         */
/*===========================================================================*/

int control_center_open(void)
{
    g_cc.is_open = true;
    printk("[CONTROL_CENTER] Opened\n");
    return 0;
}

int control_center_close(void)
{
    g_cc.is_open = false;
    printk("[CONTROL_CENTER] Closed\n");
    return 0;
}

int control_center_toggle(void)
{
    if (g_cc.is_open) {
        return control_center_close();
    } else {
        return control_center_open();
    }
}

/*===========================================================================*/
/*                         QUICK SETTINGS                                    */
/*===========================================================================*/

int control_center_toggle_setting(u32 id)
{
    for (u32 i = 0; i < g_cc.quick_setting_count; i++) {
        if (g_cc.quick_settings[i].id == id) {
            quick_setting_t *qs = &g_cc.quick_settings[i];
            qs->enabled = !qs->enabled;
            
            printk("[CONTROL_CENTER] Toggled %s: %s\n", 
                   qs->name, qs->enabled ? "ON" : "OFF");
            
            if (qs->on_toggle) {
                qs->on_toggle(qs->enabled);
            }
            
            if (g_cc.on_settings_change) {
                g_cc.on_settings_change(qs->name, qs->enabled ? 1 : 0);
            }
            
            return 0;
        }
    }
    
    return -ENOENT;
}

int control_center_set_setting(u32 id, bool enabled)
{
    for (u32 i = 0; i < g_cc.quick_setting_count; i++) {
        if (g_cc.quick_settings[i].id == id) {
            g_cc.quick_settings[i].enabled = enabled;
            return 0;
        }
    }
    return -ENOENT;
}

/*===========================================================================*/
/*                         SLIDERS                                           */
/*===========================================================================*/

int control_center_set_slider(u32 id, u32 value)
{
    for (u32 i = 0; i < g_cc.slider_count; i++) {
        if (g_cc.sliders[i].id == id) {
            control_slider_t *slider = &g_cc.sliders[i];
            
            if (value < slider->min_value) value = slider->min_value;
            if (value > slider->max_value) value = slider->max_value;
            
            slider->value = value;
            
            printk("[CONTROL_CENTER] %s: %d%%\n", slider->name, value);
            
            if (slider->on_change) {
                slider->on_change(value);
            }
            
            return 0;
        }
    }
    return -ENOENT;
}

int control_center_get_slider(u32 id, u32 *value)
{
    if (!value) return -EINVAL;
    
    for (u32 i = 0; i < g_cc.slider_count; i++) {
        if (g_cc.sliders[i].id == id) {
            *value = g_cc.sliders[i].value;
            return 0;
        }
    }
    return -ENOENT;
}

/*===========================================================================*/
/*                         USER PROFILE                                      */
/*===========================================================================*/

int control_center_set_user(const char *name, const char *avatar_path)
{
    if (name) {
        strncpy(g_cc.user_name, name, sizeof(g_cc.user_name) - 1);
    }
    if (avatar_path) {
        strncpy(g_cc.user_avatar, avatar_path, sizeof(g_cc.user_avatar) - 1);
    }
    return 0;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

int control_center_render(void *surface)
{
    if (!g_cc.is_open || !surface) return -EINVAL;
    
    /* Render control center panel */
    /* Render user profile */
    /* Render quick settings grid */
    /* Render sliders */
    /* Render settings button */
    
    (void)surface;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

control_center_t *control_center_get(void)
{
    return &g_cc;
}
