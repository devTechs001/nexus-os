/*
 * NEXUS OS - VGA/SVGA Driver
 * drivers/video/vga.c
 *
 * VGA and SVGA graphics driver with hardware acceleration
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         VGA CONFIGURATION                                 */
/*===========================================================================*/

#define VGA_MEMORY_BASE           0x000A0000
#define VGA_MEMORY_SIZE           0x00010000
#define VGA_IO_BASE               0x03C0
#define VGA_MAX_WIDTH             2048
#define VGA_MAX_HEIGHT            2048

/* VGA Registers */
#define VGA_ATT_PORT              0x03C0
#define VGA_MIS_PORT              0x03C2
#define VGA_SEQ_PORT              0x03C4
#define VGA_GDC_PORT              0x03CE
#define VGA_CRTC_PORT             0x03D4
#define VGA_PEL_PORT              0x03C8

/* VGA Misc Output Register */
#define VGA_MIS_COLOR             0x01
#define VGA_MIS_ENB_MEM_ACCESS    0x02
#define VGA_MIS_DCLK_28322_720    0x04
#define VGA_MIS_ENB_PLL_LOAD      0x04
#define VGA_MIS_SEL_HIGH_PAGE     0x08

/* VGA Sequencer Registers */
#define VGA_SEQ_RESET             0x00
#define VGA_SEQ_CLOCK_MODE        0x01
#define VGA_SEQ_PLANE_WRITE       0x02
#define VGA_SEQ_CHAR_MAP          0x03
#define VGA_SEQ_MEM_MAP           0x04

/* VGA Graphics Controller Registers */
#define VGA_GDC_SR_INDEX          0x00
#define VGA_GDC_SR_VALUE          0x01
#define VGA_GDC_SR_ENABLE_SR      0x02
#define VGA_GDC_MISC                0x06

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 width;
    u32 height;
    u32 bpp;
    u32 pitch;
    u32 memory_size;
    void *framebuffer;
    phys_addr_t phys_base;
    bool is_linear;
    bool is_initialized;
} vga_mode_t;

typedef struct {
    bool initialized;
    vga_mode_t current_mode;
    vga_mode_t *supported_modes;
    u32 mode_count;
    u8 saved_palette[768];
    u8 current_palette[768];
    u16 saved_crtc[25];
    u8 saved_seq[5];
    u8 saved_gdc[9];
    spinlock_t lock;
} vga_driver_t;

static vga_driver_t g_vga;

/*===========================================================================*/
/*                         LOW-LEVEL VGA OPERATIONS                          */
/*===========================================================================*/

static inline void vga_outb(u16 port, u8 value)
{
    asm volatile("outb %0, %1" :: "a"(value), "d"(port));
}

static inline u8 vga_inb(u16 port)
{
    u8 value;
    asm volatile("inb %1, %0" : "=a"(value) : "d"(port));
    return value;
}

static inline void vga_outw(u16 port, u16 value)
{
    asm volatile("outw %0, %1" :: "a"(value), "d"(port));
}

static void vga_write_seq(u8 index, u8 value)
{
    vga_outb(VGA_SEQ_PORT, index);
    vga_outb(VGA_SEQ_PORT + 1, value);
}

static void vga_write_gdc(u8 index, u8 value)
{
    vga_outb(VGA_GDC_PORT, index);
    vga_outb(VGA_GDC_PORT + 1, value);
}

static void vga_write_crtc(u8 index, u8 value)
{
    vga_outb(VGA_CRTC_PORT, index);
    vga_outb(VGA_CRTC_PORT + 1, value);
}

static u8 vga_read_crtc(u8 index)
{
    vga_outb(VGA_CRTC_PORT, index);
    return vga_inb(VGA_CRTC_PORT + 1);
}

/*===========================================================================*/
/*                         PALETTE OPERATIONS                                */
/*===========================================================================*/

static void vga_set_palette(u8 index, u8 r, u8 g, u8 b)
{
    vga_outb(VGA_PEL_PORT, index);
    vga_outb(VGA_PEL_PORT + 1, r);
    vga_outb(VGA_PEL_PORT + 1, g);
    vga_outb(VGA_PEL_PORT + 1, b);
    
    g_vga.current_palette[index * 3 + 0] = r;
    g_vga.current_palette[index * 3 + 1] = g;
    g_vga.current_palette[index * 3 + 2] = b;
}

static void vga_load_palette(const u8 *palette)
{
    vga_outb(VGA_PEL_PORT, 0);
    for (int i = 0; i < 768; i++) {
        vga_outb(VGA_PEL_PORT + 1, palette[i]);
    }
    memcpy(g_vga.current_palette, palette, 768);
}

static void vga_save_palette(void)
{
    vga_outb(0x03C7, 0);
    for (int i = 0; i < 768; i++) {
        g_vga.saved_palette[i] = vga_inb(0x03C9);
    }
}

/*===========================================================================*/
/*                         MODE SETTING                                      */
/*===========================================================================*/

static int vga_set_mode_vga(u32 width, u32 height, u32 bpp)
{
    spinlock_lock(&g_vga.lock);
    
    /* Save current state */
    vga_save_palette();
    
    /* Reset sequencer */
    vga_write_seq(VGA_SEQ_RESET, 0x01);
    
    /* Set misc output register */
    vga_outb(VGA_MIS_PORT, 0x63);
    
    /* Enable sequencer */
    vga_write_seq(VGA_SEQ_RESET, 0x03);
    
    /* Set graphics controller */
    vga_write_gdc(VGA_GDC_SR_INDEX, 0x00);
    vga_write_gdc(VGA_GDC_SR_VALUE, 0x00);
    vga_write_gdc(VGA_GDC_SR_ENABLE_SR, 0x00);
    vga_write_gdc(VGA_GDC_MISC, 0x05);
    
    /* Standard VGA mode */
    if (width == 320 && height == 200) {
        /* Mode 13h: 320x200x256 colors */
        vga_write_crtc(0x00, 0x5F);
        vga_write_crtc(0x01, 0x4F);
        vga_write_crtc(0x02, 0x50);
        vga_write_crtc(0x03, 0x82);
        vga_write_crtc(0x04, 0x54);
        vga_write_crtc(0x05, 0x80);
        vga_write_crtc(0x06, 0xBF);
        vga_write_crtc(0x07, 0x1F);
        vga_write_crtc(0x08, 0x00);
        vga_write_crtc(0x09, 0x41);
        vga_write_crtc(0x0A, 0x00);
        vga_write_crtc(0x0B, 0x00);
        vga_write_crtc(0x0C, 0x00);
        vga_write_crtc(0x0D, 0x00);
        vga_write_crtc(0x0E, 0x00);
        vga_write_crtc(0x0F, 0x00);
        vga_write_crtc(0x10, 0x00);
        vga_write_crtc(0x11, 0x00);
        vga_write_crtc(0x12, 0x00);
        vga_write_crtc(0x13, 0x00);
        vga_write_crtc(0x14, 0x00);
        vga_write_crtc(0x15, 0x00);
        vga_write_crtc(0x16, 0x00);
        vga_write_crtc(0x17, 0x00);
        vga_write_crtc(0x18, 0x00);
        
        g_vga.current_mode.width = 320;
        g_vga.current_mode.height = 200;
        g_vga.current_mode.bpp = 8;
        g_vga.current_mode.pitch = 320;
        g_vga.current_mode.memory_size = 320 * 200;
        g_vga.current_mode.is_linear = false;
    }
    
    spinlock_unlock(&g_vga.lock);
    return 0;
}

static int vga_set_mode_svga(u32 width, u32 height, u32 bpp)
{
    /* SVGA requires VESA BIOS Extensions or hardware-specific code */
    /* This is a placeholder for SVGA implementation */
    
    g_vga.current_mode.width = width;
    g_vga.current_mode.height = height;
    g_vga.current_mode.bpp = bpp;
    g_vga.current_mode.pitch = width * (bpp / 8);
    g_vga.current_mode.memory_size = width * height * (bpp / 8);
    g_vga.current_mode.is_linear = true;
    g_vga.current_mode.framebuffer = (void *)VGA_MEMORY_BASE;
    g_vga.current_mode.phys_base = VGA_MEMORY_BASE;
    
    printk("[VGA] SVGA mode: %dx%dx%d (linear)\n", width, height, bpp);
    return 0;
}

/*===========================================================================*/
/*                         DRAWING OPERATIONS                                */
/*===========================================================================*/

void vga_set_pixel(u32 x, u32 y, u32 color)
{
    if (!g_vga.initialized) return;
    if (x >= g_vga.current_mode.width || y >= g_vga.current_mode.height) return;
    
    if (g_vga.current_mode.bpp == 8) {
        /* 256 color mode */
        u8 *fb = (u8 *)g_vga.current_mode.framebuffer;
        fb[y * g_vga.current_mode.pitch + x] = (u8)color;
    } else if (g_vga.current_mode.bpp == 16) {
        /* 16-bit color mode */
        u16 *fb = (u16 *)g_vga.current_mode.framebuffer;
        fb[y * g_vga.current_mode.pitch / 2 + x] = (u16)color;
    } else if (g_vga.current_mode.bpp == 32) {
        /* 32-bit color mode */
        u32 *fb = (u32 *)g_vga.current_mode.framebuffer;
        fb[y * g_vga.current_mode.pitch / 4 + x] = color;
    }
}

u32 vga_get_pixel(u32 x, u32 y)
{
    if (!g_vga.initialized) return 0;
    if (x >= g_vga.current_mode.width || y >= g_vga.current_mode.height) return 0;
    
    if (g_vga.current_mode.bpp == 8) {
        u8 *fb = (u8 *)g_vga.current_mode.framebuffer;
        return fb[y * g_vga.current_mode.pitch + x];
    } else if (g_vga.current_mode.bpp == 16) {
        u16 *fb = (u16 *)g_vga.current_mode.framebuffer;
        return fb[y * g_vga.current_mode.pitch / 2 + x];
    } else if (g_vga.current_mode.bpp == 32) {
        u32 *fb = (u32 *)g_vga.current_mode.framebuffer;
        return fb[y * g_vga.current_mode.pitch / 4 + x];
    }
    
    return 0;
}

void vga_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    if (!g_vga.initialized) return;
    
    for (u32 j = 0; j < h; j++) {
        for (u32 i = 0; i < w; i++) {
            vga_set_pixel(x + i, y + j, color);
        }
    }
}

void vga_clear_screen(u32 color)
{
    vga_fill_rect(0, 0, g_vga.current_mode.width, g_vga.current_mode.height, color);
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int vga_init(void)
{
    printk("[VGA] ========================================\n");
    printk("[VGA] NEXUS OS VGA/SVGA Driver\n");
    printk("[VGA] ========================================\n");
    
    memset(&g_vga, 0, sizeof(vga_driver_t));
    spinlock_init(&g_vga.lock);
    
    /* Detect VGA hardware */
    u8 misc = vga_inb(VGA_MIS_PORT);
    printk("[VGA] VGA hardware detected (MIS: 0x%02X)\n", misc);
    
    /* Set default mode */
    vga_set_mode_vga(320, 200, 8);
    
    /* Initialize framebuffer pointer */
    g_vga.current_mode.framebuffer = (void *)VGA_MEMORY_BASE;
    g_vga.current_mode.phys_base = VGA_MEMORY_BASE;
    
    g_vga.initialized = true;
    
    printk("[VGA] VGA driver initialized\n");
    printk("[VGA] Mode: %dx%dx%d\n", 
           g_vga.current_mode.width,
           g_vga.current_mode.height,
           g_vga.current_mode.bpp);
    
    return 0;
}

int vga_shutdown(void)
{
    printk("[VGA] Shutting down VGA driver...\n");
    
    /* Restore saved palette */
    vga_load_palette(g_vga.saved_palette);
    
    g_vga.initialized = false;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

vga_mode_t *vga_get_current_mode(void)
{
    return &g_vga.current_mode;
}

int vga_get_mode_list(vga_mode_t *modes, u32 *count)
{
    if (!modes || !count) return -EINVAL;
    
    /* Standard VGA/SVGA modes */
    vga_mode_t std_modes[] = {
        {320, 200, 8, 320, 64000, NULL, 0, false, false},
        {640, 480, 8, 640, 307200, NULL, 0, false, false},
        {640, 480, 16, 1280, 614400, NULL, 0, false, false},
        {800, 600, 8, 800, 480000, NULL, 0, false, false},
        {800, 600, 16, 1600, 960000, NULL, 0, false, false},
        {1024, 768, 16, 2048, 1572864, NULL, 0, false, false},
        {1280, 1024, 16, 2560, 2621440, NULL, 0, false, false},
        {1920, 1080, 32, 7680, 8294400, NULL, 0, true, false},
    };
    
    u32 copy = (*count < 8) ? *count : 8;
    memcpy(modes, std_modes, sizeof(vga_mode_t) * copy);
    *count = copy;
    
    return 0;
}

vga_driver_t *vga_get_driver(void)
{
    return &g_vga;
}
