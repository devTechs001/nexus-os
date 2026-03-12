/*
 * NEXUS OS - Graphical Boot Manager
 * boot/bootloader/gui-boot.c
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Graphical boot menu with terminal, mouse support, and theme
 */

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         BOOT MENU CONSTANTS                               */
/*===========================================================================*/

#define BOOT_MENU_MAX_ENTRIES       16
#define BOOT_MENU_WIDTH             800
#define BOOT_MENU_HEIGHT            600
#define BOOT_MENU_BG_COLOR          0x1A1A2E
#define BOOT_MENU_FG_COLOR          0xEEEEEE
#define BOOT_MENUAccent_COLOR       0xE94560
#define BOOT_MENU_HOVER_COLOR       0x1F4287
#define BOOT_MENU_TIMEOUT_SEC       10

/*===========================================================================*/
/*                         BOOT MENU STRUCTURES                              */
/*===========================================================================*/

/**
 * boot_entry_t - Boot menu entry
 */
typedef struct {
    char name[128];
    char description[256];
    char kernel_path[256];
    char cmdline[512];
    char icon[64];
    int type;  /* 0=OS, 1=Utility, 2=Test */
    bool enabled;
} boot_entry_t;

/**
 * boot_menu_t - Graphical boot menu
 */
typedef struct {
    boot_entry_t entries[BOOT_MENU_MAX_ENTRIES];
    int entry_count;
    int selected;
    int timeout;
    bool timeout_active;
    
    /* Display info */
    int width;
    int height;
    int bpp;
    
    /* Mouse */
    int mouse_x;
    int mouse_y;
    bool mouse_visible;
    
    /* Theme */
    u32 bg_color;
    u32 fg_color;
    u32 accent_color;
    
    /* Logo */
    char logo_text[64];
    
    /* Framebuffer */
    void *fb_base;
    size_t fb_size;
    int fb_pitch;
} boot_menu_t;

/*===========================================================================*/
/*                         GLOBAL BOOT MENU                                  */
/*===========================================================================*/

static boot_menu_t g_boot_menu;

/*===========================================================================*/
/*                         FRAMEBUFFER FUNCTIONS                             */
/*===========================================================================*/

/**
 * fb_init - Initialize framebuffer
 */
static int fb_init(boot_menu_t *menu)
{
    /* In real implementation, get framebuffer info from bootloader */
    menu->fb_base = (void *)0xE0000000;  /* Common framebuffer address */
    menu->fb_size = menu->width * menu->height * 4;
    menu->fb_pitch = menu->width * 4;
    
    return 0;
}

/**
 * fb_pixel - Draw a pixel
 */
static void fb_pixel(boot_menu_t *menu, int x, int y, u32 color)
{
    if (x < 0 || x >= menu->width || y < 0 || y >= menu->height) {
        return;
    }
    
    u32 *fb = (u32 *)menu->fb_base;
    fb[y * menu->width + x] = color;
}

/**
 * fb_rect - Draw a rectangle
 */
static void fb_rect(boot_menu_t *menu, int x, int y, int w, int h, u32 color)
{
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            fb_pixel(menu, ix, iy, color);
        }
    }
}

/**
 * fb_clear - Clear screen
 */
static void fb_clear(boot_menu_t *menu, u32 color)
{
    fb_rect(menu, 0, 0, menu->width, menu->height, color);
}

/**
 * fb_char - Draw a character (simple bitmap font)
 */
static void fb_char(boot_menu_t *menu, int x, int y, char c, u32 color)
{
    /* Simple 8x16 bitmap font */
    static const u8 font[128][16] = {
        /* Basic ASCII font would go here */
        /* For now, just draw a box */
    };
    
    /* Placeholder - in real implementation, draw actual font */
    fb_rect(menu, x, y, 8, 16, color);
}

/**
 * fb_text - Draw a string
 */
static void fb_text(boot_menu_t *menu, int x, int y, const char *text, u32 color)
{
    int cx = x;
    while (*text) {
        fb_char(menu, cx, y, *text, color);
        cx += 9;  /* 8px char + 1px spacing */
        text++;
    }
}

/**
 * fb_circle - Draw a circle
 */
static void fb_circle(boot_menu_t *menu, int cx, int cy, int r, u32 color)
{
    int x = r;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        fb_pixel(menu, cx + x, cy + y, color);
        fb_pixel(menu, cx + y, cy + x, color);
        fb_pixel(menu, cx - y, cy + x, color);
        fb_pixel(menu, cx - x, cy + y, color);
        fb_pixel(menu, cx - x, cy - y, color);
        fb_pixel(menu, cx - y, cy - x, color);
        fb_pixel(menu, cx + y, cy - x, color);
        fb_pixel(menu, cx + x, cy - y, color);
        
        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

/*===========================================================================*/
/*                         BOOT MENU RENDERING                               */
/*===========================================================================*/

/**
 * boot_menu_draw_background - Draw boot menu background
 */
static void boot_menu_draw_background(boot_menu_t *menu)
{
    /* Gradient background */
    for (int y = 0; y < menu->height; y++) {
        u32 color = menu->bg_color;
        /* Simple gradient effect */
        fb_rect(menu, 0, y, menu->width, 1, color);
    }
    
    /* Draw decorative circles */
    fb_circle(menu, menu->width - 100, 100, 150, menu->accent_color & 0x333333);
    fb_circle(menu, 150, menu->height - 150, 200, menu->accent_color & 0x222222);
}

/**
 * boot_menu_draw_logo - Draw NEXUS logo
 */
static void boot_menu_draw_logo(boot_menu_t *menu)
{
    int logo_x = menu->width / 2 - 200;
    int logo_y = 40;
    
    /* Draw logo text */
    fb_text(menu, logo_x, logo_y, "NEXUS OS", menu->accent_color);
    fb_text(menu, logo_x, logo_y + 20, "Universal Operating System", menu->fg_color);
    
    /* Draw version */
    char version[64];
    snprintf(version, sizeof(version), "Version 1.0.0 (Genesis) - Copyright 2026");
    fb_text(menu, logo_x, logo_y + 45, version, 0x888888);
}

/**
 * boot_menu_draw_entry - Draw a boot menu entry
 */
static void boot_menu_draw_entry(boot_menu_t *menu, int index, int x, int y, int w, int h)
{
    boot_entry_t *entry = &menu->entries[index];
    bool selected = (index == menu->selected);
    
    /* Background */
    u32 bg_color = selected ? menu->accent_color : (menu->bg_color | 0x202020);
    fb_rect(menu, x, y, w, h, bg_color);
    
    /* Border for selected */
    if (selected) {
        fb_rect(menu, x, y, w, 3, menu->accent_color);
        fb_rect(menu, x, y + h - 3, w, 3, menu->accent_color);
        fb_rect(menu, x, y, 3, h, menu->accent_color);
        fb_rect(menu, x + w - 3, y, 3, h, menu->accent_color);
    }
    
    /* Icon placeholder */
    fb_rect(menu, x + 15, y + 15, 32, 32, selected ? 0xFFFFFF : 0x888888);
    
    /* Name */
    fb_text(menu, x + 60, y + 20, entry->name, selected ? 0xFFFFFF : menu->fg_color);
    
    /* Description */
    fb_text(menu, x + 60, y + 45, entry->description, 0x888888);
    
    /* Type indicator */
    if (entry->type == 1) {
        fb_text(menu, x + w - 80, y + 25, "[UTIL]", menu->accent_color);
    } else if (entry->type == 2) {
        fb_text(menu, x + w - 80, y + 25, "[TEST]", menu->accent_color);
    }
}

/**
 * boot_menu_draw_timeout - Draw timeout bar
 */
static void boot_menu_draw_timeout(boot_menu_t *menu)
{
    if (!menu->timeout_active) return;
    
    int bar_x = menu->width / 2 - 150;
    int bar_y = menu->height - 60;
    int bar_w = 300;
    int bar_h = 20;
    
    /* Background */
    fb_rect(menu, bar_x, bar_y, bar_w, bar_h, 0x333333);
    
    /* Progress */
    int progress_w = (menu->timeout * bar_w) / BOOT_MENU_TIMEOUT_SEC;
    fb_rect(menu, bar_x, bar_y, progress_w, bar_h, menu->accent_color);
    
    /* Text */
    char text[64];
    snprintf(text, sizeof(text), "Booting in %d seconds...", menu->timeout);
    fb_text(menu, bar_x + 80, bar_y + 3, text, menu->fg_color);
}

/**
 * boot_menu_draw_help - Draw help text
 */
static void boot_menu_draw_help(boot_menu_t *menu)
{
    int help_y = menu->height - 30;
    
    fb_text(menu, 20, help_y, "↑↓ : Select", 0x666666);
    fb_text(menu, 150, help_y, "Enter : Boot", 0x666666);
    fb_text(menu, 280, help_y, "E : Edit", 0x666666);
    fb_text(menu, 380, help_y, "T : Terminal", 0x666666);
    fb_text(menu, 500, help_y, "R : Restart", 0x666666);
    fb_text(menu, 620, help_y, "S : Shutdown", 0x666666);
}

/**
 * boot_menu_render - Render complete boot menu
 */
static void boot_menu_render(boot_menu_t *menu)
{
    /* Background */
    boot_menu_draw_background(menu);
    
    /* Logo */
    boot_menu_draw_logo(menu);
    
    /* Boot entries */
    int entry_x = menu->width / 2 - 300;
    int entry_y = 150;
    int entry_w = 600;
    int entry_h = 70;
    int entry_spacing = 10;
    
    for (int i = 0; i < menu->entry_count; i++) {
        boot_menu_draw_entry(menu, i, entry_x, entry_y + i * (entry_h + entry_spacing),
                            entry_w, entry_h);
    }
    
    /* Timeout */
    boot_menu_draw_timeout(menu);
    
    /* Help */
    boot_menu_draw_help(menu);
    
    /* Mouse cursor */
    if (menu->mouse_visible) {
        fb_rect(menu, menu->mouse_x, menu->mouse_y, 3, 3, 0xFFFFFF);
    }
}

/*===========================================================================*/
/*                         BOOT MENU INPUT                                   */
/*===========================================================================*/

/**
 * boot_menu_handle_key - Handle keyboard input
 */
static int boot_menu_handle_key(boot_menu_t *menu, u32 key)
{
    menu->timeout_active = false;  /* Stop timeout on user input */
    
    switch (key) {
    case 0x48:  /* Up arrow */
        menu->selected--;
        if (menu->selected < 0) menu->selected = menu->entry_count - 1;
        break;
        
    case 0x50:  /* Down arrow */
        menu->selected++;
        if (menu->selected >= menu->entry_count) menu->selected = 0;
        break;
        
    case 0x1C:  /* Enter */
        return menu->selected + 1;  /* Return selected index + 1 */
        
    case 0x12:  /* E - Edit */
        /* TODO: Open edit dialog */
        break;
        
    case 0x14:  /* T - Terminal */
        /* TODO: Open terminal */
        break;
        
    case 0x13:  /* R - Restart */
        return -1;  /* Signal restart */
        
    case 0x1F:  /* S - Shutdown */
        return -2;  /* Signal shutdown */
    }
    
    return 0;  /* Continue */
}

/*===========================================================================*/
/*                         BOOT MENU INITIALIZATION                          */
/*===========================================================================*/

/**
 * boot_menu_init - Initialize boot menu
 */
static void boot_menu_init(boot_menu_t *menu)
{
    memset(menu, 0, sizeof(boot_menu_t));
    
    /* Display settings */
    menu->width = BOOT_MENU_WIDTH;
    menu->height = BOOT_MENU_HEIGHT;
    menu->bpp = 32;
    
    /* Colors */
    menu->bg_color = BOOT_MENU_BG_COLOR;
    menu->fg_color = BOOT_MENU_FG_COLOR;
    menu->accent_color = BOOT_MENUAccent_COLOR;
    
    /* Timeout */
    menu->timeout = BOOT_MENU_TIMEOUT_SEC;
    menu->timeout_active = true;
    
    /* Mouse */
    menu->mouse_visible = true;
    menu->mouse_x = menu->width / 2;
    menu->mouse_y = menu->height / 2;
    
    /* Logo */
    strcpy(menu->logo_text, "NEXUS OS");
    
    /* Add boot entries */
    menu->entry_count = 0;
    
    /* Entry 0: VMware Mode (Default) */
    boot_entry_t *e0 = &menu->entries[menu->entry_count++];
    strcpy(e0->name, "NEXUS OS (VMware Mode)");
    strcpy(e0->description, "Optimized for VMware - Default selection");
    strcpy(e0->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(e0->cmdline, "quiet splash virt=vmware");
    strcpy(e0->icon, "os");
    e0->type = 0;
    e0->enabled = true;
    
    /* Entry 1: Safe Mode */
    boot_entry_t *e1 = &menu->entries[menu->entry_count++];
    strcpy(e1->name, "NEXUS OS (Safe Mode)");
    strcpy(e1->description, "Minimal drivers, no SMP - For troubleshooting");
    strcpy(e1->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(e1->cmdline, "nomodeset nosmp noapic nolapic");
    strcpy(e1->icon, "safe");
    e1->type = 0;
    e1->enabled = true;
    
    /* Entry 2: Debug Mode */
    boot_entry_t *e2 = &menu->entries[menu->entry_count++];
    strcpy(e2->name, "NEXUS OS (Debug Mode)");
    strcpy(e2->description, "Verbose logging - For developers");
    strcpy(e2->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(e2->cmdline, "debug loglevel=7 earlyprintk=vga");
    strcpy(e2->icon, "debug");
    e2->type = 0;
    e2->enabled = true;
    
    /* Entry 3: Native Mode */
    boot_entry_t *e3 = &menu->entries[menu->entry_count++];
    strcpy(e3->name, "NEXUS OS (Native Mode)");
    strcpy(e3->description, "Bare metal - No virtualization");
    strcpy(e3->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(e3->cmdline, "virt=native");
    strcpy(e3->icon, "native");
    e3->type = 0;
    e3->enabled = true;
    
    /* Entry 4: Memory Test */
    boot_entry_t *e4 = &menu->entries[menu->entry_count++];
    strcpy(e4->name, "Memory Test (MemTest86)");
    strcpy(e4->description, "Test system memory for errors");
    strcpy(e4->kernel_path, "/boot/memtest86.bin");
    strcpy(e4->cmdline, "");
    strcpy(e4->icon, "test");
    e4->type = 2;
    e4->enabled = true;
    
    /* Entry 5: Terminal */
    boot_entry_t *e5 = &menu->entries[menu->entry_count++];
    strcpy(e5->name, "Terminal / Shell");
    strcpy(e5->description, "Command line interface");
    strcpy(e5->kernel_path, "/boot/shell.bin");
    strcpy(e5->cmdline, "");
    strcpy(e5->icon, "terminal");
    e5->type = 1;
    e5->enabled = true;
    
    /* Select first entry by default */
    menu->selected = 0;
    
    /* Initialize framebuffer */
    fb_init(menu);
}

/*===========================================================================*/
/*                         BOOT MENU MAIN LOOP                               */
/*===========================================================================*/

/**
 * boot_menu_run - Run boot menu
 * Returns: Selected entry index, -1 for restart, -2 for shutdown
 */
static int boot_menu_run(boot_menu_t *menu)
{
    u64 last_tick = get_time_ms();
    u64 timeout_tick = last_tick + (BOOT_MENU_TIMEOUT_SEC * 1000);
    
    while (1) {
        /* Render */
        boot_menu_render(menu);
        
        /* Update timeout */
        u64 current_tick = get_time_ms();
        if (menu->timeout_active && current_tick >= timeout_tick) {
            /* Timeout expired - boot default */
            return menu->selected + 1;
        }
        
        /* Update timeout display */
        if (menu->timeout_active) {
            menu->timeout = (timeout_tick - current_tick) / 1000;
        }
        
        /* Handle input (in real implementation, read from keyboard) */
        /* u32 key = keyboard_read(); */
        /* int result = boot_menu_handle_key(menu, key); */
        /* if (result != 0) return result; */
        
        /* Small delay */
        delay_ms(16);  /* ~60 FPS */
    }
}

/*===========================================================================*/
/*                         GRAPHICAL BOOT ENTRY POINT                        */
/*===========================================================================*/

/**
 * gui_boot_main - Graphical boot manager entry point
 */
int gui_boot_main(void)
{
    printk("[BOOT] Starting graphical boot manager...\n");
    
    /* Initialize boot menu */
    boot_menu_init(&g_boot_menu);
    
    printk("[BOOT] Boot menu initialized with %d entries\n", g_boot_menu.entry_count);
    
    /* Run boot menu */
    int selected = boot_menu_run(&g_boot_menu);
    
    if (selected < 0) {
        /* Restart or shutdown */
        if (selected == -1) {
            printk("[BOOT] Restart requested\n");
            /* reboot(); */
        } else {
            printk("[BOOT] Shutdown requested\n");
            /* shutdown(); */
        }
    } else {
        /* Boot selected entry */
        boot_entry_t *entry = &g_boot_menu.entries[selected - 1];
        printk("[BOOT] Booting: %s\n", entry->name);
        printk("[BOOT] Kernel: %s\n", entry->kernel_path);
        printk("[BOOT] Cmdline: %s\n", entry->cmdline);
        
        /* In real implementation, load kernel and boot */
        /* load_and_boot_kernel(entry->kernel_path, entry->cmdline); */
    }
    
    return selected;
}
