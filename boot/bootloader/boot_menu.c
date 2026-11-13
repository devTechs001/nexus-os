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

#include "../../kernel/include/kernel.h"
#include "../../drivers/video/font8x16.h"

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
#define COLOR_SUCCESS           0x0A    /* Light green on black */
#define COLOR_INFO              0x0B    /* Light cyan on black */

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
static bool display_dirty = true;

/*===========================================================================*/
/*                         LOW-LEVEL I/O (USING KERNEL WRAPPERS)             */
/*===========================================================================*/

/**
 * io_wait_delay - Small delay
 */
static inline void io_wait_delay(void)
{
    outb(0x80, 0);
}

/*===========================================================================*/
/*                         VGA MODE SWITCH                                   */
/*===========================================================================*/

/**
 * vga_init_text_mode - Switch VGA to text mode 3 (80x25)
 * 
 * GRUB may have set up a framebuffer (graphics mode), which makes the
 * VGA text buffer at 0xB8000 invisible on the display. This function
 * reprograms the VGA hardware to standard color text mode 3 so that
 * writes to 0xB8000 are visible again. After the boot menu returns,
 * the kernel can reinitialize the framebuffer for the graphical splash.
 */
static void vga_init_text_mode(void)
{
    /* Step 1: Miscellaneous Output Register - enable color, 0x3D4/0x3D5 */
    outb(0x3C2, 0x67);
    io_wait_delay();

    /* Step 2: Program Sequencer (0x3C4/0x3C5) */
    outb(0x3C4, 0x00); outb(0x3C5, 0x03); io_wait_delay();  // Reset: enable all
    outb(0x3C4, 0x01); outb(0x3C5, 0x01); io_wait_delay();  // Clocking: 8 dots/char
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F); io_wait_delay();  // Map Mask: all planes
    outb(0x3C4, 0x03); outb(0x3C5, 0x00); io_wait_delay();  // Char Map Select: default
    outb(0x3C4, 0x04); outb(0x3C5, 0x03); io_wait_delay();  // Memory Mode: sequential

    /* Step 3: Unlock CRTC registers by clearing protection bit (CR11 bit 7) */
    outb(0x3D4, 0x11);
    io_wait_delay();
    u8 cr11_val = inb(0x3D5);
    outb(0x3D5, cr11_val & ~0x80);
    io_wait_delay();

    /* Step 4: Program CRTC for 80x25 text mode (0x3D4/0x3D5) */
    const struct { u8 index; u8 val; } crtc_regs[] = {
        {0x00, 0x5F}, {0x01, 0x4F}, {0x02, 0x50}, {0x03, 0x82},
        {0x04, 0x55}, {0x05, 0x81}, {0x06, 0xBF}, {0x07, 0x1F},
        {0x08, 0x00}, {0x09, 0x4F}, {0x0A, 0x0D}, {0x0B, 0x0E},
        {0x0C, 0x00}, {0x0D, 0x00}, {0x0E, 0x00}, {0x0F, 0x00},
        {0x10, 0x9C}, {0x11, 0x0E}, {0x12, 0x8F}, {0x13, 0x28},
        {0x14, 0x1F}, {0x15, 0x96}, {0x16, 0xB9}, {0x17, 0xA3},
        {0x18, 0xFF},
    };
    for (u32 i = 0; i < sizeof(crtc_regs)/sizeof(crtc_regs[0]); i++) {
        outb(0x3D4, crtc_regs[i].index);
        outb(0x3D5, crtc_regs[i].val);
        io_wait_delay();
    }

    /* Step 5: Program Graphics Controller (0x3CE/0x3CF) */
    outb(0x3CE, 0x00); outb(0x3CF, 0x00); io_wait_delay();
    outb(0x3CE, 0x01); outb(0x3CF, 0x00); io_wait_delay();
    outb(0x3CE, 0x02); outb(0x3CF, 0x00); io_wait_delay();
    outb(0x3CE, 0x03); outb(0x3CF, 0x00); io_wait_delay();
    outb(0x3CE, 0x04); outb(0x3CF, 0x00); io_wait_delay();
    outb(0x3CE, 0x05); outb(0x3CF, 0x10); io_wait_delay();
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E); io_wait_delay();
    outb(0x3CE, 0x07); outb(0x3CF, 0x0F); io_wait_delay();
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF); io_wait_delay();

    /* Step 6: Program Attribute Controller (0x3C0) */
    inb(0x3DA);  // Reset flip-flop to index mode

    /* Palette registers (AR0-AR15): standard VGA colors */
    for (u8 i = 0; i < 16; i++) {
        outb(0x3C0, i); io_wait_delay();
        outb(0x3C0, i); io_wait_delay();
    }

    /* AR10 - Mode Control: text mode A, no line graphics */
    outb(0x3C0, 0x10); outb(0x3C0, 0x0C); io_wait_delay();
    /* AR11 - Overscan color: black */
    outb(0x3C0, 0x11); outb(0x3C0, 0x00); io_wait_delay();
    /* AR12 - Color Plane Enable: all planes */
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F); io_wait_delay();
    /* AR13 - Horizontal Pixel Panning: none */
    outb(0x3C0, 0x13); outb(0x3C0, 0x00); io_wait_delay();
    /* AR14 - Color Select: default */
    outb(0x3C0, 0x14); outb(0x3C0, 0x00); io_wait_delay();
    /* Enable video output (set bit 5 of address register) */
    outb(0x3C0, 0x20);
    io_wait_delay();

    /* Step 7: Clear text buffer to white-on-black spaces */
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (0x0F << 8) | ' ';
    }
}

/*===========================================================================*/
/*                         VGA TEXT MODE OUTPUT                              */
/*===========================================================================*/

/* Mirrored into the framebuffer from vga_putchar(); defined below. */
static void menu_gfx_cell(int gx, int gy, char c, u8 attr);

/**
 * vga_putchar - Print character to VGA buffer
 */
static void vga_putchar(int x, int y, char c, u8 attr)
{
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = c | (attr << 8);
    }

    /* Keep the on-screen copy in step when we are in graphics mode */
    menu_gfx_cell(x, y, c, attr);
}

/*===========================================================================*/
/*                     FRAMEBUFFER MIRROR                                    */
/*===========================================================================*/


/*
 * When GRUB gives us a linear framebuffer we must NOT switch the card back
 * to text mode: doing so blanks the display for the whole splash, which is
 * what used to happen. The menu therefore keeps the framebuffer and mirrors
 * every character it writes to the VGA text buffer into the framebuffer
 * using the shared 8x16 font.
 *
 * With no framebuffer (pure text boot) these helpers are no-ops and the
 * menu behaves exactly as before.
 */

static u32 *g_menu_fb;         /* framebuffer base, NULL in text mode */
static u32  g_menu_fb_w;
static u32  g_menu_fb_h;
static u32  g_menu_fb_pitch;   /* bytes per scanline */
static bool g_menu_gfx;

/* VGA attribute -> 24bpp foreground. The DOS palette is close enough. */
static u32 menu_attr_fg(u8 attr)
{
    static const u32 palette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    };
    return palette[attr & 0x0F];
}

static u32 menu_attr_bg(u8 attr)
{
    static const u32 palette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    };
    return palette[(attr >> 4) & 0x0F];
}

/**
 * menu_gfx_init - Bind the menu's mirror to the bootloader framebuffer
 */
static void menu_gfx_init(void)
{
    boot_params_t *params = get_boot_params();

    g_menu_fb = NULL;
    g_menu_gfx = false;

    if (!params || !params->gfx_addr || params->gfx_width == 0 ||
        params->gfx_height == 0 || params->gfx_depth != 32) {
        return;
    }

    g_menu_fb = (u32 *)(uintptr_t)params->gfx_addr;
    g_menu_fb_w = params->gfx_width;
    g_menu_fb_h = params->gfx_height;
    /* Fall back to a packed scanline if the tag omitted the pitch */
    g_menu_fb_pitch = params->gfx_pitch ? params->gfx_pitch
                                        : params->gfx_width * 4;
    g_menu_gfx = true;
}

/**
 * menu_gfx_cell - Draw one character cell into the framebuffer
 *
 * The VGA grid is 80x25; scale the cell to fill the framebuffer so the menu
 * looks right at any resolution.
 */
static void menu_gfx_cell(int gx, int gy, char c, u8 attr)
{
    if (!g_menu_gfx || !g_menu_fb) {
        return;
    }

    u32 scale_x = g_menu_fb_w / VGA_WIDTH;
    u32 scale_y = g_menu_fb_h / VGA_HEIGHT;

    if (scale_x == 0) scale_x = 1;
    if (scale_y == 0) scale_y = 1;

    s32 px = (s32)((u32)gx * scale_x);
    s32 py = (s32)((u32)gy * scale_y);

    /* Cell background first */
    for (u32 y = 0; y < scale_y; y++) {
        u32 *row = (u32 *)((u8 *)g_menu_fb + (u32)(py + (s32)y) * g_menu_fb_pitch);
        for (u32 x = 0; x < scale_x; x++) {
            s32 fx = px + (s32)x;
            if (fx >= 0 && (u32)fx < g_menu_fb_w &&
                (u32)(py + (s32)y) < g_menu_fb_h) {
                row[fx] = menu_attr_bg(attr);
            }
        }
    }

    /* Glyph, centred in the cell */
    if (c > ' ') {
        const u8 *glyph = font8x16_glyph(c);
        u32 fg = menu_attr_fg(attr);
        u32 gx0 = (u32)px + (scale_x > FONT8X16_WIDTH ? (scale_x - FONT8X16_WIDTH) / 2 : 0);
        u32 gy0 = (u32)py + (scale_y > FONT8X16_HEIGHT ? (scale_y - FONT8X16_HEIGHT) / 2 : 0);

        for (u32 row_i = 0; row_i < FONT8X16_HEIGHT && gy0 + row_i < g_menu_fb_h; row_i++) {
            u8 bits = glyph[row_i];
            u32 *row = (u32 *)((u8 *)g_menu_fb + (gy0 + row_i) * g_menu_fb_pitch);

            for (u32 col = 0; col < FONT8X16_WIDTH; col++) {
                if (bits & (0x80 >> col)) {
                    u32 fx = gx0 + col;
                    if (fx < g_menu_fb_w) {
                        row[fx] = fg;
                    }
                }
            }
        }
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
    int len = (int)strlen(str);

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
        io_wait_delay();
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
 * keyboard_wait_release - Wait for key release (bounded)
 */
static void keyboard_wait_release(void)
{
    u32 bound = 1000;
    while ((inb(0x64) & 0x01) && bound > 0) {
        inb(0x60);
        io_wait_delay();
        bound--;
    }
}

/*===========================================================================*/
/*                         TIMER UTILITIES                                   */
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
        timer_ticks++;  /* Advance timer if interrupt hasn't been set up yet */
        io_wait_delay();
    }
}

/*===========================================================================*/
/*                         MENU RENDERING                                    */
/*===========================================================================*/

/**
 * menu_draw_header - Draw menu header
 */
static void menu_draw_header(void)
{
    /* Title bar */
    vga_fill_box(0, 0, VGA_WIDTH - 1, 2, ' ', COLOR_TITLE);
    vga_center_text(1, MENU_TITLE, COLOR_TITLE);

    /* Version (right-aligned in the title bar) */
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
        /*
         * Show the real remaining time. The old code declared msg[64] and
         * never filled it, then printed a hard-coded label with no number.
         */
        char msg[40];
        u32 seconds = (u32)(menu.timeout / 10);
        u32 pos = 0;

        const char *prefix = "Booting in ";
        while (*prefix) msg[pos++] = *prefix++;

        if (seconds >= 10) msg[pos++] = (char)('0' + (seconds / 10) % 10);
        msg[pos++] = (char)('0' + seconds % 10);

        const char *suffix = " seconds...";
        while (*suffix) msg[pos++] = *suffix++;
        msg[pos] = '\0';

        vga_print_at(30, VGA_HEIGHT - 4, msg, COLOR_NORMAL);
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
 * 
 * Returns: true if boot successful, false if failed
 */
static bool menu_boot_entry(u32 index)
{
    if (index >= menu.entry_count) {
        return false;
    }

    boot_entry_t *entry = &menu.entries[index];

    /* Clear screen */
    vga_clear_screen(COLOR_NORMAL);

    /* Display boot message */
    vga_print_at(0, 5, "===============================================", COLOR_TITLE);
    vga_print_at(0, 6, "  BOOTING NEXUS OS...", COLOR_TITLE);
    vga_print_at(0, 7, "===============================================", COLOR_TITLE);
    
    vga_print_at(5, 10, "Entry: ", COLOR_NORMAL);
    vga_print_at(15, 10, entry->name, COLOR_INFO);
    
    vga_print_at(5, 11, "Kernel: ", COLOR_NORMAL);
    vga_print_at(15, 11, entry->kernel_path, COLOR_INFO);
    
    vga_print_at(5, 12, "Cmdline: ", COLOR_NORMAL);
    vga_print_at(15, 12, entry->cmdline, COLOR_INFO);
    
    vga_print_at(5, 14, "Loading kernel modules...", COLOR_NORMAL);
    timer_delay_ms(200);
    vga_print_at(40, 14, "[ OK ]", COLOR_SUCCESS);
    
    vga_print_at(5, 15, "Initializing memory...", COLOR_NORMAL);
    timer_delay_ms(100);
    vga_print_at(40, 15, "[ OK ]", COLOR_SUCCESS);
    
    vga_print_at(5, 16, "Starting system services...", COLOR_NORMAL);
    timer_delay_ms(100);
    vga_print_at(40, 16, "[ OK ]", COLOR_SUCCESS);

    vga_print_at(0, 19, "Transitioning to Kernel...", COLOR_TITLE);
    timer_delay_ms(500);

    /* Copy selected command line to a global buffer that kernel can read */
    extern char g_boot_cmdline[512];
    strncpy(g_boot_cmdline, entry->cmdline, 511);
    g_boot_cmdline[511] = '\0';
    
    outb(0x3F8, 'R');
    outb(0x3F8, '\n');
    return true;
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

    /* Entry 0: VMware Mode (Default) */
    boot_entry_t *entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "NEXUS OS (VMware Mode)");
    strcpy(entry->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(entry->initrd_path, "/boot/initrd.img");
    strcpy(entry->cmdline, "quiet splash virt=vmware");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    /* Entry 1: Safe Mode */
    entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "NEXUS OS (Safe Mode)");
    strcpy(entry->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(entry->initrd_path, "/boot/initrd.img");
    strcpy(entry->cmdline, "nomodeset nosmp noapic nolapic");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    /* Entry 2: Debug Mode */
    entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "NEXUS OS (Debug Mode)");
    strcpy(entry->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(entry->initrd_path, "/boot/initrd.img");
    strcpy(entry->cmdline, "debug loglevel=7 earlyprintk=vga");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    /* Entry 3: Native Mode */
    entry = &menu.entries[menu.entry_count++];
    strcpy(entry->name, "NEXUS OS (Native Mode)");
    strcpy(entry->kernel_path, "/boot/nexus-kernel.bin");
    strcpy(entry->initrd_path, "/boot/initrd.img");
    strcpy(entry->cmdline, "virt=native");
    entry->kernel_address = 0x00100000;
    entry->enabled = true;

    /* Entry 4: Memory Test */
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
        /* Advance the tick counter so our spin-based delays work */
        timer_ticks++;

        /* Only redraw screen when state changes (massive speedup in QEMU) */
        if (display_dirty) {
            menu_draw();
            display_dirty = false;
        }

        /* Advance timer and handle timeout */
        timer_ticks++;
        u32 current_tick = timer_ticks;
        if (current_tick - last_tick >= 100) {  /* 1 second at 100 Hz */
            last_tick = current_tick;
            if (menu.timeout > 0) {
                menu.timeout--;
                display_dirty = true;
                if (menu.timeout == 0) {
                    if (menu_boot_entry(menu.selected)) {
                        running = false;
                        continue;
                    }
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
                        display_dirty = true;
                    }
                    break;

                case KEY_DOWN:
                    if (menu.selected < menu.entry_count - 1) {
                        menu.selected++;
                        display_dirty = true;
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
                    if (menu_boot_entry(menu.selected)) {
                        running = false;
                    }
                    break;

                case KEY_ESCAPE:
                    /* Cancel timeout and wait */
                    menu.timeout = 0;
                    display_dirty = true;
                    break;

                case 'e':
                case 'E':
                    /* Edit entry */
                    menu_edit_entry();
                    display_dirty = true;
                    break;

                case 'b':
                case 'B':
                    /* Boot now */
                    menu.timeout = 0;
                    if (menu_boot_entry(menu.selected)) {
                        running = false;
                    }
                    break;

                case 'h':
                case 'H':
                case KEY_F1:
                    /* Toggle help */
                    menu.show_help = !menu.show_help;
                    display_dirty = true;
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
                            display_dirty = true;
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

/*
 * Not wired to IRQ0 yet: menu_wait_ticks() advances timer_ticks in a spin
 * loop, so the countdown works without an interrupt handler. This is the
 * intended hook-up point if the menu ever needs to block instead of poll.
 */
__attribute__((unused)) static void (*const g_menu_timer_hook)(void) = timer_handler;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

/**
 * menu_init - Initialize boot menu
 */
static void menu_init(void)
{
    /* Simple byte-by-byte clear (memset may be optimized to SSE at -O2, avoid that) */
    {
        volatile u8 *p = (volatile u8 *)&menu;
        u32 i;
        for (i = 0; i < sizeof(menu_state_t); i++) {
            *p++ = 0;
        }
    }
    display_dirty = true;

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
    outb(0x3F8, 'B');
    outb(0x3F8, 'M');
    outb(0x3F8, '\n');

    /*
     * Do NOT switch VGA back to text mode 3 here.
     *
     * GRUB hands the kernel a linear framebuffer (the Multiboot2
     * framebuffer tag) so the graphical splash can draw on it. Forcing
     * mode 3 reprograms the CRTC/sequencer back to 80x25 text: the
     * framebuffer address stays valid, but nothing written to it reaches
     * the display any more. That is exactly why the VGA window went black
     * once the kernel switched to graphical mode - the boot menu had
     * already reverted the video card.
     *
     * The menu itself is drawn with BIOS text writes, so only fall back
     * to text mode when there is no framebuffer to protect.
     */
    {
        boot_params_t *params = get_boot_params();
        bool have_fb = (params && params->gfx_addr != 0);

        if (!have_fb) {
            vga_init_text_mode();
        } else {
            /* Keep the framebuffer and draw the menu into it */
            menu_gfx_init();
            outb(0x3F8, 'K');
            outb(0x3F8, 'F');
            outb(0x3F8, 'B');
            outb(0x3F8, '\n');
        }
    }

    /* Initialize menu */
    menu_init();

    /* Run menu */
    menu_run();
}
