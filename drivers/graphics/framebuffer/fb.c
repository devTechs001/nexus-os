/*
 * NEXUS OS - Framebuffer Driver
 * drivers/graphics/framebuffer/fb.c
 *
 * Generic framebuffer driver with acceleration support
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../../kernel/include/types.h"
#include "../../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         FRAMEBUFFER CONFIGURATION                         */
/*===========================================================================*/

#define FB_MAX_DEVICES            8
#define FB_MAX_MODES              128
#define FB_MAX_CMAP               256

/*===========================================================================*/
/*                         FRAMEBUFFER FLAGS                                 */
/*===========================================================================*/

#define FB_FLAG_MEMORY_MAPPED     0x01
#define FB_FLAG_HAS_ACCEL         0x02
#define FB_FLAG_IS_COLOR          0x04
#define FB_FLAG_IS_VGA_PALETTE    0x08
#define FB_FLAG_GLOBAL_ALPHA      0x10
#define FB_FLAG_LOCAL_ALPHA       0x20

/* FB Acceleration Types */
#define FB_HAS_ACCEL              1
#define FB_NO_ACCEL               0

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 xres;                     /* Visible resolution */
    u32 yres;
    u32 xres_virtual;             /* Virtual resolution */
    u32 yres_virtual;
    u32 xoffset;                  /* Offset from virtual to visible */
    u32 yoffset;
    u32 bits_per_pixel;
    u32 grayscale;
} fb_var_screeninfo_t;

typedef struct {
    void *smem_start;             /* Start of frame buffer mem */
    u32 smem_len;                 /* Length of frame buffer mem */
    u32 type;                     /* see FB_TYPE_* */
    u32 type_aux;
    u32 visual;                   /* see FB_VISUAL_* */
    u16 xpanstep;
    u16 ypanstep;
    u16 ywrapstep;
    u32 line_length;              /* For one line */
    void *mmio_start;             /* Start of Memory Mapped I/O */
    u32 mmio_len;
    u32 accel;                    /* see FB_ACCEL_* */
} fb_fix_screeninfo_t;

typedef struct {
    u16 red;
    u16 green;
    u16 blue;
    u16 transp;
} fb_cmap_entry_t;

typedef struct {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    u32 color;
} fb_fillrect_t;

typedef struct {
    u32 dx;
    u32 dy;
    u32 sx;
    u32 sy;
    u32 width;
    u32 height;
} fb_copyarea_t;

typedef struct {
    u32 device_id;
    char name[64];
    char id[64];
    void *screen_base;
    phys_addr_t screen_phys;
    u32 screen_size;
    fb_var_screeninfo_t var;
    fb_fix_screeninfo_t fix;
    fb_cmap_entry_t cmap[FB_MAX_CMAP];
    u32 cmap_len;
    u32 flags;
    bool is_open;
    u32 ref_count;
    
    /* Acceleration ops */
    void (*fillrect)(fb_fillrect_t *rect);
    void (*copyarea)(fb_copyarea_t *area);
    void (*imageblit)(void *image);
    
    /* Driver private data */
    void *par;
} fb_info_t;

/* Registration entry points (defined further down this file). */
fb_info_t *fb_register_device(const char *name, void *base, phys_addr_t phys,
                              u32 size, u32 xres, u32 yres, u32 bpp);
fb_info_t *fb_register_device_ex(const char *name, void *base, phys_addr_t phys,
                                 u32 size, u32 xres, u32 yres, u32 bpp,
                                 u32 pitch);

typedef struct {
    bool initialized;
    fb_info_t devices[FB_MAX_DEVICES];
    u32 device_count;
    fb_info_t *current_device;
    spinlock_t lock;
} fb_driver_t;

static fb_driver_t g_fb;

/*===========================================================================*/
/*                         BASIC DRAWING OPERATIONS                          */
/*===========================================================================*/

static void fb_fillrect_generic(fb_fillrect_t *rect)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb || !fb->screen_base) return;
    
    u32 bpp = fb->var.bits_per_pixel;
    u32 bytes_per_pixel = bpp / 8;
    
    if (bpp == 32) {
        u32 *dst = (u32 *)((u8 *)fb->screen_base + 
                            rect->y * fb->fix.line_length + 
                            rect->x * 4);
        u32 pitch = fb->fix.line_length / 4;
        
        for (u32 y = 0; y < rect->height; y++) {
            for (u32 x = 0; x < rect->width; x++) {
                dst[y * pitch + x] = rect->color;
            }
        }
    } else if (bpp == 16) {
        u16 *dst = (u16 *)((u8 *)fb->screen_base + 
                            rect->y * fb->fix.line_length + 
                            rect->x * 2);
        u32 pitch = fb->fix.line_length / 2;
        
        for (u32 y = 0; y < rect->height; y++) {
            for (u32 x = 0; x < rect->width; x++) {
                dst[y * pitch + x] = (u16)rect->color;
            }
        }
    } else if (bpp == 8) {
        u8 *dst = (u8 *)((u8 *)fb->screen_base + 
                           rect->y * fb->fix.line_length + 
                           rect->x * 1);
        u32 pitch = fb->fix.line_length;
        
        for (u32 y = 0; y < rect->height; y++) {
            for (u32 x = 0; x < rect->width; x++) {
                dst[y * pitch + x] = (u8)rect->color;
            }
        }
    }
}

static void fb_copyarea_generic(fb_copyarea_t *area)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb || !fb->screen_base) return;
    
    u32 bpp = fb->var.bits_per_pixel / 8;
    u32 pitch = fb->fix.line_length / bpp;
    
    u32 *src = (u32 *)((u8 *)fb->screen_base + 
                        area->sy * fb->fix.line_length + 
                        area->sx * bpp);
    u32 *dst = (u32 *)((u8 *)fb->screen_base + 
                        area->dy * fb->fix.line_length + 
                        area->dx * bpp);
    
    /* Handle overlapping regions */
    if (dst > src) {
        /* Copy bottom-up */
        for (s32 y = area->height - 1; y >= 0; y--) {
            memmove(&dst[y * pitch], &src[y * pitch], area->width * bpp);
        }
    } else {
        /* Copy top-down */
        for (u32 y = 0; y < area->height; y++) {
            memmove(&dst[y * pitch], &src[y * pitch], area->width * bpp);
        }
    }
}

/*===========================================================================*/
/*                         PIXEL OPERATIONS                                  */
/*===========================================================================*/

int fb_set_pixel(u32 x, u32 y, u32 color)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb || !fb->screen_base) return -EINVAL;
    
    if (x >= fb->var.xres || y >= fb->var.yres) return -EINVAL;
    
    u32 bpp = fb->var.bits_per_pixel / 8;
    u32 *dst = (u32 *)((u8 *)fb->screen_base + 
                        y * fb->fix.line_length + 
                        x * bpp);
    
    *dst = color;
    return 0;
}

u32 fb_get_pixel(u32 x, u32 y)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb || !fb->screen_base) return 0;
    
    if (x >= fb->var.xres || y >= fb->var.yres) return 0;
    
    u32 bpp = fb->var.bits_per_pixel / 8;
    u32 *src = (u32 *)((u8 *)fb->screen_base + 
                        y * fb->fix.line_length + 
                        x * bpp);
    
    return *src;
}

void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    fb_fillrect_t rect = {x, y, w, h, color};
    
    fb_info_t *fb = g_fb.current_device;
    if (fb && fb->fillrect) {
        fb->fillrect(&rect);
    } else {
        fb_fillrect_generic(&rect);
    }
}

void fb_copy_area(u32 sx, u32 sy, u32 dx, u32 dy, u32 w, u32 h)
{
    fb_copyarea_t area = {dx, dy, sx, sy, w, h};
    
    fb_info_t *fb = g_fb.current_device;
    if (fb && fb->copyarea) {
        fb->copyarea(&area);
    } else {
        fb_copyarea_generic(&area);
    }
}

void fb_image_blit(void *image, u32 x, u32 y, u32 w, u32 h)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb || !fb->screen_base) return;
    
    if (fb->imageblit) {
        fb->imageblit(image);
    } else {
        /* Generic image blit */
        u32 bpp = fb->var.bits_per_pixel;
        
        if (bpp == 32) {
            u32 *src = (u32 *)image;
            u32 pitch = fb->fix.line_length / 4;
            u32 *dst = (u32 *)((u8 *)fb->screen_base + 
                                y * fb->fix.line_length + 
                                x * 4);
            
            for (u32 j = 0; j < h; j++) {
                for (u32 i = 0; i < w; i++) {
                    dst[j * pitch + i] = src[j * w + i];
                }
            }
        } else if (bpp == 16) {
            u16 *src = (u16 *)image;
            u32 pitch = fb->fix.line_length / 2;
            u16 *dst = (u16 *)((u8 *)fb->screen_base + 
                                y * fb->fix.line_length + 
                                x * 2);
            
            for (u32 j = 0; j < h; j++) {
                for (u32 i = 0; i < w; i++) {
                    dst[j * pitch + i] = src[j * w + i];
                }
            }
        } else if (bpp == 8) {
            u8 *src = (u8 *)image;
            u32 pitch = fb->fix.line_length;
            u8 *dst = (u8 *)((u8 *)fb->screen_base + 
                               y * fb->fix.line_length + 
                               x * 1);
            
            for (u32 j = 0; j < h; j++) {
                for (u32 i = 0; i < w; i++) {
                    dst[j * pitch + i] = src[j * w + i];
                }
            }
        }
    }
}

/*===========================================================================*/
/*                         COLOR MAP                                         */
/*===========================================================================*/

int fb_set_cmap_entry(u32 index, u16 red, u16 green, u16 blue, u16 transp)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb || index >= FB_MAX_CMAP) return -EINVAL;
    
    fb->cmap[index].red = red;
    fb->cmap[index].green = green;
    fb->cmap[index].blue = blue;
    fb->cmap[index].transp = transp;
    fb->cmap_len = index + 1;
    
    /* Update hardware palette if VGA */
    if (fb->flags & FB_FLAG_IS_VGA_PALETTE) {
        /* Would write to VGA DAC */
    }
    
    return 0;
}

int fb_get_cmap_entry(u32 index, u16 *red, u16 *green, u16 *blue, u16 *transp)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb || index >= FB_MAX_CMAP) return -EINVAL;
    
    if (red) *red = fb->cmap[index].red;
    if (green) *green = fb->cmap[index].green;
    if (blue) *blue = fb->cmap[index].blue;
    if (transp) *transp = fb->cmap[index].transp;
    
    return 0;
}

/*===========================================================================*/
/*                         MODE SETTING                                      */
/*===========================================================================*/

int fb_set_mode(u32 xres, u32 yres, u32 bpp)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb) return -EINVAL;
    
    fb->var.xres = xres;
    fb->var.yres = yres;
    fb->var.xres_virtual = xres;
    fb->var.yres_virtual = yres;
    fb->var.bits_per_pixel = bpp;
    
    fb->fix.line_length = xres * (bpp / 8);
    fb->screen_size = xres * yres * (bpp / 8);
    
    printk("[FB] Mode set: %dx%dx%d\n", xres, yres, bpp);
    return 0;
}

int fb_get_mode(u32 *xres, u32 *yres, u32 *bpp)
{
    fb_info_t *fb = g_fb.current_device;
    if (!fb) return -EINVAL;
    
    if (xres) *xres = fb->var.xres;
    if (yres) *yres = fb->var.yres;
    if (bpp) *bpp = fb->var.bits_per_pixel;
    
    return 0;
}

/*===========================================================================*/
/*                         DEVICE MANAGEMENT                                 */
/*===========================================================================*/

fb_info_t *fb_register_device_ex(const char *name, void *base, phys_addr_t phys,
                                  u32 size, u32 xres, u32 yres, u32 bpp,
                                  u32 pitch)
{
    spinlock_lock(&g_fb.lock);
    
    if (g_fb.device_count >= FB_MAX_DEVICES) {
        spinlock_unlock(&g_fb.lock);
        return NULL;
    }
    
    fb_info_t *fb = &g_fb.devices[g_fb.device_count++];
    memset(fb, 0, sizeof(fb_info_t));
    
    fb->device_id = g_fb.device_count;
    strncpy(fb->name, name, sizeof(fb->name) - 1);
    snprintf(fb->id, sizeof(fb->id), "fb%d", g_fb.device_count - 1);
    
    fb->screen_base = base;
    fb->screen_phys = phys;
    fb->screen_size = size;
    
    /* Set default mode */
    fb->var.xres = xres;
    fb->var.yres = yres;
    fb->var.bits_per_pixel = bpp;
    
    fb->fix.smem_start = base;
    fb->fix.smem_len = size;
    /*
     * Honour the pitch the bootloader actually gave us. GRUB commonly pads
     * each scanline to a larger alignment than xres * bytes_per_pixel, and
     * assuming the tight stride shears the image diagonally: every row
     * starts a few pixels further right than the one above it.
     */
    fb->fix.line_length = pitch ? pitch : (xres * (bpp / 8));
    fb->fix.accel = FB_HAS_ACCEL;
    
    /* Set default color map */
    for (u32 i = 0; i < 256; i++) {
        fb->cmap[i].red = (i << 8) | i;
        fb->cmap[i].green = (i << 8) | i;
        fb->cmap[i].blue = (i << 8) | i;
    }
    fb->cmap_len = 256;
    
    /* Set acceleration ops */
    fb->fillrect = fb_fillrect_generic;
    fb->copyarea = fb_copyarea_generic;
    
    fb->flags = FB_FLAG_MEMORY_MAPPED | FB_FLAG_IS_COLOR;
    
    /* Set as current device if first */
    if (g_fb.device_count == 1) {
        g_fb.current_device = fb;
    }
    
    spinlock_unlock(&g_fb.lock);
    
    printk("[FB] Registered %s: %dx%dx%d at 0x%p (%d KB)\n",
           name, xres, yres, bpp, base, size / 1024);
    
    return fb;
}

int fb_unregister_device(u32 device_id)
{
    spinlock_lock(&g_fb.lock);
    
    for (u32 i = 0; i < g_fb.device_count; i++) {
        if (g_fb.devices[i].device_id == device_id) {
            /* Shift devices */
            for (u32 j = i; j < g_fb.device_count - 1; j++) {
                g_fb.devices[j] = g_fb.devices[j + 1];
            }
            g_fb.device_count--;
            
            if (g_fb.current_device == &g_fb.devices[i]) {
                g_fb.current_device = g_fb.device_count > 0 ? &g_fb.devices[0] : NULL;
            }
            
            spinlock_unlock(&g_fb.lock);
            return 0;
        }
    }
    
    spinlock_unlock(&g_fb.lock);
    return -ENOENT;
}

fb_info_t *fb_get_device(u32 device_id)
{
    for (u32 i = 0; i < g_fb.device_count; i++) {
        if (g_fb.devices[i].device_id == device_id) {
            return &g_fb.devices[i];
        }
    }
    return NULL;
}

int fb_set_current_device(u32 device_id)
{
    fb_info_t *fb = fb_get_device(device_id);
    if (!fb) return -ENOENT;
    
    g_fb.current_device = fb;
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int framebuffer_init(void)
{
    printk("[FRAMEBUFFER] ========================================\n");
    printk("[FRAMEBUFFER] NEXUS OS Framebuffer Driver\n");
    printk("[FRAMEBUFFER] ========================================\n");
    
    memset(&g_fb, 0, sizeof(fb_driver_t));
    spinlock_init(&g_fb.lock);
    
    /* Register the actual GRUB-provided framebuffer when available */
    boot_params_t *params = get_boot_params();
    if (params && params->gfx_addr) {
        u32 fb_size = params->gfx_pitch * params->gfx_height;
        fb_register_device_ex("GRUB FB",
                              (void *)(uintptr_t)params->gfx_addr,
                              params->gfx_addr,
                              fb_size,
                              params->gfx_width,
                              params->gfx_height,
                              params->gfx_depth,
                              params->gfx_pitch);
        printk("[FRAMEBUFFER] Registered GRUB framebuffer: %dx%dx%d, pitch %u at 0x%llX\n",
               params->gfx_width, params->gfx_height, params->gfx_depth,
               params->gfx_pitch, params->gfx_addr);
    } else {
        /*
         * Fallback: VGA *text* memory, not the 0xA0000 graphics window.
         *
         * Registering 0xA0000 at 320x200x8 painted nothing: the display was
         * in text mode (console_init() had set it up that way), so writes to
         * the graphics aperture were never scanned out - which is exactly
         * why the VGA window stayed black while the serial log ran.
         *
         * The text buffer at 0xB8000 *is* what the card displays in text
         * mode, so registering it as a 720x400x4 surface means the graphical
         * renderer's output actually lands on screen.
         */
        void *vga_text = (void *)0xB8000;
        fb_register_device_ex("VGA text", vga_text, 0xB8000, 32768,
                              720, 400, 4, 80 * 2);
        printk("[FRAMEBUFFER] No GRUB framebuffer: registered VGA text memory "
               "(80x25 characters at 0xB8000) instead of an unusable "
               "320x200 graphics fallback\n");
    }
    
    printk("[FRAMEBUFFER] Framebuffer driver initialized\n");
    return 0;
}

int framebuffer_shutdown(void)
{
    printk("[FRAMEBUFFER] Shutting down framebuffer driver...\n");
    
    /* Unregister all devices */
    while (g_fb.device_count > 0) {
        fb_unregister_device(g_fb.devices[0].device_id);
    }
    
    g_fb.initialized = false;
    return 0;
}

/*
 * fb_register_device - Backwards-compatible registration with a tight pitch
 *
 * Callers that know their scanline stride should use fb_register_device_ex()
 * so a padded framebuffer is described correctly.
 */
fb_info_t *fb_register_device(const char *name, void *base, phys_addr_t phys,
                              u32 size, u32 xres, u32 yres, u32 bpp)
{
    return fb_register_device_ex(name, base, phys, size, xres, yres, bpp,
                                 xres * (bpp / 8));
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

fb_driver_t *fb_get_driver(void)
{
    return &g_fb;
}

fb_info_t *fb_get_current(void)
{
    return g_fb.current_device;
}

int fb_list_devices(fb_info_t *devices, u32 *count)
{
    if (!devices || !count) return -EINVAL;
    
    u32 copy = (*count < g_fb.device_count) ? *count : g_fb.device_count;
    memcpy(devices, g_fb.devices, sizeof(fb_info_t) * copy);
    *count = copy;
    
    return 0;
}
