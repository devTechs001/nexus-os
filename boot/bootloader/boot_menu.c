/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * boot_menu.c - Boot Menu System
 *
 * This module implements an interactive boot menu for NEXUS OS.
 * It allows users to select boot options, edit kernel parameters,
 * and access system utilities.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*===========================================================================*/
/*                         BOOT MENU CONFIGURATION                           */
/*===========================================================================*/

#define MENU_TITLE              "NEXUS OS Boot Menu"
#define MENU_VERSION            "1.0.0"
#define MENU_TIMEOUT_SECONDS    10
#define MENU_MAX_ENTRIES        16
#define MENU_MAX_CMDLINE        512

/* Colors */
#define COLOR_NORMAL            0x07    /* Light gray on black */
#define COLOR_SELECTED          0x70    /* Black on light gray */
#define COLOR_TITLE             0x1F    /* White on blue */
#define COLOR_HELP              0x08    /* Dark gray on black */
#define COLOR_ERROR             0x4F    /* White on red */

/* VGA */
#define VGA_BASE                0xB8000
#define VGA_WIDTH               80
#define VGA_HEIGHT              25

/* Key codes */
#define KEY_UP                  0x48
#define KEY_DOWN                0x50
#define KEY_LEFT                0x4B
#define KEY_RIGHT               0x4D
#define KEY_ENTER               0x1C
#define KEY_ESCAPE              0x01
#define KEY_SPACE               0x39
#define KEY_BACKSPACE           0x0E
#define KEY_TAB                 0x0F
#define KEY_F1                  0x3B
#define KEY_F2                  0x3C
#define KEY_F10                 0x44

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* Boot entry */
typedef struct {
    char name[64];
    char kernel_path[128];
    char initrd_path[128];
    char cmdline[MENU_MAX_CMDLINE];
    u32 kernel_address;
    bool enabled;
} boot_entry_t;

/* Menu state */
typedef struct {
    boot_entry_t entries[MENU_MAX_ENTRIES];
    u32 entry_count;
    u32 selected;
    u32 timeout;
    bool editing;
    bool show_help;
    char edit_buffer[MENU_MAX_CMDLINE];
    u32 edit_cursor;
} menu_state_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static menu_state_t menu;
static u16 *vga_buffer = (u16 *)VGA_BASE;

/*===========================================================================*/
/*                         LOW-LEVEL I/O                                     */
/*===========================================================================*/

/**
 * outb - Write byte to port
 */
static inline void outb(u16 port, u8 value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * inb - Read byte from port
 */
static inline u8 inb(u16 port)
{
    u8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * io_wait - Small delay
 */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

/*===========================================================================*/
/*                         VGA TEXT MODE OUTPUT                              */
/*===========================================================================*/

/**
 * vga_putchar - Print character to VGA buffer
 */
static void vga_putchar(int x, int y, char c, u8 attr)
{
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = c | (attr << 8);
    }
}

/**
 * vga_print_at - Print string at position
 */
static void vga_print_at(int x, int y, const char *str, u8 attr)
{
    while (*str && x < VGA_WIDTH) {
        vga_putchar(x++, y, *str++, attr);
    }
}

/**
 * vga_clear_line - Clear a line
 */
static void vga_clear_line(int y, u8 attr)
{
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_putchar(x, y, ' ', attr);
    }
}

/**
 * vga_clear_screen - Clear entire screen
 */
static void vga_clear_screen(u8 attr)
{
    for (int y = 0; y < VGA_HEIGHT; y++) {
        vga_clear_line(y, attr);
    }
}

/**
 * vga_fill_box - Fill rectangular area
 */
static void vga_fill_box(int x1, int y1, int x2, int y2, char c, u8 attr)
{
    for (int y = y1; y <= y2 && y < VGA_HEIGHT; y++) {
        for (int x = x1; x <= x2 && x < VGA_WIDTH; x++) {
            vga_putchar(x, y, c, attr);
        }
    }
}

/**
 * vga_draw_box - Draw box with borders
 */
static void vga_draw_box(int x1, int y1, int x2, int y2, u8 attr)
{
    /* Corners */
    vga_putchar(x1, y1, '+', attr);
    vga_putchar(x2, y1, '+', attr);
    vga_putchar(x1, y2, '+', attr);
    vga_putchar(x2, y2, '+', attr);

    /* Horizontal lines */
    for (int x = x1 + 1; x < x2; x++) {
        vga_putchar(x, y1, '-', attr);
        vga_putchar(x, y2, '-', attr);
    }

    /* Vertical lines */
    for (int y = y1 + 1; y < y2; y++) {
        vga_putchar(x1, y, '|', attr);
        vga_putchar(x2, y, '|', attr);
    }

    /* Fill interior */
    vga_fill_box(x1 + 1, y1 + 1, x2 - 1, y2 - 1, ' ', attr);
}

/**
 * vga_center_text - Center text horizontally
 */
static void vga_center_text(int y, const char *str, u8 attr)
{
    int len = 0;
    const char *p = str;
    while (*p++) len++;

    int x = (VGA_WIDTH - len) / 2;
    if (x < 0) x = 0;

    vga_print_at(x, y, str, attr);
}

/*===========================================================================*/
/*                         KEYBOARD INPUT                                    */
/*===========================================================================*/

/**
 * keyboard_read - Read keyboard scancode
 */
static u8 keyboard_read(void)
{
    u8 status = inb(0x64);

    if (!(status & 0x01)) {
        return 0;
    }

    return inb(0x60);
}

/**
 * keyboard_wait - Wait for keyboard data
 */
static void keyboard_wait(void)
{
    while (!(inb(0x64) & 0x01)) {
        io_wait();
    }
}

/**
 * keyboard_getchar - Get character from keyboard (blocking)
 */
static u8 keyboard_getchar(void)
{
    keyboard_wait();
    return keyboard_read();
}

/**
 * keyboard_poll - Poll keyboard (non-blocking)
 */
static u8 keyboard_poll(void)
{
    u8 status = inb(0x64);
    if (status & 0x01) {
        return inb(0x60);
    }
    return 0;
}

/**
 * keyboard_wait_release - Wait for key release
 */
static void keyboard_wait_release(void)
{
    while (inb(0x64) & 0x01) {
        inb(0x60);
        io_wait();
    }
}

/*===========================================================================*/
/*                         TIMER FUNCTIONS                                   */
/*===========================================================================*/

static volatile u32 timer_ticks = 0;

/**
 * timer_get_ticks - Get current timer value
 */
static u32 timer_get_ticks(void)
{
    return timer_ticks;
}

/**
 * timer_delay_ms - Delay for milliseconds
 */
static void timer_delay_ms(u32 ms)
{
    u32 start = timer_get_ticks();
    u32 target = start + (ms / 10);  /* Assuming 100 Hz timer */

    while (timer_get_ticks() < target) {
        io_wait();
    }
}

/*===========================================================================*/
/*                         STRING UTILITIES                                  */
/*===========================================================================*/

/**
 * strlen - Get string length
 */
static u32 strlen(const char *s)
{
    u32 len = 0;
    while (*s++) len++;
    return len;
}

/**
 * strcpy - Copy string
 */
static char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/**
 * strncpy - Copy string with length limit
 */
static char *strncpy(char *dest, const char *src, u32 n)
{
    char *d = dest;
    while (n-- && (*d++ = *src++));
    return dest;
}

/**
 * strcmp - Compare strings
 */
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(u8 *)s1 - *(u8 *)s2;
}

/**
 * memset - Set memory
 */
static void *memset(void *s, int c, u32 n)
{
    u8 *p = (u8 *)s;
    while (n--) *p++ = (u8)c;
    return s;
}

/**
 * memcpy - Copy memory
 */
static void *memcpy(void *dest, const void *src, u32 n)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dest;
}

/*===========================================================================*/
/*                         BOOT MENU DISPLAY                                 */
/*===========================================================================*/

/**
 * menu_draw_header - Draw menu header
 */
static void menu_draw_header(void)
{
    /* Title bar */
    vga_fill_box(0, 0, VGA_WIDTH - 1, 2, ' ', COLOR_TITLE);
    vga_center_text(1, MENU_TITLE, COLOR_TITLE);

    /* Version */
    char version_str[32];
    version_str[0] = 'v';
    version_str[1] = '\0';
    /* Simple version append */
    vga_print_at(VGA_WIDTH - 10, 1, MENU_VERSION, COLOR_TITLE);
}

/**
 * menu_draw_footer - Draw menu footer with help
 */
static void menu_draw_footer(void)
{
    vga_fill_box(0, VGA_HEIGHT - 2, VGA_WIDTH - 1, VGA_HEIGHT - 1, ' ', COLOR_HELP);

    vga_print_at(2, VGA_HEIGHT - 2, "Arrow Keys: Navigate", COLOR_HELP);
    vga_print_at(30, VGA_HEIGHT - 2, "|", COLOR_HELP);
    vga_print_at(33, VGA_HEIGHT - 2, "Enter: Boot", COLOR_HELP);
    vga_print_at(50, VGA_HEIGHT - 2, "|", COLOR_HELP);
    vga_print_at(53, VGA_HEIGHT - 2, "E: Edit", COLOR_HELP);
    vga_print_at(68, VGA_HEIGHT - 2, "|", COLOR_HELP);
    vga_print_at(71, VGA_HEIGHT - 2, "Esc: Timeout", COLOR_HELP);
}

/**
 * menu_draw_entries - Draw boot entries
 */
static void menu_draw_entries(void)
{
    int start_y = 4;
    int entries_per_page = VGA_HEIGHT - 6;

    for (u32 i = 0; i < menu.entry_count && i < (u32)entries_per_page; i++) {
        u8 attr = (i == menu.selected) ? COLOR_SELECTED : COLOR_NORMAL;

        /* Draw entry */
        char line[80];
        line[0] = ' ';
        line[1] = ' ';

        if (i == menu.selected) {
            line[0] = '>';
            line[1] = ' ';
        } else {
            line[0] = ' ';
            line[1] = ' ';
        }

        /* Entry name */
        u32 name_len = strlen(menu.entries[i].name);
        if (name_len > 50) name_len = 50;

        for (u32 j = 0; j < name_len; j++) {
            line[2 + j] = menu.entries[i].name[j];
        }
        line[2 + name_len] = '\0';

        vga_print_at(2, start_y + i, line, attr);
        vga_clear_line(start_y + i, attr);
        vga_print_at(2, start_y + i, line, attr);
    }
}

/**
 * menu_draw_timeout - Draw timeout countdown
 */
static void menu_draw_timeout(void)
{
    if (menu.timeout > 0) {
        char msg[64];
        /* Simple message */
        vga_print_at(30, VGA_HEIGHT - 4, "Booting in ", COLOR_NORMAL);
        /* Would print number here */
        vga_print_at(45, VGA_HEIGHT - 4, " seconds...", COLOR_NORMAL);
    }
}

/**
 * menu_draw - Draw complete menu
 */
static void menu_draw(void)
{
    vga_clear_screen(COLOR_NORMAL);
    menu_draw_header();
    menu_draw_entries();
    menu_draw_timeout();
    menu_draw_footer();
}

/*===========================================================================*/
/*                         EDIT MODE                                         */
/*===========================================================================*/

/**
 * menu_draw_edit - Draw edit screen
 */
static void menu_draw_edit(void)
{
    vga_clear_screen(COLOR_NORMAL);

    /* Draw edit box */
    vga_draw_box(5, 5, 74, 18, COLOR_NORMAL);

    /* Title */
    vga_print_at(30, 6, "Edit Boot Parameters", COLOR_SELECTED);

    /* Entry name */
    vga_print_at(8, 8, "Entry:", COLOR_NORMAL);
    vga_print_at(8, 9, menu.entries[menu.selected].name, COLOR_NORMAL);

    /* Current cmdline */
    vga_print_at(8, 11, "Command Line:", COLOR_NORMAL);
    vga_print_at(8, 13, menu.edit_buffer, COLOR_NORMAL);

    /* Cursor */
    vga_putchar(8 + menu.edit_cursor, 13, '_', COLOR_SELECTED);

    /* Help */
    vga_print_at(10, 16, "Enter: Accept  Esc: Cancel  Backspace: Delete", COLOR_HELP);
}

/**
 * menu_edit_entry - Edit current entry's command line
 */
static bool menu_edit_entry(void)
{
    /* Copy current cmdline to edit buffer */
    strncpy(menu.edit_buffer, menu.entries[menu.selected].cmdline, MENU_MAX_CMDLINE - 1);
    menu.edit_buffer[MENU_MAX_CMDLINE - 1] = '\0';
    menu.edit_cursor = strlen(menu.edit_buffer);

    while (true) {
        menu_draw_edit();

        u8 key = keyboard_getchar();

        /* Handle extended keys */
        if (key == 0xE0) {
            key = keyboard_getchar();

            switch (key) {
                case KEY_LEFT:
                    if (menu.edit_cursor > 0) {
                        menu.edit_cursor--;
                    }
                    break;

                case KEY_RIGHT:
                    if (menu.edit_cursor < strlen(menu.edit_buffer)) {
                        menu.edit_cursor++;
                    }
                    break;

                case KEY_UP:
                case KEY_DOWN:
                    /* Navigate history (not implemented) */
                    break;
            }
        } else {
            switch (key) {
                case KEY_ENTER:
                    /* Accept changes */
                    strncpy(menu.entries[menu.selected].cmdline,
                            menu.edit_buffer, MENU_MAX_CMDLINE - 1);
                    return true;

                case KEY_ESCAPE:
                    /* Cancel */
                    return false;

                case KEY_BACKSPACE:
                    if (menu.edit_cursor > 0) {
                        menu.edit_cursor--;
                        for (u32 i = menu.edit_cursor;
                             i < strlen(menu.edit_buffer); i++) {
                            menu.edit_buffer[i] = menu.edit_buffer[i + 1];
                        }
                    }
                    break;

                default:
                    /* Printable character */
                    if (key >= 0x20 && key < 0x7F) {
                        u32 len = strlen(menu.edit_buffer);
                        if (len < MENU_MAX_CMDLINE - 1) {
                            /* Insert character */
                            for (u32 i = len; i > menu.edit_cursor; i--) {
                                menu.edit_buffer[i] = menu.edit_buffer[i - 1];
                            }
                            menu.edit_buffer[menu.edit_cursor] = key;
                            menu.edit_cursor++;
                        }
                    }
                    break;
            }
        }
    }
}

/*===========================================================================*/
/*                         BOOT CONFIGURATION                                */
/*===========================================================================*/

/**
 * menu_boot_entry - Boot selected entry
 */
static void menu_boot_entry(u32 index)
{
    if (index >= menu.entry_count) {
        return;
    }

    boot_entry_t *entry = &menu.entries[index];

    /* Clear screen */
    vga_clear_screen(COLOR_NORMAL);

    /* Display boot message */
    vga_print_at(0, 10, "Booting NEXUS OS...", COLOR_NORMAL);
    vga_print_at(0, 12, entry->name, COLOR_NORMAL);
    vga_print_at(0, 14, "Kernel: ", COLOR_NORMAL);
    vga_print_at(8, 14, entry->kernel_path, COLOR_NORMAL);
    vga_print_at(0, 16, "Command line: ", COLOR_NORMAL);
    vga_print_at(14, 16, entry->cmdline, COLOR_NORMAL);

    /* Small delay */
    timer_delay_ms(1000);

    /* In real implementation, would load and jump to kernel here */
    /* For now, just display message */
    vga_print_at(0, 20, "Press any key to return to menu...", COLOR_HELP);
    keyboard_getchar();
}

/*===========================================================================*/
/*                         DEFAULT BOOT ENTRIES                              */
/*===========================================================================*/

/**
 * menu_init_default_entries - Initialize default boot entries
 */
static void menu_init_default_entries(void)
{
    menu.entry_count = 0;

    /* Entry 0: Normal boot */
    boot_entry_t *entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "NEXUS OS (Normal)");
    strcpy(entry->kernel_path, "/boot/nexus.elf");
    strcpy(entry->initrd_path, "/boot/initrd.img");
    strcpy(entry->cmdline, "root=/dev/sda2 quiet splash loglevel=3");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    /* Entry 1: Safe mode */
    entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "NEXUS OS (Safe Mode)");
    strcpy(entry->kernel_path, "/boot/nexus.elf");
    strcpy(entry->initrd_path, "/boot/initrd.img");
    strcpy(entry->cmdline, "root=/dev/sda2 nomodeset single");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    /* Entry 2: Debug mode */
    entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "NEXUS OS (Debug)");
    strcpy(entry->kernel_path, "/boot/nexus.elf");
    strcpy(entry->initrd_path, "/boot/initrd.img");
    strcpy(entry->cmdline, "root=/dev/sda2 debug loglevel=7 earlyprintk");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    /* Entry 3: Memory test */
    entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "Memory Test");
    strcpy(entry->kernel_path, "/boot/memtest.bin");
    strcpy(entry->initrd_path, "");
    strcpy(entry->cmdline, "");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    menu.selected = 0;
}

/*===========================================================================*/
/*                         MAIN MENU LOOP                                    */
/*===========================================================================*/

/**
 * menu_run - Run boot menu
 */
static void menu_run(void)
{
    bool running = true;
    u32 last_tick = timer_get_ticks();

    while (running) {
        /* Draw menu */
        menu_draw();

        /* Handle timeout */
        u32 current_tick = timer_get_ticks();
        if (current_tick - last_tick >= 100) {  /* 1 second at 100 Hz */
            last_tick = current_tick;
            if (menu.timeout > 0) {
                menu.timeout--;
                if (menu.timeout == 0) {
                    menu_boot_entry(menu.selected);
                }
            }
        }

        /* Handle keyboard input */
        u8 key = keyboard_poll();

        if (key == 0) {
            continue;
        }

        /* Handle extended keys */
        if (key == 0xE0) {
            key = keyboard_getchar();

            switch (key) {
                case KEY_UP:
                    if (menu.selected > 0) {
                        menu.selected--;
                    }
                    break;

                case KEY_DOWN:
                    if (menu.selected < menu.entry_count - 1) {
                        menu.selected++;
                    }
                    break;

                case KEY_LEFT:
                case KEY_RIGHT:
                    /* Could be used for other navigation */
                    break;
            }
        } else {
            switch (key) {
                case KEY_ENTER:
                    /* Boot selected entry */
                    menu_boot_entry(menu.selected);
                    break;

                case KEY_ESCAPE:
                    /* Cancel timeout and wait */
                    menu.timeout = 0;
                    break;

                case 'e':
                case 'E':
                    /* Edit entry */
                    menu_edit_entry();
                    break;

                case 'b':
                case 'B':
                    /* Boot now */
                    menu.timeout = 0;
                    menu_boot_entry(menu.selected);
                    break;

                case 'h':
                case 'H':
                case KEY_F1:
                    /* Toggle help */
                    menu.show_help = !menu.show_help;
                    break;

                case KEY_F10:
                    /* System options (not implemented) */
                    break;

                default:
                    /* Number keys for quick select */
                    if (key >= '1' && key <= '9') {
                        u32 idx = key - '1';
                        if (idx < menu.entry_count) {
                            menu.selected = idx;
                        }
                    }
                    break;
            }
        }

        /* Wait for key release */
        keyboard_wait_release();
    }
}

/*===========================================================================*/
/*                         TIMER INTERRUPT HANDLER                           */
/*===========================================================================*/

/**
 * timer_handler - Timer interrupt handler
 */
static void timer_handler(void)
{
    timer_ticks++;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/**
 * menu_init - Initialize boot menu
 */
static void menu_init(void)
{
    /* Clear state */
    memset(&menu, 0, sizeof(menu_state_t));

    /* Initialize default entries */
    menu_init_default_entries();

    /* Set timeout */
    menu.timeout = MENU_TIMEOUT_SECONDS * 10;  /* Convert to ticks */

    /* Reset timer */
    timer_ticks = 0;
}

/*===========================================================================*/
/*                         ENTRY POINT                                       */
/*===========================================================================*/

/**
 * boot_menu_main - Boot menu entry point
 */
void boot_menu_main(void)
{
    /* Initialize menu */
    menu_init();

    /* Run menu */
    menu_run();
}
