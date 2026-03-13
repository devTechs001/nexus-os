/*
 * NEXUS OS - Graphical Boot Splash Screen
 * drivers/video/splash.c
 *
 * Displays NEXUS OS boot splash with progress indicator
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         SPLASH CONFIGURATION                              */
/*===========================================================================*/

#define SPLASH_WIDTH        800
#define SPLASH_HEIGHT       600
#define SPLASH_BG_COLOR     0x1a1a2e
#define SPLASH_TEXT_COLOR   0x16c79a
#define SPLASH_ACCENT       0x0f3460
#define PROGRESS_BAR_WIDTH  400
#define PROGRESS_BAR_HEIGHT 8

/*===========================================================================*/
/*                         SPLASH STATE                                      */
/*===========================================================================*/

typedef struct {
    bool initialized;
    u32 width;
    u32 height;
    u32 *framebuffer;
    u32 pitch;
    u32 bpp;
} splash_state_t;

static splash_state_t g_splash;

/*===========================================================================*/
/*                         GRAPHICS PRIMITIVES                               */
/*===========================================================================*/

/**
 * splash_put_pixel - Draw a pixel on splash screen
 */
static inline void splash_put_pixel(u32 x, u32 y, u32 color)
{
    if (!g_splash.initialized || x >= g_splash.width || y >= g_splash.height)
        return;
    
    u32 *pixel = g_splash.framebuffer + (y * g_splash.pitch / 4) + x;
    *pixel = color;
}

/**
 * splash_draw_rect - Draw a filled rectangle
 */
static void splash_draw_rect(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    for (u32 py = y; py < y + h && py < g_splash.height; py++) {
        for (u32 px = x; px < x + w && px < g_splash.width; px++) {
            splash_put_pixel(px, py, color);
        }
    }
}

/**
 * splash_draw_circle - Draw a filled circle
 */
static void splash_draw_circle(u32 cx, u32 cy, u32 radius, u32 color)
{
    s32 x = radius;
    s32 y = 0;
    s32 err = 0;

    while (x >= y) {
        splash_draw_rect(cx - y, cy - x, y * 2 + 1, x * 2 + 1, color);
        
        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

/**
 * splash_draw_gradient - Draw horizontal gradient
 */
static void splash_draw_gradient(u32 x, u32 y, u32 w, u32 h, u32 color1, u32 color2)
{
    for (u32 py = y; py < y + h && py < g_splash.height; py++) {
        for (u32 px = x; px < x + w && px < g_splash.width; px++) {
            u32 t = (px - x) * 256 / w;
            u32 r = ((color1 & 0xFF0000) >> 16) * (256 - t) / 256 + ((color2 & 0xFF0000) >> 16) * t / 256;
            u32 g = ((color1 & 0x00FF00) >> 8) * (256 - t) / 256 + ((color2 & 0x00FF00) >> 8) * t / 256;
            u32 b = (color1 & 0x0000FF) * (256 - t) / 256 + (color2 & 0x0000FF) * t / 256;
            splash_put_pixel(px, py, (r << 16) | (g << 8) | b);
        }
    }
}

/*===========================================================================*/
/*                         SPLASH SCREEN FUNCTIONS                           */
/*===========================================================================*/

/**
 * splash_draw_logo - Draw NEXUS OS logo
 */
static void splash_draw_logo(void)
{
    u32 center_x = g_splash.width / 2;
    u32 center_y = g_splash.height / 3;
    
    // Draw outer glow circles
    for (u32 i = 0; i < 5; i++) {
        u32 alpha = 50 - i * 10;
        u32 color = (alpha << 16) | ((alpha * 199 / 255) << 8) | (alpha * 154 / 255);
        splash_draw_circle(center_x, center_y, 80 + i * 10, color);
    }
    
    // Draw main logo circle
    splash_draw_circle(center_x, center_y, 80, SPLASH_TEXT_COLOR);
    
    // Draw inner circle
    splash_draw_circle(center_x, center_y, 60, SPLASH_BG_COLOR);
    
    // Draw NEXUS text placeholder (simplified as geometric shapes)
    splash_draw_rect(center_x - 40, center_y - 20, 80, 40, SPLASH_TEXT_COLOR);
}

/**
 * splash_draw_text - Draw text string (simplified)
 */
static void splash_draw_text(const char *text, u32 x, u32 y, u32 color)
{
    // Simplified text rendering - in real implementation would use fonts
    (void)text;
    (void)x;
    (void)y;
    (void)color;
    // Text is rendered via console for now
}

/**
 * splash_draw_progress - Draw progress bar
 */
static void splash_draw_progress(u32 percent)
{
    u32 bar_x = (g_splash.width - PROGRESS_BAR_WIDTH) / 2;
    u32 bar_y = g_splash.height * 2 / 3;
    
    // Draw progress bar background
    splash_draw_rect(bar_x, bar_y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, 0x2a2a4e);
    
    // Draw progress bar fill
    u32 fill_width = (PROGRESS_BAR_WIDTH - 4) * percent / 100;
    splash_draw_gradient(bar_x + 2, bar_y + 2, fill_width, PROGRESS_BAR_HEIGHT - 4,
                         0x16c79a, 0x1fff9f);
    
    // Draw border
    splash_draw_rect(bar_x, bar_y, PROGRESS_BAR_WIDTH, 2, 0x3a3a5e);
    splash_draw_rect(bar_x, bar_y + PROGRESS_BAR_HEIGHT - 2, PROGRESS_BAR_WIDTH, 2, 0x3a3a5e);
    splash_draw_rect(bar_x, bar_y, 2, PROGRESS_BAR_HEIGHT, 0x3a3a5e);
    splash_draw_rect(bar_x + PROGRESS_BAR_WIDTH - 2, bar_y, 2, PROGRESS_BAR_HEIGHT, 0x3a3a5e);
}

/**
 * splash_draw_status - Draw status text
 */
static void splash_draw_status(const char *status)
{
    u32 text_y = g_splash.height * 2 / 3 + PROGRESS_BAR_HEIGHT + 20;
    console_print("\r\033[%d;%dH%s", text_y / 16, 0, status);
}

/*===========================================================================*/
/*                         PUBLIC API                                        */
/*===========================================================================*/

/**
 * splash_init - Initialize splash screen
 */
void splash_init(u32 *fb, u32 width, u32 height, u32 pitch, u32 bpp)
{
    g_splash.framebuffer = fb;
    g_splash.width = width;
    g_splash.height = height;
    g_splash.pitch = pitch;
    g_splash.bpp = bpp;
    g_splash.initialized = true;
    
    // Clear screen to background color
    u32 bg = SPLASH_BG_COLOR;
    for (u32 i = 0; i < width * height; i++) {
        fb[i] = bg;
    }
    
    // Draw logo
    splash_draw_logo();
    
    // Draw title
    console_print("\033[10;30H");
    console_print("  ╔════════════════════════════════════════╗\n");
    console_print("  ║                                        ║\n");
    console_print("  ║     \033[1;36mN E X U S   O S\033[0m                  ║\n");
    console_print("  ║     \033[2;36mGenesis Edition\033[0m                  ║\n");
    console_print("  ║                                        ║\n");
    console_print("  ╚════════════════════════════════════════╝\n");
    
    // Draw progress bar
    splash_draw_progress(0);
    
    console_print("\033[18;35H");
    console_print("Initializing system...");
}

/**
 * splash_update_progress - Update splash progress
 */
void splash_update_progress(u32 percent, const char *status)
{
    if (!g_splash.initialized)
        return;
    
    splash_draw_progress(percent);
    
    if (status) {
        console_print("\033[19;35H\033[K");
        console_print("%s", status);
    }
}

/**
 * splash_show_message - Show a message on splash
 */
void splash_show_message(const char *message)
{
    if (!g_splash.initialized)
        return;
    
    console_print("\033[21;30H\033[33m%s\033[0m", message);
}

/**
 * splash_done - Finish splash screen
 */
void splash_done(void)
{
    if (!g_splash.initialized)
        return;
    
    splash_update_progress(100, "System ready!");
    
    // Small delay to show 100%
    for (volatile u32 i = 0; i < 1000000; i++);
    
    g_splash.initialized = false;
}

/**
 * splash_test - Test splash screen
 */
void splash_test(void)
{
    console_print("\033[2J\033[H");
    console_print("NEXUS OS Splash Screen Test\n");
    console_print("Framebuffer: %ux%u @ %ubpp\n", g_splash.width, g_splash.height, g_splash.bpp);
}
