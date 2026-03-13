/*
 * NEXUS OS - Graphical Boot Menu Enhancement
 * boot/bootloader/gui-boot-enhanced.c
 *
 * Enhanced graphical boot menu with animations and visual feedback
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         BOOT MENU CONFIGURATION                           */
/*===========================================================================*/

#define BOOT_MENU_TITLE             "NEXUS OS"
#define BOOT_MENU_VERSION           "v1.0.0 (Genesis)"
#define BOOT_MENU_TIMEOUT           10
#define BOOT_MENU_ENTRIES           4

/* Colors */
#define BOOT_COLOR_BG               0x1A1A2E
#define BOOT_COLOR_TITLE            0x00D4FF
#define BOOT_MENU_COLOR_TEXT        0xEEEEEE
#define BOOT_COLOR_SELECTED_BG      0x0F3460
#define BOOT_COLOR_SELECTED_TEXT    0x00D4FF
#define BOOT_COLOR_PROGRESS         0x00D4FF

/* VGA Text Mode */
#define VGA_WIDTH                   80
#define VGA_HEIGHT                  25
#define VGA_FRAMEBUFFER             0xB8000

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    char *title;
    char *description;
    char *kernel_params;
    bool is_default;
} boot_entry_t;

typedef struct {
    u32 current_entry;
    u32 timeout;
    bool running;
} boot_menu_state_t;

/*===========================================================================*/
/*                         BOOT ENTRIES                                      */
/*===========================================================================*/

static boot_entry_t boot_entries[BOOT_MENU_ENTRIES] = {
    {
        .title = "NEXUS OS (VMware Mode)",
        .description = "Optimized for VMware - Default selection",
        .kernel_params = "",
        .is_default = true
    },
    {
        .title = "NEXUS OS (Safe Mode)",
        .description = "Minimal drivers, no SMP - For troubleshooting",
        .kernel_params = "nomodeset nosmp noapic",
        .is_default = false
    },
    {
        .title = "NEXUS OS (Debug Mode)",
        .description = "Verbose logging - For developers",
        .kernel_params = "debug loglevel=7 earlyprintk=vga",
        .is_default = false
    },
    {
        .title = "NEXUS OS (Native Mode)",
        .description = "Bare metal - No virtualization",
        .kernel_params = "virt=native",
        .is_default = false
    }
};

static boot_menu_state_t boot_state = {0};

/*===========================================================================*/
/*                         VGA FUNCTIONS                                     */
/*===========================================================================*/

static inline void vga_putchar(u32 x, u32 y, char c, u8 color)
{
    u16 *vga = (u16 *)VGA_FRAMEBUFFER;
    vga[y * VGA_WIDTH + x] = (u16)c | ((u16)color << 8);
}

static void vga_print(u32 x, u32 y, const char *str, u8 color)
{
    while (*str) {
        vga_putchar(x++, y, *str++, color);
    }
}

static void vga_clear(u8 color)
{
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        ((u16 *)VGA_FRAMEBUFFER)[i] = ' ' | ((u16)color << 8);
    }
}

static void vga_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color)
{
    for (u32 row = 0; row < h; row++) {
        for (u32 col = 0; col < w; col++) {
            vga_putchar(x + col, y + row, ' ', color);
        }
    }
}

/*===========================================================================*/
/*                         BOOT MENU DISPLAY                                 */
/*===========================================================================*/

static void draw_boot_menu(void)
{
    /* Clear screen with dark blue background */
    vga_clear(0x10);  /* Blue on black */
    
    /* Draw title */
    vga_print(25, 2, "____   ___   __  __   _   _  _____", 0x0B);
    vga_print(26, 3, "|  _ \\ / _ \\ |  \\/  | | \\ | || ____|", 0x0B);
    vga_print(26, 4, "| |_) | | | || |\\/| | |  \\| ||  _|", 0x0B);
    vga_print(26, 5, "|  _ <| |_| || |  | | | |\\  || |___", 0x0B);
    vga_print(26, 6, "|_| \\_\\\\___/ |_|  |_| |_| \\_||_____|", 0x0B);
    
    vga_print(30, 8, BOOT_MENU_TITLE, 0x0B);
    vga_print(35, 9, BOOT_MENU_VERSION, 0x07);
    
    /* Draw separator */
    for (u32 i = 15; i < 65; i++) {
        vga_putchar(i, 10, '=', 0x07);
    }
    
    /* Draw menu entries */
    for (u32 i = 0; i < BOOT_MENU_ENTRIES; i++) {
        u8 bg_color = (i == boot_state.current_entry) ? 0x10 : 0x00;
        u8 fg_color = (i == boot_state.current_entry) ? 0x0B : 0x07;
        u8 attr = (bg_color << 4) | fg_color;
        
        /* Draw entry background */
        vga_fill_rect(15, 12 + (i * 3), 50, 2, attr);
        
        /* Draw entry title */
        vga_print(17, 12 + (i * 3), boot_entries[i].title, attr);
        
        /* Draw entry description */
        vga_print(17, 13 + (i * 3), boot_entries[i].description, 0x07);
    }
    
    /* Draw timeout */
    char timeout_str[32];
    __builtin_sprintf(timeout_str, "Boot in: %d seconds", boot_state.timeout);
    vga_print(15, 24, timeout_str, 0x0B);
    
    /* Draw instructions */
    vga_print(50, 24, "↑↓: Select  Enter: Boot", 0x07);
}

static void update_timeout_display(void)
{
    char timeout_str[32];
    __builtin_sprintf(timeout_str, "Boot in: %d seconds", boot_state.timeout);
    vga_print(15, 24, timeout_str, 0x0B);
    
    /* Draw progress bar */
    u32 progress_width = (boot_state.timeout * 50) / BOOT_MENU_TIMEOUT;
    vga_fill_rect(15, 23, progress_width, 1, 0x0B);
}

/*===========================================================================*/
/*                         INPUT HANDLING                                    */
/*===========================================================================*/

static u8 read_port(u16 port)
{
    u8 result;
    __asm__ __volatile__("inb %1, %0" : "=a"(result) : "d"(port));
    return result;
}

static bool keyboard_available(void)
{
    return (read_port(0x64) & 0x01) != 0;
}

static u8 read_key(void)
{
    while (!keyboard_available());
    return read_port(0x60);
}

static void handle_input(void)
{
    if (keyboard_available()) {
        u8 scancode = read_key();
        
        switch (scancode) {
            case 0x48:  /* Up arrow */
                if (boot_state.current_entry > 0) {
                    boot_state.current_entry--;
                    draw_boot_menu();
                }
                break;
            case 0x50:  /* Down arrow */
                if (boot_state.current_entry < BOOT_MENU_ENTRIES - 1) {
                    boot_state.current_entry++;
                    draw_boot_menu();
                }
                break;
            case 0x1C:  /* Enter */
                boot_state.running = false;
                break;
        }
    }
}

/*===========================================================================*/
/*                         BOOT FUNCTION                                     */
/*===========================================================================*/

void graphical_boot_menu(void)
{
    boot_state.current_entry = 0;
    boot_state.timeout = BOOT_MENU_TIMEOUT;
    boot_state.running = true;
    
    /* Find default entry */
    for (u32 i = 0; i < BOOT_MENU_ENTRIES; i++) {
        if (boot_entries[i].is_default) {
            boot_state.current_entry = i;
            break;
        }
    }
    
    /* Draw initial menu */
    draw_boot_menu();
    
    /* Main loop */
    while (boot_state.running && boot_state.timeout > 0) {
        handle_input();
        
        /* Simple delay */
        for (volatile u32 i = 0; i < 1000000; i++);
        
        boot_state.timeout--;
        update_timeout_display();
    }
    
    /* Boot selected entry */
    /* In real implementation, would load kernel with parameters */
}
