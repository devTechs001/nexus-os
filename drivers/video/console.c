/*
 * NEXUS OS - Text Mode Console Driver
 * drivers/video/console.c
 *
 * Simple VGA text mode console for boot messages
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"
#include <stdarg.h>

/*===========================================================================*/
/*                         CONSOLE CONFIGURATION                             */
/*===========================================================================*/

#define VGA_TEXT_MEMORY         0xB8000
#define VGA_TEXT_WIDTH          80
#define VGA_TEXT_HEIGHT         25
#define VGA_TEXT_CELLS          (VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT)

/* Color attributes */
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW        14
#define VGA_COLOR_WHITE         15

/*===========================================================================*/
/*                         CONSOLE STATE                                     */
/*===========================================================================*/

typedef struct {
    u16 *buffer;
    u32 cursor_x;
    u32 cursor_y;
    u8 color;
    bool initialized;
} console_state_t;

static console_state_t g_console;

/*===========================================================================*/
/*                         CONSOLE FUNCTIONS                                 */
/*===========================================================================*/

/**
 * console_putchar - Write a character to console
 */
static void console_putchar(char c)
{
    u16 *cell;
    u16 ch_attr;

    if (!g_console.initialized)
        return;

    ch_attr = (u16)g_console.color << 8 | (u16)c;

    switch (c) {
    case '\n':
        g_console.cursor_x = 0;
        g_console.cursor_y++;
        break;

    case '\r':
        g_console.cursor_x = 0;
        break;

    case '\t':
        g_console.cursor_x = (g_console.cursor_x + 8) & ~7;
        if (g_console.cursor_x >= VGA_TEXT_WIDTH) {
            g_console.cursor_x = 0;
            g_console.cursor_y++;
        }
        break;

    case '\b':
        if (g_console.cursor_x > 0) {
            g_console.cursor_x--;
            cell = g_console.buffer + g_console.cursor_y * VGA_TEXT_WIDTH + g_console.cursor_x;
            *cell = (u16)g_console.color << 8 | ' ';
        }
        break;

    default:
        if (g_console.cursor_x >= VGA_TEXT_WIDTH) {
            g_console.cursor_x = 0;
            g_console.cursor_y++;
        }

        if (g_console.cursor_y >= VGA_TEXT_HEIGHT) {
            /* Scroll up */
            for (u32 i = 0; i < (VGA_TEXT_HEIGHT - 1) * VGA_TEXT_WIDTH; i++) {
                g_console.buffer[i] = g_console.buffer[i + VGA_TEXT_WIDTH];
            }
            for (u32 i = (VGA_TEXT_HEIGHT - 1) * VGA_TEXT_WIDTH; i < VGA_TEXT_CELLS; i++) {
                g_console.buffer[i] = (u16)g_console.color << 8 | ' ';
            }
            g_console.cursor_y = VGA_TEXT_HEIGHT - 1;
        }

        cell = g_console.buffer + g_console.cursor_y * VGA_TEXT_WIDTH + g_console.cursor_x;
        *cell = ch_attr;
        g_console.cursor_x++;
        break;
    }
}

/**
 * console_write - Write a string to console
 */
void console_write(const char *str, u32 len)
{
    if (!g_console.initialized)
        return;

    for (u32 i = 0; i < len && str[i]; i++) {
        console_putchar(str[i]);
    }

    /* Update cursor */
    u32 pos = g_console.cursor_y * VGA_TEXT_WIDTH + g_console.cursor_x;
    outb(0x0E, 0x3D4);
    outb((pos >> 8) & 0xFF, 0x3D5);
    outb(0x0F, 0x3D4);
    outb(pos & 0xFF, 0x3D5);
}

/**
 * console_print - Print a formatted string to console
 */
void console_print(const char *fmt, ...)
{
    va_list args;
    char buffer[256];
    int len;

    if (!g_console.initialized)
        return;

    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    console_write(buffer, len);
}

/**
 * console_clear - Clear the console screen
 */
void console_clear(void)
{
    if (!g_console.initialized)
        return;

    for (u32 i = 0; i < VGA_TEXT_CELLS; i++) {
        g_console.buffer[i] = (u16)g_console.color << 8 | ' ';
    }

    g_console.cursor_x = 0;
    g_console.cursor_y = 0;

    /* Update cursor */
    outb(0x0E, 0x3D4);
    outb(0, 0x3D5);
    outb(0x0F, 0x3D4);
    outb(0, 0x3D5);
}

/**
 * console_set_color - Set console text color
 */
void console_set_color(u8 fg, u8 bg)
{
    g_console.color = (bg << 4) | (fg & 0x0F);
}

/**
 * console_init - Initialize text mode console
 */
void console_init(void)
{
    g_console.buffer = (u16 *)(uintptr_t)VGA_TEXT_MEMORY;
    g_console.cursor_x = 0;
    g_console.cursor_y = 0;
    g_console.color = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREY;
    g_console.initialized = true;

    console_clear();
    console_print("NEXUS OS Console Initialized\n");
}

/**
 * console_scroll - Scroll console up by one line
 */
void console_scroll(void)
{
    if (!g_console.initialized)
        return;

    for (u32 i = 0; i < (VGA_TEXT_HEIGHT - 1) * VGA_TEXT_WIDTH; i++) {
        g_console.buffer[i] = g_console.buffer[i + VGA_TEXT_WIDTH];
    }
    for (u32 i = (VGA_TEXT_HEIGHT - 1) * VGA_TEXT_WIDTH; i < VGA_TEXT_CELLS; i++) {
        g_console.buffer[i] = (u16)g_console.color << 8 | ' ';
    }

    if (g_console.cursor_y > 0)
        g_console.cursor_y--;
}
