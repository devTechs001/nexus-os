/*
 * NEXUS OS - Enhanced Display Settings
 * gui/display/display_settings.c
 *
 * Advanced display configuration with backgrounds, effects, glassmorphism
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         DISPLAY CONFIGURATION                             */
/*===========================================================================*/

#define MAX_BACKGROUNDS         64
#define MAX_EFFECTS             32
#define MAX_PANELS              16

/* Background Types */
#define BG_TYPE_SOLID           0
#define BG_TYPE_GRADIENT_H      1
#define BG_TYPE_GRADIENT_V      2
#define BG_TYPE_GRADIENT_RADIAL 3
#define BG_TYPE_IMAGE           4
#define BG_TYPE_VIDEO           5
#define BG_TYPE_ANIMATED        6
#define BG_TYPE_GLASSMORPHIC    7

/* Effect Types */
#define EFFECT_NONE             0
#define EFFECT_BLUR             1
#define EFFECT_GLASSMORPHIC     2
#define EFFECT_TRANSPARENCY     3
#define EFFECT_SHADOW           4
#define EFFECT_GLOW             5
#define EFFECT_ANIMATION        6
#define EFFECT_PARALLAX         7

/* Panel Types */
#define PANEL_TOP               0
#define PANEL_BOTTOM            1
#define PANEL_LEFT              2
#define PANEL_RIGHT             3
#define PANEL_FLOATING          4
#define PANEL_DOCK              5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/* Gradient Definition */
typedef struct {
    u32 colors[8];
    u32 stops[8];
    u32 color_count;
    float angle;
    float center_x;
    float center_y;
} gradient_def_t;

/* Background Definition */
typedef struct {
    char name[64];
    u32 type;
    u32 solid_color;
    gradient_def_t gradient;
    char image_path[256];
    char video_path[256];
    bool tiling;
    float scale;
    float opacity;
    u32 change_interval_minutes;
    bool slideshow;
    char **slideshow_images;
    u32 slideshow_count;
} background_def_t;

/* Glassmorphic Effect Settings */
typedef struct {
    bool enabled;
    float blur_radius;
    float transparency;
    u32 border_color;
    float border_width;
    float corner_radius;
    float noise_amount;
    bool tint_enabled;
    u32 tint_color;
    float tint_opacity;
    bool shadow_enabled;
    float shadow_blur;
    float shadow_offset_x;
    float shadow_offset_y;
    u32 shadow_color;
} glassmorphic_settings_t;

/* Animation Settings */
typedef struct {
    bool enabled;
    float speed;
    u32 type;  /* 0=fade, 1=slide, 2=zoom, 3=rotate */
    bool loop;
    float delay;
} animation_settings_t;

/* Panel Configuration */
typedef struct {
    u32 id;
    u32 type;
    bool visible;
    bool auto_hide;
    u32 height;
    u32 width;
    u32 position;
    bool floating;
    float opacity;
    bool glassmorphic;
    glassmorphic_settings_t glass_settings;
    char **widgets;
    u32 widget_count;
} panel_config_t;

/* Display Settings Manager */
typedef struct {
    bool initialized;
    
    /* Display Info */
    u32 screen_width;
    u32 screen_height;
    u32 refresh_rate;
    u32 color_depth;
    float dpi;
    float scale_factor;
    bool hdr_enabled;
    bool night_light;
    float color_temperature;
    
    /* Background */
    background_def_t backgrounds[MAX_BACKGROUNDS];
    u32 background_count;
    u32 current_background;
    
    /* Effects */
    glassmorphic_settings_t glass_settings;
    animation_settings_t animation_settings;
    bool effects_enabled;
    
    /* Panels */
    panel_config_t panels[MAX_PANELS];
    u32 panel_count;
    
    /* OS Info */
    char os_name[64];
    char os_version[32];
    char os_codename[64];
    char build_number[32];
    char kernel_version[32];
    char architecture[16];
    u64 install_date;
    char registered_owner[128];
    
    /* System Info */
    char cpu_model[128];
    u32 cpu_cores;
    u32 cpu_threads;
    u64 total_memory;
    u64 available_memory;
    char gpu_model[128];
    u64 gpu_memory;
    char motherboard[128];
    char bios_version[64];
    
    spinlock_t lock;
} display_settings_manager_t;

static display_settings_manager_t g_display_settings;

/*===========================================================================*/
/*                         OS VERSION INFO                                   */
/*===========================================================================*/

/* Get OS Version String */
const char *display_get_os_version(void)
{
    return g_display_settings.os_version;
}

/* Get Full OS Info */
void display_get_os_info(char *buffer, u32 buffer_size)
{
    snprintf(buffer, buffer_size,
             "%s %s (%s)\n"
             "Build: %s\n"
             "Kernel: %s\n"
             "Architecture: %s\n"
             "Installed: %lu\n"
             "Owner: %s",
             g_display_settings.os_name,
             g_display_settings.os_version,
             g_display_settings.os_codename,
             g_display_settings.build_number,
             g_display_settings.kernel_version,
             g_display_settings.architecture,
             g_display_settings.install_date,
             g_display_settings.registered_owner);
}

/* Initialize OS Info */
void display_init_os_info(void)
{
    strncpy(g_display_settings.os_name, "NEXUS OS", 63);
    strncpy(g_display_settings.os_version, NEXUS_VERSION_STRING, 31);
    strncpy(g_display_settings.os_codename, "Genesis Edition", 63);
    strncpy(g_display_settings.build_number, "2026.03.14", 31);
    strncpy(g_display_settings.kernel_version, "1.0.0", 31);
    strncpy(g_display_settings.architecture, "x86_64", 15);
    strncpy(g_display_settings.registered_owner, "User", 127);
    g_display_settings.install_date = 0;  /* Would get from system */
    
    printk("[DISPLAY] OS: %s %s (%s)\n",
           g_display_settings.os_name,
           g_display_settings.os_version,
           g_display_settings.os_codename);
}

/* Initialize System Info */
void display_init_system_info(void)
{
    /* CPU Info */
    strncpy(g_display_settings.cpu_model, "Intel Core i7 / AMD Ryzen 7", 127);
    g_display_settings.cpu_cores = 8;
    g_display_settings.cpu_threads = 16;
    
    /* Memory Info */
    g_display_settings.total_memory = 16 * 1024 * 1024 * 1024ULL;  /* 16 GB */
    g_display_settings.available_memory = 12 * 1024 * 1024 * 1024ULL;  /* 12 GB */
    
    /* GPU Info */
    strncpy(g_display_settings.gpu_model, "NVIDIA RTX 3080 / AMD RX 6800", 127);
    g_display_settings.gpu_memory = 10 * 1024 * 1024 * 1024ULL;  /* 10 GB */
    
    /* Motherboard/BIOS */
    strncpy(g_display_settings.motherboard, "NEXUS Motherboard v1.0", 127);
    strncpy(g_display_settings.bios_version, "NEXUS BIOS 1.0.0", 63);
    
    printk("[DISPLAY] CPU: %s (%d cores, %d threads)\n",
           g_display_settings.cpu_model,
           g_display_settings.cpu_cores,
           g_display_settings.cpu_threads);
    printk("[DISPLAY] Memory: %lu GB / %lu GB\n",
           g_display_settings.total_memory / (1024*1024*1024),
           g_display_settings.available_memory / (1024*1024*1024));
    printk("[DISPLAY] GPU: %s (%lu GB)\n",
           g_display_settings.gpu_model,
           g_display_settings.gpu_memory / (1024*1024*1024));
}

/*===========================================================================*/
/*                         BACKGROUND PRESETS                                */
/*===========================================================================*/

/* Initialize Background Presets */
void display_init_backgrounds(void)
{
    printk("[DISPLAY] Initializing background presets...\n");
    
    /* Solid Colors */
    background_def_t *bg;
    
    /* 1. Midnight Blue */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "Midnight Blue", 63);
    bg->type = BG_TYPE_SOLID;
    bg->solid_color = 0x1E3A5F;
    
    /* 2. Dark Gray */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "Dark Gray", 63);
    bg->type = BG_TYPE_SOLID;
    bg->solid_color = 0x2C3E50;
    
    /* 3. Black */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "Pure Black", 63);
    bg->type = BG_TYPE_SOLID;
    bg->solid_color = 0x000000;
    
    /* Gradients */
    /* 4. Sunset Gradient */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "Sunset", 63);
    bg->type = BG_TYPE_GRADIENT_H;
    bg->gradient.colors[0] = 0xFF6B6B;
    bg->gradient.colors[1] = 0xFFA07A;
    bg->gradient.colors[2] = 0xFFD700;
    bg->gradient.stops[0] = 0;
    bg->gradient.stops[1] = 128;
    bg->gradient.stops[2] = 255;
    bg->gradient.color_count = 3;
    
    /* 5. Ocean Gradient */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "Ocean", 63);
    bg->type = BG_TYPE_GRADIENT_V;
    bg->gradient.colors[0] = 0x006994;
    bg->gradient.colors[1] = 0x40E0D0;
    bg->gradient.colors[2] = 0x48D1CC;
    bg->gradient.stops[0] = 0;
    bg->gradient.stops[1] = 128;
    bg->gradient.stops[2] = 255;
    bg->gradient.color_count = 3;
    
    /* 6. Aurora Gradient */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "Aurora", 63);
    bg->type = BG_TYPE_GRADIENT_RADIAL;
    bg->gradient.colors[0] = 0x00FF87;
    bg->gradient.colors[1] = 0x60EFF3;
    bg->gradient.colors[2] = 0x0061FF;
    bg->gradient.center_x = 0.5;
    bg->gradient.center_y = 0.5;
    bg->gradient.color_count = 3;
    
    /* 7. Glassmorphic Background */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "Glassmorphic", 63);
    bg->type = BG_TYPE_GLASSMORPHIC;
    bg->gradient.colors[0] = 0xFFFFFF;
    bg->gradient.colors[1] = 0xF0F0F0;
    bg->gradient.color_count = 2;
    bg->opacity = 0.3;
    
    /* 8. NEXUS Brand */
    bg = &g_display_settings.backgrounds[g_display_settings.background_count++];
    strncpy(bg->name, "NEXUS Brand", 63);
    bg->type = BG_TYPE_GRADIENT_V;
    bg->gradient.colors[0] = 0x1E3A5F;
    bg->gradient.colors[1] = 0x3498DB;
    bg->gradient.colors[2] = 0x5DADE2;
    bg->gradient.stops[0] = 0;
    bg->gradient.stops[1] = 128;
    bg->gradient.stops[2] = 255;
    bg->gradient.color_count = 3;
    
    g_display_settings.current_background = 7;  /* Default to NEXUS Brand */
    
    printk("[DISPLAY] %u background presets loaded\n", g_display_settings.background_count);
}

/*===========================================================================*/
/*                         GLASSMORPHIC EFFECTS                              */
/*===========================================================================*/

/* Initialize Glassmorphic Settings */
void display_init_glassmorphic(void)
{
    glassmorphic_settings_t *glass = &g_display_settings.glass_settings;
    
    glass->enabled = true;
    glass->blur_radius = 20.0;
    glass->transparency = 0.7;
    glass->border_color = 0xFFFFFF;
    glass->border_width = 1.0;
    glass->corner_radius = 12.0;
    glass->noise_amount = 0.05;
    glass->tint_enabled = false;
    glass->tint_color = 0xFFFFFF;
    glass->tint_opacity = 0.1;
    glass->shadow_enabled = true;
    glass->shadow_blur = 20.0;
    glass->shadow_offset_x = 0.0;
    glass->shadow_offset_y = 4.0;
    glass->shadow_color = 0x000000;
    
    printk("[DISPLAY] Glassmorphic effects initialized\n");
}

/* Apply Glassmorphic Effect */
void display_apply_glassmorphic(glassmorphic_settings_t *settings)
{
    if (!settings->enabled) {
        return;
    }
    
    printk("[DISPLAY] Applying glassmorphic effect:\n");
    printk("[DISPLAY]   Blur: %.1fpx\n", settings->blur_radius);
    printk("[DISPLAY]   Transparency: %.0f%%\n", settings->transparency * 100);
    printk("[DISPLAY]   Corner Radius: %.1fpx\n", settings->corner_radius);
    printk("[DISPLAY]   Shadow: %s\n", settings->shadow_enabled ? "Enabled" : "Disabled");
    printk("[DISPLAY]   Noise: %.0f%%\n", settings->noise_amount * 100);
}

/*===========================================================================*/
/*                         PANEL CONFIGURATION                               */
/*===========================================================================*/

/* Initialize Default Panels */
void display_init_panels(void)
{
    printk("[DISPLAY] Initializing panels...\n");
    
    panel_config_t *panel;
    
    /* Top Panel */
    panel = &g_display_settings.panels[g_display_settings.panel_count++];
    panel->id = 0;
    panel->type = PANEL_TOP;
    panel->visible = true;
    panel->auto_hide = false;
    panel->height = 28;
    panel->opacity = 0.9;
    panel->glassmorphic = true;
    memcpy(&panel->glass_settings, &g_display_settings.glass_settings, sizeof(glassmorphic_settings_t));
    
    /* Widgets for top panel */
    panel->widgets = (char **)kmalloc(sizeof(char *) * 8);
    panel->widget_count = 8;
    panel->widgets[0] = "start-menu";
    panel->widgets[1] = "search";
    panel->widgets[2] = "tasks";
    panel->widgets[3] = "spacer";
    panel->widgets[4] = "network";
    panel->widgets[5] = "sound";
    panel->widgets[6] = "battery";
    panel->widgets[7] = "clock";
    
    /* Bottom Panel (Dock) */
    panel = &g_display_settings.panels[g_display_settings.panel_count++];
    panel->id = 1;
    panel->type = PANEL_DOCK;
    panel->visible = true;
    panel->auto_hide = false;
    panel->height = 64;
    panel->opacity = 0.85;
    panel->glassmorphic = true;
    memcpy(&panel->glass_settings, &g_display_settings.glass_settings, sizeof(glassmorphic_settings_t));
    
    /* Widgets for dock */
    panel->widgets = (char **)kmalloc(sizeof(char *) * 12);
    panel->widget_count = 12;
    panel->widgets[0] = "files";
    panel->widgets[1] = "browser";
    panel->widgets[2] = "terminal";
    panel->widgets[3] = "store";
    panel->widgets[4] = "spacer";
    panel->widgets[5] = "settings";
    panel->widgets[6] = "vm-manager";
    panel->widgets[7] = "container";
    panel->widgets[8] = "spacer";
    panel->widgets[9] = "trash";
    panel->widgets[10] = "minimize-all";
    panel->widgets[11] = "show-desktop";
    
    /* Left Panel (Optional) */
    panel = &g_display_settings.panels[g_display_settings.panel_count++];
    panel->id = 2;
    panel->type = PANEL_LEFT;
    panel->visible = false;
    panel->auto_hide = true;
    panel->width = 64;
    panel->opacity = 0.8;
    panel->glassmorphic = true;
    
    /* Right Panel (Optional) */
    panel = &g_display_settings.panels[g_display_settings.panel_count++];
    panel->id = 3;
    panel->type = PANEL_RIGHT;
    panel->visible = false;
    panel->auto_hide = true;
    panel->width = 320;
    panel->opacity = 0.9;
    panel->glassmorphic = true;
    
    printk("[DISPLAY] %u panels configured\n", g_display_settings.panel_count);
}

/*===========================================================================*/
/*                         DISPLAY SETTINGS UI                               */
/*===========================================================================*/

/* Show Display Settings */
void display_show_settings(void)
{
    printk("\n[DISPLAY] ====== DISPLAY SETTINGS ======\n");
    
    printk("[DISPLAY] Screen:\n");
    printk("[DISPLAY]   Resolution: %ux%u\n", g_display_settings.screen_width, g_display_settings.screen_height);
    printk("[DISPLAY]   Refresh Rate: %u Hz\n", g_display_settings.refresh_rate);
    printk("[DISPLAY]   Color Depth: %u-bit\n", g_display_settings.color_depth);
    printk("[DISPLAY]   DPI: %.1f\n", g_display_settings.dpi);
    printk("[DISPLAY]   Scale: %.0f%%\n", g_display_settings.scale_factor * 100);
    printk("[DISPLAY]   HDR: %s\n", g_display_settings.hdr_enabled ? "Enabled" : "Disabled");
    printk("[DISPLAY]   Night Light: %s\n", g_display_settings.night_light ? "Enabled" : "Disabled");
    printk("[DISPLAY]   Color Temp: %u K\n", (u32)g_display_settings.color_temperature);
    
    printk("[DISPLAY]\nBackground:\n");
    printk("[DISPLAY]   Current: %s\n", g_display_settings.backgrounds[g_display_settings.current_background].name);
    printk("[DISPLAY]   Type: %u\n", g_display_settings.backgrounds[g_display_settings.current_background].type);
    printk("[DISPLAY]   Available: %u\n", g_display_settings.background_count);
    
    printk("[DISPLAY]\nEffects:\n");
    printk("[DISPLAY]   Glassmorphic: %s\n", g_display_settings.glass_settings.enabled ? "Enabled" : "Disabled");
    printk("[DISPLAY]   Blur: %.1fpx\n", g_display_settings.glass_settings.blur_radius);
    printk("[DISPLAY]   Transparency: %.0f%%\n", g_display_settings.glass_settings.transparency * 100);
    printk("[DISPLAY]   Animations: %s\n", g_display_settings.animation_settings.enabled ? "Enabled" : "Disabled");
    
    printk("[DISPLAY]\nPanels:\n");
    for (u32 i = 0; i < g_display_settings.panel_count; i++) {
        panel_config_t *panel = &g_display_settings.panels[i];
        const char *type_names[] = { "Top", "Bottom", "Left", "Right", "Floating", "Dock" };
        printk("[DISPLAY]   %s Panel: %s, Auto-hide: %s, Glass: %s\n",
               type_names[panel->type],
               panel->visible ? "Visible" : "Hidden",
               panel->auto_hide ? "Yes" : "No",
               panel->glassmorphic ? "Yes" : "No");
    }
    
    printk("[DISPLAY] ================================\n\n");
}

/* Show OS Info */
void display_show_os_info(void)
{
    printk("\n[DISPLAY] ====== OS INFORMATION ======\n");
    printk("[DISPLAY] %s %s\n", g_display_settings.os_name, g_display_settings.os_version);
    printk("[DISPLAY] Codename: %s\n", g_display_settings.os_codename);
    printk("[DISPLAY] Build: %s\n", g_display_settings.build_number);
    printk("[DISPLAY] Kernel: %s\n", g_display_settings.kernel_version);
    printk("[DISPLAY] Architecture: %s\n", g_display_settings.architecture);
    printk("[DISPLAY] Installed: %lu\n", g_display_settings.install_date);
    printk("[DISPLAY] Owner: %s\n", g_display_settings.registered_owner);
    printk("[DISPLAY] =============================\n\n");
}

/* Show System Info */
void display_show_system_info(void)
{
    printk("\n[DISPLAY] ====== SYSTEM INFORMATION ======\n");
    printk("[DISPLAY] CPU: %s\n", g_display_settings.cpu_model);
    printk("[DISPLAY]   Cores: %u\n", g_display_settings.cpu_cores);
    printk("[DISPLAY]   Threads: %u\n", g_display_settings.cpu_threads);
    printk("[DISPLAY] Memory: %lu GB total, %lu GB available\n",
           g_display_settings.total_memory / (1024*1024*1024),
           g_display_settings.available_memory / (1024*1024*1024));
    printk("[DISPLAY] GPU: %s\n", g_display_settings.gpu_model);
    printk("[DISPLAY]   Memory: %lu GB\n", g_display_settings.gpu_memory / (1024*1024*1024));
    printk("[DISPLAY] Motherboard: %s\n", g_display_settings.motherboard);
    printk("[DISPLAY] BIOS: %s\n", g_display_settings.bios_version);
    printk("[DISPLAY] ==================================\n\n");
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/* Initialize Display Settings Manager */
int display_settings_init(void)
{
    printk("[DISPLAY] ========================================\n");
    printk("[DISPLAY] NEXUS OS Display Settings Manager\n");
    printk("[DISPLAY] ========================================\n");
    
    memset(&g_display_settings, 0, sizeof(display_settings_manager_t));
    g_display_settings.lock.lock = 0;
    
    /* Display Info */
    g_display_settings.screen_width = 1920;
    g_display_settings.screen_height = 1080;
    g_display_settings.refresh_rate = 60;
    g_display_settings.color_depth = 32;
    g_display_settings.dpi = 96.0;
    g_display_settings.scale_factor = 1.0;
    g_display_settings.hdr_enabled = false;
    g_display_settings.night_light = false;
    g_display_settings.color_temperature = 6500.0;
    
    /* Initialize components */
    display_init_os_info();
    display_init_system_info();
    display_init_backgrounds();
    display_init_glassmorphic();
    display_init_panels();
    
    /* Animation settings */
    g_display_settings.animation_settings.enabled = true;
    g_display_settings.animation_settings.speed = 1.0;
    g_display_settings.animation_settings.type = 0;
    g_display_settings.animation_settings.loop = true;
    g_display_settings.animation_settings.delay = 0.0;
    
    g_display_settings.effects_enabled = true;
    g_display_settings.initialized = true;
    
    printk("[DISPLAY] Display settings initialized\n");
    printk("[DISPLAY] ========================================\n");
    
    return 0;
}
