/*
 * NEXUS OS - Terminal Animations & Visual Effects
 * apps/terminal/terminal_animations.c
 *
 * Advanced terminal animations, color schemes, and visual effects
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ANIMATION CONFIGURATION                           */
/*===========================================================================*/

#define MAX_ANIMATIONS          64
#define MAX_COLOR_SCHEMES       32
#define MAX_FONTS               16
#define MAX_PROMPT_STYLES       32

/* Animation Types */
#define ANIM_CURSOR_BLINK       1
#define ANIM_SCROLL_SMOOTH      2
#define ANIM_FADE_IN            3
#define ANIM_FADE_OUT           4
#define ANIM_TYPING             5
#define ANIM_WAVE               6
#define ANIM_PULSE              7
#define ANIM_RAINBOW            8
#define ANIM_MATRIX             9
#define ANIM_GLITCH             10

/* Animation Speed */
#define ANIM_SPEED_SLOW         1000    /* ms */
#define ANIM_SPEED_NORMAL       500     /* ms */
#define ANIM_SPEED_FAST         250     /* ms */
#define ANIM_SPEED_INSTANT      0       /* ms */

/* Cursor Styles */
#define CURSOR_BLOCK            0
#define CURSOR_UNDERLINE        1
#define CURSOR_VERTICAL         2
#define CURSOR_BOX              3

/* Scroll Effects */
#define SCROLL_NONE             0
#define SCROLL_SMOOTH           1
#define SCROLL_BOUNCE           2
#define SCROLL_ELASTIC          3
#define SCROLL_PARALLAX         4

/* Text Effects */
#define TEXT_NONE               0
#define TEXT_BOLD               1
#define TEXT_FAINT              2
#define TEXT_ITALIC             3
#define TEXT_UNDERLINE          4
#define TEXT_BLINK              5
#define TEXT_REVERSE            6
#define TEXT_HIDDEN             7
#define TEXT_STRIKETHROUGH      8
#define TEXT_DOUBLE_UNDERLINE   9
#define TEXT_OVERLINE           10

/*===========================================================================*/
/*                         COLOR SCHEMES                                     */
/*===========================================================================*/

/* Color Scheme Definition */
typedef struct {
    char name[64];
    char author[64];
    char description[256];
    u32 colors[16];  /* ANSI colors 0-15 */
    u32 fg_default;
    u32 bg_default;
    u32 cursor_fg;
    u32 cursor_bg;
    u32 selection_bg;
    bool bright_bold;
    bool transparency;
    u32 transparency_percent;
} color_scheme_t;

/* Global Color Schemes */
static color_scheme_t g_color_schemes[MAX_COLOR_SCHEMES];
static u32 g_scheme_count = 0;
static u32 g_current_scheme = 0;

/*===========================================================================*/
/*                         TERMINAL PREFERENCES                              */
/*===========================================================================*/

/* Terminal Preferences */
typedef struct {
    /* Appearance */
    char theme_name[64];
    u32 font_size;
    char font_family[64];
    bool font_bold;
    bool font_italic;
    bool font_ligatures;
    bool anti_aliasing;
    
    /* Window */
    u32 width_chars;
    u32 height_chars;
    bool show_title_bar;
    bool show_tab_bar;
    bool show_scrollbar;
    bool show_status_bar;
    bool fullscreen;
    bool maximized;
    u32 opacity_percent;
    bool blur_background;
    bool rounded_corners;
    u32 corner_radius;
    
    /* Cursor */
    u32 cursor_style;
    bool cursor_blink;
    u32 cursor_blink_rate_ms;
    bool cursor_block_filled;
    
    /* Text */
    u32 line_height;
    u32 letter_spacing;
    bool smooth_scrolling;
    bool scrollback_unlimited;
    u32 scrollback_lines;
    
    /* Input */
    bool natural_scrolling;
    bool mouse_reporting;
    bool alt_sends_escape;
    bool ctrl_shift_shortcuts;
    
    /* Bell */
    bool audible_bell;
    bool visual_bell;
    u32 bell_volume;
    
    /* Selection */
    bool copy_on_select;
    bool right_click_paste;
    bool triple_click_select_line;
    
    /* Advanced */
    bool gpu_acceleration;
    bool vsync;
    bool pixel_perfect;
    bool true_color;
    bool unicode_support;
    bool rtl_support;
} terminal_preferences_t;

/* Global Preferences */
static terminal_preferences_t g_term_prefs;
static bool g_prefs_initialized = false;

/*===========================================================================*/
/*                         ANIMATION STATE                                   */
/*===========================================================================*/

/* Animation Entry */
typedef struct {
    u32 id;
    u32 type;
    bool active;
    u64 start_time;
    u64 duration_ms;
    u32 progress_percent;
    void *data;
    void (*callback)(void *);
} animation_t;

/* Animation Manager */
typedef struct {
    animation_t animations[MAX_ANIMATIONS];
    u32 active_count;
    u32 next_id;
    bool enabled;
    u32 global_speed;  /* 50 = half speed, 100 = normal, 200 = double */
} animation_manager_t;

static animation_manager_t g_anim_manager;

/*===========================================================================*/
/*                         COLOR SCHEME INITIALIZATION                       */
/*===========================================================================*/

/* Add Color Scheme */
int color_scheme_add(const char *name, const char *author, const char *desc,
                     u32 *colors, u32 fg, u32 bg)
{
    if (g_scheme_count >= MAX_COLOR_SCHEMES || !name) {
        return -EINVAL;
    }
    
    color_scheme_t *scheme = &g_color_schemes[g_scheme_count++];
    memset(scheme, 0, sizeof(color_scheme_t));
    
    strncpy(scheme->name, name, 63);
    strncpy(scheme->author, author ? author : "Unknown", 63);
    strncpy(scheme->description, desc ? desc : "", 255);
    
    for (u32 i = 0; i < 16; i++) {
        scheme->colors[i] = colors[i];
    }
    
    scheme->fg_default = fg;
    scheme->bg_default = bg;
    scheme->cursor_fg = bg;
    scheme->cursor_bg = fg;
    scheme->selection_bg = 0x44475A;
    scheme->bright_bold = true;
    scheme->transparency = false;
    scheme->transparency_percent = 90;
    
    printk("[TERM-COLOR] Added scheme: %s by %s\n", name, scheme->author);
    
    return 0;
}

/* Initialize Built-in Color Schemes */
void color_schemes_init(void)
{
    printk("[TERM-COLOR] ========================================\n");
    printk("[TERM-COLOR] Initializing color schemes\n");
    printk("[TERM-COLOR] ========================================\n");
    
    /* Dracula (Dark) */
    u32 dracula[16] = {
        0x21222C, 0xFF5555, 0x50FA7B, 0xF1FA8C,
        0xBD93F9, 0xFF79C6, 0x8BE9FD, 0xF8F8F2,
        0x6272A4, 0xFF6E6E, 0x69FF94, 0xFFFFA5,
        0xD6ACFF, 0xFF92DF, 0xA4FFFF, 0xFFFFFF
    };
    color_scheme_add("Dracula", "Zeno Rocha", "Dark theme with vibrant colors",
                     dracula, 0xF8F8F2, 0x282A36);
    
    /* Monokai (Dark) */
    u32 monokai[16] = {
        0x272822, 0xF92672, 0xA6E22E, 0xF4BF75,
        0x66D9EF, 0xAE81FF, 0xA1EFE4, 0xF8F8F2,
        0x75715E, 0xF92672, 0xA6E22E, 0xF4BF75,
        0x66D9EF, 0xAE81FF, 0xA1EFE4, 0xF8F8F0
    };
    color_scheme_add("Monokai", "Wimer Hazenberg", "Classic dark theme",
                     monokai, 0xF8F8F2, 0x272822);
    
    /* Solarized Dark */
    u32 solarized_dark[16] = {
        0x073642, 0xDC322F, 0x859900, 0xB58900,
        0x268BD2, 0xD33682, 0x2AA198, 0xEEEEEC,
        0x002B36, 0xCB4B16, 0x586E75, 0x657B83,
        0x839496, 0x6C71C4, 0x93A1A1, 0xFDF6E3
    };
    color_scheme_add("Solarized Dark", "Ethan Schoonover", "Carefully designed dark theme",
                     solarized_dark, 0x839496, 0x002B36);
    
    /* Gruvbox Dark */
    u32 gruvbox[16] = {
        0x282828, 0xCC241D, 0x98971A, 0xD79921,
        0x458588, 0xB16286, 0x689D6A, 0xA89984,
        0x928374, 0xFB4934, 0xB8BB26, 0xFABD2F,
        0x83A598, 0xD3869B, 0x8EC07C, 0xEBDBB2
    };
    color_scheme_add("Gruvbox Dark", "Pavel Pertsev", "Retro groove dark theme",
                     gruvbox, 0xEBDBB2, 0x282828);
    
    /* Nord */
    u32 nord[16] = {
        0x3B4252, 0xBF616A, 0xA3BE8C, 0xEBCB8B,
        0x81A1C1, 0xB48EAD, 0x88C0D0, 0xE5E9F0,
        0x4C566A, 0xBF616A, 0xA3BE8C, 0xEBCB8B,
        0x81A1C1, 0xB48EAD, 0x8FBCBB, 0xD8DEE9
    };
    color_scheme_add("Nord", "Arctic Ice Studio", "Arctic north theme",
                     nord, 0xD8DEE9, 0x2E3440);
    
    /* One Dark */
    u32 one_dark[16] = {
        0x1E2127, 0xE06C75, 0x98C379, 0xD19A66,
        0x61AFEF, 0xC678DD, 0x56B6C2, 0xABB2BF,
        0x5C6370, 0xE06C75, 0x98C379, 0xD19A66,
        0x61AFEF, 0xC678DD, 0x56B6C2, 0xFFFFFF
    };
    color_scheme_add("One Dark", "Atom", "Atom's default dark theme",
                     one_dark, 0xABB2BF, 0x282C34);
    
    /* Material Dark */
    u32 material[16] = {
        0x212121, 0xF07178, 0xC3E88D, 0xFFCB6B,
        0x82AAFF, 0xC792EA, 0x89DDFF, 0xEEFFFF,
        0x4A4A4A, 0xFF8B92, 0xDDFFA7, 0xFFE585,
        0x9CC4FF, 0xE1ACFF, 0xA3F7FF, 0xFFFFFF
    };
    color_scheme_add("Material Dark", "Mattia Astorino", "Google Material Design dark",
                     material, 0xEEFFFF, 0x212121);
    
    /* Tomorrow Night */
    u32 tomorrow[16] = {
        0x1D1F21, 0xCC6666, 0xB5BD68, 0xF0C674,
        0x81A2BE, 0xB294BB, 0x8ABEB7, 0xC5C8C6,
        0x969896, 0xFF3334, 0x9EC400, 0xF0C674,
        0x81A2BE, 0xB77EE0, 0x54CED6, 0xFFFFFF
    };
    color_scheme_add("Tomorrow Night", "Chris Kempson", "Clean dark theme",
                     tomorrow, 0xC5C8C6, 0x1D1F21);
    
    /* Light themes */
    
    /* Solarized Light */
    u32 solarized_light[16] = {
        0x073642, 0xDC322F, 0x859900, 0xB58900,
        0x268BD2, 0xD33682, 0x2AA198, 0xEEE8D5,
        0x002B36, 0xCB4B16, 0x586E75, 0x657B83,
        0x839496, 0x6C71C4, 0x93A1A1, 0xFDF6E3
    };
    color_scheme_add("Solarized Light", "Ethan Schoonover", "Carefully designed light theme",
                     solarized_light, 0x657B83, 0xFDF6E3);
    
    /* GitHub Light */
    u32 github[16] = {
        0x24292E, 0xD73A49, 0x22863A, 0xB08800,
        0x0366D6, 0x6F42C1, 0x24292E, 0x24292E,
        0x959DA5, 0xCB2431, 0x22863A, 0xB08800,
        0x0366D6, 0x6F42C1, 0x24292E, 0x24292E
    };
    color_scheme_add("GitHub Light", "GitHub", "GitHub's light theme",
                     github, 0x24292E, 0xFFFFFF);
    
    printk("[TERM-COLOR] %u color schemes loaded\n", g_scheme_count);
}

/* Set Current Color Scheme */
int color_scheme_set(const char *name)
{
    for (u32 i = 0; i < g_scheme_count; i++) {
        if (strcmp(g_color_schemes[i].name, name) == 0) {
            g_current_scheme = i;
            printk("[TERM-COLOR] Switched to %s theme\n", name);
            return 0;
        }
    }
    return -ENOENT;
}

/* List Color Schemes */
void color_schemes_list(void)
{
    printk("\n[TERM-COLOR] ====== COLOR SCHEMES ======\n");

    for (u32 i = 0; i < g_scheme_count; i++) {
        color_scheme_t *scheme = &g_color_schemes[i];
        const char *curr_marker = (i == g_current_scheme) ? " (current)" : "";
        printk("[TERM-COLOR] %2u. %-20s by %-15s%s\n",
               i + 1, scheme->name, scheme->author, curr_marker);
    }

    printk("[TERM-COLOR] Total: %u schemes\n", g_scheme_count);
    printk("[TERM-COLOR] Use 'theme <name>' to switch\n");
    printk("[TERM-COLOR] =============================\n\n");
}

/*===========================================================================*/
/*                         PREFERENCES INITIALIZATION                        */
/*===========================================================================*/

/* Initialize Default Preferences */
int preferences_init(void)
{
    if (g_prefs_initialized) {
        return 0;
    }
    
    printk("[TERM-PREFS] ========================================\n");
    printk("[TERM-PREFS] Initializing terminal preferences\n");
    printk("[TERM-PREFS] ========================================\n");
    
    memset(&g_term_prefs, 0, sizeof(terminal_preferences_t));
    
    /* Appearance */
    strncpy(g_term_prefs.theme_name, "Dracula", 63);
    g_term_prefs.font_size = 14;
    strncpy(g_term_prefs.font_family, "Consolas", 63);
    g_term_prefs.font_bold = false;
    g_term_prefs.font_italic = false;
    g_term_prefs.font_ligatures = true;
    g_term_prefs.anti_aliasing = true;
    
    /* Window */
    g_term_prefs.width_chars = 120;
    g_term_prefs.height_chars = 40;
    g_term_prefs.show_title_bar = true;
    g_term_prefs.show_tab_bar = true;
    g_term_prefs.show_scrollbar = true;
    g_term_prefs.show_status_bar = true;
    g_term_prefs.fullscreen = false;
    g_term_prefs.maximized = false;
    g_term_prefs.opacity_percent = 100;
    g_term_prefs.blur_background = true;
    g_term_prefs.rounded_corners = true;
    g_term_prefs.corner_radius = 8;
    
    /* Cursor */
    g_term_prefs.cursor_style = CURSOR_BLOCK;
    g_term_prefs.cursor_blink = true;
    g_term_prefs.cursor_blink_rate_ms = 500;
    g_term_prefs.cursor_block_filled = true;
    
    /* Text */
    g_term_prefs.line_height = 20;
    g_term_prefs.letter_spacing = 0;
    g_term_prefs.smooth_scrolling = true;
    g_term_prefs.scrollback_unlimited = false;
    g_term_prefs.scrollback_lines = 10000;
    
    /* Input */
    g_term_prefs.natural_scrolling = false;
    g_term_prefs.mouse_reporting = true;
    g_term_prefs.alt_sends_escape = true;
    g_term_prefs.ctrl_shift_shortcuts = true;
    
    /* Bell */
    g_term_prefs.audible_bell = false;
    g_term_prefs.visual_bell = true;
    g_term_prefs.bell_volume = 50;
    
    /* Selection */
    g_term_prefs.copy_on_select = true;
    g_term_prefs.right_click_paste = true;
    g_term_prefs.triple_click_select_line = true;
    
    /* Advanced */
    g_term_prefs.gpu_acceleration = true;
    g_term_prefs.vsync = true;
    g_term_prefs.pixel_perfect = true;
    g_term_prefs.true_color = true;
    g_term_prefs.unicode_support = true;
    g_term_prefs.rtl_support = false;
    
    g_prefs_initialized = true;
    
    printk("[TERM-PREFS] Preferences initialized\n");
    printk("[TERM-PREFS] Theme: %s\n", g_term_prefs.theme_name);
    printk("[TERM-PREFS] Font: %s %upx\n", g_term_prefs.font_family, g_term_prefs.font_size);
    printk("[TERM-PREFS] Cursor: %s, blink %ums\n",
           g_term_prefs.cursor_style == CURSOR_BLOCK ? "Block" :
           g_term_prefs.cursor_style == CURSOR_UNDERLINE ? "Underline" : "Vertical",
           g_term_prefs.cursor_blink_rate_ms);
    printk("[TERM-PREFS] GPU Acceleration: %s\n", g_term_prefs.gpu_acceleration ? "Enabled" : "Disabled");
    printk("[TERM-PREFS] ========================================\n");
    
    return 0;
}

/* Get Preference Value */
const char *preferences_get_string(const char *key)
{
    if (!key) return NULL;
    
    if (strcmp(key, "theme") == 0) return g_term_prefs.theme_name;
    if (strcmp(key, "font_family") == 0) return g_term_prefs.font_family;
    
    return NULL;
}

u32 preferences_get_int(const char *key)
{
    if (!key) return 0;
    
    if (strcmp(key, "font_size") == 0) return g_term_prefs.font_size;
    if (strcmp(key, "cursor_style") == 0) return g_term_prefs.cursor_style;
    if (strcmp(key, "cursor_blink_rate") == 0) return g_term_prefs.cursor_blink_rate_ms;
    if (strcmp(key, "line_height") == 0) return g_term_prefs.line_height;
    if (strcmp(key, "scrollback_lines") == 0) return g_term_prefs.scrollback_lines;
    if (strcmp(key, "opacity") == 0) return g_term_prefs.opacity_percent;
    
    return 0;
}

bool preferences_get_bool(const char *key)
{
    if (!key) return false;
    
    if (strcmp(key, "cursor_blink") == 0) return g_term_prefs.cursor_blink;
    if (strcmp(key, "font_ligatures") == 0) return g_term_prefs.font_ligatures;
    if (strcmp(key, "smooth_scrolling") == 0) return g_term_prefs.smooth_scrolling;
    if (strcmp(key, "gpu_acceleration") == 0) return g_term_prefs.gpu_acceleration;
    if (strcmp(key, "copy_on_select") == 0) return g_term_prefs.copy_on_select;
    if (strcmp(key, "visual_bell") == 0) return g_term_prefs.visual_bell;
    
    return false;
}

/* Set Preference */
int preferences_set(const char *key, const char *value)
{
    if (!key || !value) return -EINVAL;
    
    if (strcmp(key, "theme") == 0) {
        strncpy(g_term_prefs.theme_name, value, 63);
        color_scheme_set(value);
        printk("[TERM-PREFS] Theme set to: %s\n", value);
    } else if (strcmp(key, "font_size") == 0) {
        g_term_prefs.font_size = atoi(value);
        printk("[TERM-PREFS] Font size set to: %u\n", g_term_prefs.font_size);
    } else if (strcmp(key, "cursor_style") == 0) {
        if (strcmp(value, "block") == 0) g_term_prefs.cursor_style = CURSOR_BLOCK;
        else if (strcmp(value, "underline") == 0) g_term_prefs.cursor_style = CURSOR_UNDERLINE;
        else if (strcmp(value, "vertical") == 0) g_term_prefs.cursor_style = CURSOR_VERTICAL;
        printk("[TERM-PREFS] Cursor style set to: %s\n", value);
    } else if (strcmp(key, "cursor_blink") == 0) {
        g_term_prefs.cursor_blink = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        printk("[TERM-PREFS] Cursor blink: %s\n", g_term_prefs.cursor_blink ? "Enabled" : "Disabled");
    }
    
    return 0;
}

/* Print All Preferences */
void preferences_print(void)
{
    printk("\n[TERM-PREFS] ====== PREFERENCES ======\n");
    
    printk("[TERM-PREFS] Appearance:\n");
    printk("[TERM-PREFS]   Theme: %s\n", g_term_prefs.theme_name);
    printk("[TERM-PREFS]   Font: %s %upx\n", g_term_prefs.font_family, g_term_prefs.font_size);
    printk("[TERM-PREFS]   Ligatures: %s\n", g_term_prefs.font_ligatures ? "Yes" : "No");
    printk("[TERM-PREFS]   Anti-alias: %s\n", g_term_prefs.anti_aliasing ? "Yes" : "No");
    
    printk("[TERM-PREFS] Window:\n");
    printk("[TERM-PREFS]   Size: %ux%u chars\n", g_term_prefs.width_chars, g_term_prefs.height_chars);
    printk("[TERM-PREFS]   Title bar: %s\n", g_term_prefs.show_title_bar ? "Visible" : "Hidden");
    printk("[TERM-PREFS]   Tab bar: %s\n", g_term_prefs.show_tab_bar ? "Visible" : "Hidden");
    printk("[TERM-PREFS]   Scrollbar: %s\n", g_term_prefs.show_scrollbar ? "Visible" : "Hidden");
    printk("[TERM-PREFS]   Status bar: %s\n", g_term_prefs.show_status_bar ? "Visible" : "Hidden");
    printk("[TERM-PREFS]   Opacity: %u%%\n", g_term_prefs.opacity_percent);
    printk("[TERM-PREFS]   Blur: %s\n", g_term_prefs.blur_background ? "Enabled" : "Disabled");
    printk("[TERM-PREFS]   Rounded corners: %s (%upx)\n",
           g_term_prefs.rounded_corners ? "Yes" : "No", g_term_prefs.corner_radius);
    
    printk("[TERM-PREFS] Cursor:\n");
    printk("[TERM-PREFS]   Style: %s\n",
           g_term_prefs.cursor_style == CURSOR_BLOCK ? "Block" :
           g_term_prefs.cursor_style == CURSOR_UNDERLINE ? "Underline" : "Vertical");
    printk("[TERM-PREFS]   Blink: %s (%ums)\n",
           g_term_prefs.cursor_blink ? "Yes" : "No", g_term_prefs.cursor_blink_rate_ms);
    
    printk("[TERM-PREFS] Text:\n");
    printk("[TERM-PREFS]   Line height: %upx\n", g_term_prefs.line_height);
    printk("[TERM-PREFS]   Smooth scroll: %s\n", g_term_prefs.smooth_scrolling ? "Yes" : "No");
    printk("[TERM-PREFS]   Scrollback: %u lines\n", g_term_prefs.scrollback_lines);
    
    printk("[TERM-PREFS] Advanced:\n");
    printk("[TERM-PREFS]   GPU acceleration: %s\n", g_term_prefs.gpu_acceleration ? "Enabled" : "Disabled");
    printk("[TERM-PREFS]   VSync: %s\n", g_term_prefs.vsync ? "Enabled" : "Disabled");
    printk("[TERM-PREFS]   True color: %s\n", g_term_prefs.true_color ? "Enabled" : "Disabled");
    printk("[TERM-PREFS]   Unicode: %s\n", g_term_prefs.unicode_support ? "Enabled" : "Disabled");
    
    printk("[TERM-PREFS] =============================\n\n");
}

/*===========================================================================*/
/*                         ANIMATION MANAGER                                 */
/*===========================================================================*/

/* Initialize Animation Manager */
int animation_manager_init(void)
{
    printk("[TERM-ANIM] ========================================\n");
    printk("[TERM-ANIM] Initializing animation system\n");
    printk("[TERM-ANIM] ========================================\n");
    
    memset(&g_anim_manager, 0, sizeof(animation_manager_t));
    g_anim_manager.enabled = true;
    g_anim_manager.global_speed = 100;  /* Normal speed */
    
    printk("[TERM-ANIM] Max animations: %u\n", MAX_ANIMATIONS);
    printk("[TERM-ANIM] Global speed: %u%%\n", g_anim_manager.global_speed);
    printk("[TERM-ANIM] Animation system ready\n");
    printk("[TERM-ANIM] ========================================\n");
    
    return 0;
}

/* Create Animation */
u32 animation_create(u32 type, u64 duration_ms, void *data, void (*callback)(void *))
{
    if (g_anim_manager.active_count >= MAX_ANIMATIONS) {
        return 0;
    }
    
    /* Find free slot */
    for (u32 i = 0; i < MAX_ANIMATIONS; i++) {
        if (!g_anim_manager.animations[i].active) {
            animation_t *anim = &g_anim_manager.animations[i];
            anim->id = ++g_anim_manager.next_id;
            anim->type = type;
            anim->active = true;
            anim->start_time = 0;  /* Would get from kernel */
            anim->duration_ms = duration_ms;
            anim->progress_percent = 0;
            anim->data = data;
            anim->callback = callback;
            
            g_anim_manager.active_count++;
            
            printk("[TERM-ANIM] Created animation %u (type: %u, duration: %lums)\n",
                   anim->id, type, duration_ms);
            
            return anim->id;
        }
    }
    
    return 0;
}

/* Update Animations */
void animation_update(void)
{
    u64 current_time = 0;  /* Would get from kernel */
    
    for (u32 i = 0; i < MAX_ANIMATIONS; i++) {
        animation_t *anim = &g_anim_manager.animations[i];
        
        if (anim->active && anim->duration_ms > 0) {
            u64 elapsed = current_time - anim->start_time;
            anim->progress_percent = (elapsed * 100) / anim->duration_ms;
            
            if (anim->progress_percent >= 100) {
                anim->progress_percent = 100;
                anim->active = false;
                g_anim_manager.active_count--;
                
                if (anim->callback) {
                    anim->callback(anim->data);
                }
                
                printk("[TERM-ANIM] Animation %u completed\n", anim->id);
            }
        }
    }
}

/* Stop Animation */
void animation_stop(u32 id)
{
    for (u32 i = 0; i < MAX_ANIMATIONS; i++) {
        if (g_anim_manager.animations[i].id == id) {
            g_anim_manager.animations[i].active = false;
            g_anim_manager.active_count--;
            printk("[TERM-ANIM] Stopped animation %u\n", id);
            return;
        }
    }
}

/* Stop All Animations */
void animation_stop_all(void)
{
    for (u32 i = 0; i < MAX_ANIMATIONS; i++) {
        g_anim_manager.animations[i].active = false;
    }
    g_anim_manager.active_count = 0;
    printk("[TERM-ANIM] All animations stopped\n");
}

/* Set Animation Speed */
void animation_set_speed(u32 speed_percent)
{
    g_anim_manager.global_speed = speed_percent;
    printk("[TERM-ANIM] Global animation speed: %u%%\n", speed_percent);
}

/*===========================================================================*/
/*                         VISUAL EFFECTS                                    */
/*===========================================================================*/

/* Cursor Blink Effect */
void effect_cursor_blink(bool visible)
{
    /* Toggle cursor visibility */
    printk("[TERM-EFFECT] Cursor %s\n", visible ? "shown" : "hidden");
}

/* Scroll Smooth Effect */
void effect_scroll_smooth(u32 lines)
{
    printk("[TERM-EFFECT] Smooth scroll %u lines\n", lines);
}

/* Fade In Effect */
void effect_fade_in(u32 duration_ms)
{
    animation_create(ANIM_FADE_IN, duration_ms, NULL, NULL);
    printk("[TERM-EFFECT] Fade in (%lums)\n", duration_ms);
}

/* Fade Out Effect */
void effect_fade_out(u32 duration_ms)
{
    animation_create(ANIM_FADE_OUT, duration_ms, NULL, NULL);
    printk("[TERM-EFFECT] Fade out (%lums)\n", duration_ms);
}

/* Typing Effect */
void effect_typing(const char *text, u32 speed_ms)
{
    printk("[TERM-EFFECT] Typing effect: '%s' (%ums/char)\n", text, speed_ms);
}

/* Rainbow Effect */
void effect_rainbow(u32 duration_ms)
{
    animation_create(ANIM_RAINBOW, duration_ms, NULL, NULL);
    printk("[TERM-EFFECT] Rainbow effect (%lums)\n", duration_ms);
}

/* Matrix Effect */
void effect_matrix(u32 duration_ms)
{
    animation_create(ANIM_MATRIX, duration_ms, NULL, NULL);
    printk("[TERM-EFFECT] Matrix effect (%lums)\n", duration_ms);
}

/* Glitch Effect */
void effect_glitch(u32 intensity, u32 duration_ms)
{
    printk("[TERM-EFFECT] Glitch effect (intensity: %u, %lums)\n", intensity, duration_ms);
}

/* Bell Effects */
void effect_bell_visual(void)
{
    if (g_term_prefs.visual_bell) {
        /* Flash screen */
        printk("[TERM-EFFECT] Visual bell\n");
    }
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int terminal_visual_init(void)
{
    printk("[TERMINAL] ========================================\n");
    printk("[TERMINAL] Visual Effects & Preferences\n");
    printk("[TERMINAL] ========================================\n");
    
    /* Initialize color schemes */
    color_schemes_init();
    
    /* Initialize preferences */
    preferences_init();
    
    /* Initialize animations */
    animation_manager_init();
    
    printk("[TERMINAL] Visual system initialized\n");
    printk("[TERMINAL] ========================================\n");
    
    return 0;
}

/* Print Visual Status */
void terminal_visual_status(void)
{
    printk("\n[TERMINAL] ====== VISUAL STATUS ======\n");
    printk("[TERMINAL] Color schemes: %u\n", g_scheme_count);
    printk("[TERMINAL] Current theme: %s\n", g_color_schemes[g_current_scheme].name);
    printk("[TERMINAL] Font: %s %upx\n", g_term_prefs.font_family, g_term_prefs.font_size);
    printk("[TERMINAL] Cursor: %s, blink %ums\n",
           g_term_prefs.cursor_style == CURSOR_BLOCK ? "Block" : "Underline",
           g_term_prefs.cursor_blink_rate_ms);
    printk("[TERMINAL] Animations: %u active\n", g_anim_manager.active_count);
    printk("[TERMINAL] GPU: %s\n", g_term_prefs.gpu_acceleration ? "Enabled" : "Disabled");
    printk("[TERMINAL] =============================\n\n");
}
