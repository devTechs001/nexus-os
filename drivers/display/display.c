/*
 * NEXUS OS - Display Driver Implementation
 * drivers/display/display.c
 *
 * Comprehensive display driver implementation
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "display.h"
#include "../gpu/gpu.h"
#include "../video/drm.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         GLOBAL DISPLAY DRIVER STATE                       */
/*===========================================================================*/

static display_driver_t g_display_driver;

/*===========================================================================*/
/*                         DRIVER INITIALIZATION                             */
/*===========================================================================*/

/**
 * display_init - Initialize display driver
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_init(void)
{
    printk("[DISPLAY] ========================================\n");
    printk("[DISPLAY] NEXUS OS Display Driver\n");
    printk("[DISPLAY] ========================================\n");
    printk("[DISPLAY] Initializing display driver...\n");

    /* Clear driver state */
    memset(&g_display_driver, 0, sizeof(display_driver_t));
    g_display_driver.initialized = true;

    /* Set default gamma */
    for (u32 i = 0; i < 256; i++) {
        g_display_driver.gamma_r[i] = i;
        g_display_driver.gamma_g[i] = i;
        g_display_driver.gamma_b[i] = i;
    }

    g_display_driver.brightness = 100;
    g_display_driver.contrast = 100;
    g_display_driver.saturation = 100;

    /* Enumerate adapters */
    display_enumerate_adapters();

    printk("[DISPLAY] Found %d display adapter(s)\n", g_display_driver.adapter_count);

    if (g_display_driver.adapter_count > 0) {
        display_adapter_t *adapter = &g_display_driver.adapters[0];
        printk("[DISPLAY] Primary: %s (%s)\n", adapter->name, adapter->vendor);
        printk("[DISPLAY] VRAM: %d MB\n", (u32)(adapter->vram_size / (1024 * 1024)));
        printk("[DISPLAY] Connectors: %d\n", adapter->connector_count);
    }

    printk("[DISPLAY] Display driver initialized\n");
    printk("[DISPLAY] ========================================\n");

    return 0;
}

/**
 * display_shutdown - Shutdown display driver
 *
 * Returns: 0 on success
 */
int display_shutdown(void)
{
    printk("[DISPLAY] Shutting down display driver...\n");

    /* Restore video mode */
    display_restore_mode();

    /* Free all framebuffers */
    for (u32 i = 0; i < g_display_driver.framebuffer_count; i++) {
        display_destroy_framebuffer(i + 1);
    }

    g_display_driver.initialized = false;

    printk("[DISPLAY] Display driver shutdown complete\n");

    return 0;
}

/**
 * display_is_initialized - Check if display driver is initialized
 *
 * Returns: true if initialized, false otherwise
 */
bool display_is_initialized(void)
{
    return g_display_driver.initialized;
}

/*===========================================================================*/
/*                         ADAPTER MANAGEMENT                                */
/*===========================================================================*/

/**
 * display_enumerate_adapters - Enumerate display adapters
 *
 * Returns: Number of adapters found
 */
int display_enumerate_adapters(void)
{
    printk("[DISPLAY] Enumerating display adapters...\n");

    g_display_driver.adapter_count = 0;

    /* In real implementation, would enumerate PCI devices */
    /* For now, create mock adapters for different display types */

    /* Mock VESA adapter */
    display_adapter_t *vesa = &g_display_driver.adapters[g_display_driver.adapter_count++];
    vesa->adapter_id = 1;
    vesa->type = DISPLAY_TYPE_VESA;
    strcpy(vesa->name, "VESA VBE 3.0");
    strcpy(vesa->vendor, "VESA");
    vesa->vendor_id = 0x1234;
    vesa->device_id = 0x5678;
    vesa->revision = 0x03;
    vesa->capabilities = DISPLAY_CAP_VSYNC | DISPLAY_CAP_CURSOR | DISPLAY_CAP_MULTIMONITOR;
    vesa->vram_size = 16 * 1024 * 1024;  /* 16MB */
    vesa->is_active = true;
    vesa->is_primary = true;
    vesa->is_boot_device = true;
    vesa->cursor_width = 32;
    vesa->cursor_height = 32;

    /* Mock DisplayPort adapter */
    display_adapter_t *dp = &g_display_driver.adapters[g_display_driver.adapter_count++];
    dp->adapter_id = 2;
    dp->type = DISPLAY_TYPE_DISPLAYPORT;
    strcpy(dp->name, "DisplayPort Controller");
    strcpy(dp->vendor, "Intel");
    dp->vendor_id = GPU_VENDOR_INTEL;
    dp->device_id = 0x5912;
    dp->revision = 0x04;
    dp->capabilities = DISPLAY_CAP_VSYNC | DISPLAY_CAP_CURSOR | DISPLAY_CAP_SCALING |
                       DISPLAY_CAP_ROTATION | DISPLAY_CAP_MULTIMONITOR | DISPLAY_CAP_HIDPI;
    dp->vram_size = 128 * 1024 * 1024;  /* 128MB */
    dp->is_active = false;
    dp->is_primary = false;
    dp->cursor_width = 64;
    dp->cursor_height = 64;

    /* Mock HDMI adapter */
    display_adapter_t *hdmi = &g_display_driver.adapters[g_display_driver.adapter_count++];
    hdmi->adapter_id = 3;
    hdmi->type = DISPLAY_TYPE_HDMI;
    strcpy(hdmi->name, "HDMI Output");
    strcpy(hdmi->vendor, "AMD");
    hdmi->vendor_id = GPU_VENDOR_AMD;
    hdmi->device_id = 0x67DF;
    hdmi->revision = 0xC7;
    hdmi->capabilities = DISPLAY_CAP_VSYNC | DISPLAY_CAP_CURSOR | DISPLAY_CAP_SCALING |
                         DISPLAY_CAP_HDR | DISPLAY_CAP_ADAPTIVE_SYNC | DISPLAY_CAP_MULTIMONITOR;
    hdmi->vram_size = 256 * 1024 * 1024;  /* 256MB */
    hdmi->is_active = false;
    hdmi->is_primary = false;
    hdmi->cursor_width = 64;
    hdmi->cursor_height = 64;

    g_display_driver.primary_adapter = 0;

    return g_display_driver.adapter_count;
}

/**
 * display_get_adapter - Get display adapter
 * @adapter_id: Adapter ID
 *
 * Returns: Adapter pointer, or NULL if not found
 */
display_adapter_t *display_get_adapter(u32 adapter_id)
{
    for (u32 i = 0; i < g_display_driver.adapter_count; i++) {
        if (g_display_driver.adapters[i].adapter_id == adapter_id) {
            return &g_display_driver.adapters[i];
        }
    }

    return NULL;
}

/**
 * display_get_primary_adapter - Get primary display adapter
 *
 * Returns: Primary adapter pointer
 */
display_adapter_t *display_get_primary_adapter(void)
{
    if (g_display_driver.adapter_count == 0) {
        return NULL;
    }

    return &g_display_driver.adapters[g_display_driver.primary_adapter];
}

/**
 * display_get_active_adapter - Get active display adapter
 *
 * Returns: Active adapter pointer
 */
display_adapter_t *display_get_active_adapter(void)
{
    if (g_display_driver.adapter_count == 0) {
        return NULL;
    }

    for (u32 i = 0; i < g_display_driver.adapter_count; i++) {
        if (g_display_driver.adapters[i].is_active) {
            return &g_display_driver.adapters[i];
        }
    }

    return &g_display_driver.adapters[0];
}

/**
 * display_set_active_adapter - Set active display adapter
 * @adapter_id: Adapter ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_set_active_adapter(u32 adapter_id)
{
    display_adapter_t *adapter = display_get_adapter(adapter_id);
    if (!adapter) {
        return -ENOENT;
    }

    /* Set all adapters as inactive */
    for (u32 i = 0; i < g_display_driver.adapter_count; i++) {
        g_display_driver.adapters[i].is_active = false;
    }

    /* Set new active adapter */
    adapter->is_active = true;
    g_display_driver.primary_adapter = adapter->adapter_id - 1;

    return 0;
}

/**
 * display_list_adapters - List all display adapters
 * @adapters: Adapters array
 * @count: Count pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_list_adapters(display_adapter_t *adapters, u32 *count)
{
    if (!adapters || !count || *count == 0) {
        return -EINVAL;
    }

    u32 copy_count = (*count < g_display_driver.adapter_count) ? 
                     *count : g_display_driver.adapter_count;
    
    memcpy(adapters, g_display_driver.adapters, sizeof(display_adapter_t) * copy_count);
    *count = copy_count;

    return 0;
}

/*===========================================================================*/
/*                         CONNECTOR MANAGEMENT                              */
/*===========================================================================*/

/**
 * display_get_connectors - Get connectors for adapter
 * @adapter_id: Adapter ID
 * @connectors: Connectors array
 * @count: Count pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_get_connectors(u32 adapter_id, display_connector_t *connectors, u32 *count)
{
    display_adapter_t *adapter = display_get_adapter(adapter_id);
    if (!adapter || !connectors || !count) {
        return -EINVAL;
    }

    /* Mock connectors based on adapter type */
    display_connector_t mock_connectors[4];
    u32 connector_count = 0;

    if (adapter->type == DISPLAY_TYPE_VESA) {
        /* VGA connector */
        mock_connectors[connector_count].connector_id = 1;
        mock_connectors[connector_count].type = CONNECTOR_TYPE_VGA;
        strcpy(mock_connectors[connector_count].type_name, "VGA");
        mock_connectors[connector_count].is_connected = true;
        mock_connectors[connector_count].is_enabled = true;
        connector_count++;
    } else if (adapter->type == DISPLAY_TYPE_DISPLAYPORT) {
        /* DisplayPort connector */
        mock_connectors[connector_count].connector_id = 2;
        mock_connectors[connector_count].type = CONNECTOR_TYPE_DP;
        strcpy(mock_connectors[connector_count].type_name, "DP");
        mock_connectors[connector_count].is_connected = true;
        mock_connectors[connector_count].is_enabled = true;
        connector_count++;
    } else if (adapter->type == DISPLAY_TYPE_HDMI) {
        /* HDMI connector */
        mock_connectors[connector_count].connector_id = 3;
        mock_connectors[connector_count].type = CONNECTOR_TYPE_HDMI_A;
        strcpy(mock_connectors[connector_count].type_name, "HDMI-A");
        mock_connectors[connector_count].is_connected = true;
        mock_connectors[connector_count].is_enabled = true;
        connector_count++;
    }

    u32 copy_count = (*count < connector_count) ? *count : connector_count;
    memcpy(connectors, mock_connectors, sizeof(display_connector_t) * copy_count);
    *count = copy_count;

    return 0;
}

/**
 * display_detect_connectors - Detect connected displays
 * @adapter_id: Adapter ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_detect_connectors(u32 adapter_id)
{
    display_adapter_t *adapter = display_get_adapter(adapter_id);
    if (!adapter) {
        return -ENOENT;
    }

    /* In real implementation, would probe DDC/CI for connected displays */
    printk("[DISPLAY] Detecting connectors on adapter %d...\n", adapter_id);

    return 0;
}

/*===========================================================================*/
/*                         MODE SETTING                                      */
/*===========================================================================*/

/**
 * display_get_modes - Get display modes for connector
 * @connector_id: Connector ID
 * @modes: Modes array
 * @count: Count pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_get_modes(u32 connector_id, display_mode_t *modes, u32 *count)
{
    if (!modes || !count || *count == 0) {
        return -EINVAL;
    }

    /* Mock display modes */
    display_mode_t mock_modes[] = {
        {1, 25175, 640, 656, 752, 800, 0, 480, 490, 492, 525, 0, 60, 0, 
         DRM_MODE_TYPE_PREFERRED, 0, 0, "640x480@60", true, false, false},
        {2, 40000, 800, 840, 968, 1056, 0, 600, 601, 605, 628, 0, 60, 0,
         0, 0, 0, "800x600@60", false, false, false},
        {3, 65000, 1024, 1048, 1184, 1344, 0, 768, 771, 777, 806, 0, 60, 0,
         0, 0, 0, "1024x768@60", false, false, false},
        {4, 148500, 1920, 2008, 2052, 2200, 0, 1080, 1084, 1089, 1125, 0, 60, 0,
         0, 0, 0, "1920x1080@60", false, false, false},
        {5, 297000, 1920, 2008, 2052, 2200, 0, 1080, 1084, 1089, 1125, 0, 120, 0,
         0, 0, 0, "1920x1080@120", false, false, false},
        {6, 594000, 3840, 4016, 4104, 4400, 0, 2160, 2168, 2178, 2250, 0, 60, 0,
         0, 0, 0, "3840x2160@60", false, false, false},
    };

    u32 copy_count = (*count < 6) ? *count : 6;
    memcpy(modes, mock_modes, sizeof(display_mode_t) * copy_count);
    *count = copy_count;

    return 0;
}

/**
 * display_set_mode - Set display mode
 * @crtc_id: CRTC ID
 * @connector_id: Connector ID
 * @mode: Display mode
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_set_mode(u32 crtc_id, u32 connector_id, display_mode_t *mode)
{
    (void)crtc_id; (void)connector_id; (void)mode;

    /* In real implementation, would set hardware display mode */
    printk("[DISPLAY] Setting mode: %s\n", mode->name);

    return 0;
}

/**
 * display_get_preferred_mode - Get preferred mode for connector
 * @connector_id: Connector ID
 * @mode: Mode pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_get_preferred_mode(u32 connector_id, display_mode_t *mode)
{
    if (!mode) {
        return -EINVAL;
    }

    display_mode_t modes[6];
    u32 count = 6;
    
    int ret = display_get_modes(connector_id, modes, &count);
    if (ret < 0) {
        return ret;
    }

    /* Find preferred mode */
    for (u32 i = 0; i < count; i++) {
        if (modes[i].is_preferred) {
            *mode = modes[i];
            return 0;
        }
    }

    /* Return first mode if no preferred */
    if (count > 0) {
        *mode = modes[0];
        return 0;
    }

    return -ENOENT;
}

/*===========================================================================*/
/*                         FRAMEBUFFER MANAGEMENT                            */
/*===========================================================================*/

/**
 * display_create_framebuffer - Create framebuffer
 * @width: Width
 * @height: Height
 * @format: Pixel format
 * @fb: Framebuffer pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_create_framebuffer(u32 width, u32 height, u32 format, display_framebuffer_t *fb)
{
    if (!g_display_driver.initialized || width == 0 || height == 0 || !fb) {
        return -EINVAL;
    }

    if (g_display_driver.framebuffer_count >= DISPLAY_MAX_FRAMEBUFFERS) {
        return -ENOMEM;
    }

    u32 bpp = display_get_format_bpp(format);
    u32 size = width * height * (bpp / 8);

    display_framebuffer_t *framebuffer = &g_display_driver.framebuffers[g_display_driver.framebuffer_count];
    framebuffer->fb_id = g_display_driver.framebuffer_count + 1;
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->stride = width * (bpp / 8);
    framebuffer->bpp = bpp;
    framebuffer->format = format;
    framebuffer->size = size;
    framebuffer->base = kmalloc(size);
    framebuffer->is_mapped = false;

    if (!framebuffer->base) {
        return -ENOMEM;
    }

    memset(framebuffer->base, 0, size);

    g_display_driver.framebuffer_count++;
    *fb = *framebuffer;

    return 0;
}

/**
 * display_destroy_framebuffer - Destroy framebuffer
 * @fb_id: Framebuffer ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_destroy_framebuffer(u32 fb_id)
{
    if (fb_id == 0 || fb_id > g_display_driver.framebuffer_count) {
        return -EINVAL;
    }

    display_framebuffer_t *fb = &g_display_driver.framebuffers[fb_id - 1];

    if (fb->base) {
        kfree(fb->base);
        fb->base = NULL;
    }

    fb->fb_id = 0;
    g_display_driver.framebuffer_count--;

    return 0;
}

/*===========================================================================*/
/*                         DRAWING PRIMITIVES                                */
/*===========================================================================*/

/**
 * display_set_pixel - Set pixel in framebuffer
 * @x: X coordinate
 * @y: Y coordinate
 * @color: Color value
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_set_pixel(u32 x, u32 y, u32 color)
{
    display_framebuffer_t *fb = display_get_primary_framebuffer();
    if (!fb || !fb->base) {
        return -ENOENT;
    }

    if (x >= fb->width || y >= fb->height) {
        return -EINVAL;
    }

    u32 *pixels = (u32 *)fb->base;
    pixels[y * fb->stride / 4 + x] = color;

    return 0;
}

/**
 * display_fill_rect - Fill rectangle in framebuffer
 * @x: X coordinate
 * @y: Y coordinate
 * @w: Width
 * @h: Height
 * @color: Color value
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    display_framebuffer_t *fb = display_get_primary_framebuffer();
    if (!fb || !fb->base) {
        return -ENOENT;
    }

    if (x >= fb->width || y >= fb->height) {
        return -EINVAL;
    }

    /* Clamp width and height */
    if (x + w > fb->width) w = fb->width - x;
    if (y + h > fb->height) h = fb->height - y;

    u32 *pixels = (u32 *)fb->base;
    for (u32 j = 0; j < h; j++) {
        for (u32 i = 0; i < w; i++) {
            pixels[(y + j) * fb->stride / 4 + (x + i)] = color;
        }
    }

    return 0;
}

/**
 * display_draw_line - Draw line in framebuffer
 * @x0: Start X
 * @y0: Start Y
 * @x1: End X
 * @y1: End Y
 * @color: Color value
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_draw_line(u32 x0, u32 y0, u32 x1, u32 y1, u32 color)
{
    /* Bresenham's line algorithm */
    s32 dx = (s32)x1 - (s32)x0;
    s32 dy = (s32)y1 - (s32)y0;
    s32 sx = (dx > 0) ? 1 : -1;
    s32 sy = (dy > 0) ? 1 : -1;
    
    dx = (dx > 0) ? dx : -dx;
    dy = (dy > 0) ? dy : -dy;

    s32 err = ((dx > dy) ? dx : -dy) / 2;
    s32 e2;

    for (;;) {
        display_set_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }

    return 0;
}

/**
 * display_draw_circle - Draw circle in framebuffer
 * @cx: Center X
 * @cy: Center Y
 * @r: Radius
 * @color: Color value
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_draw_circle(u32 cx, u32 cy, u32 r, u32 color)
{
    /* Midpoint circle algorithm */
    s32 x = r;
    s32 y = 0;
    s32 err = 0;

    while (x >= y) {
        display_set_pixel(cx + x, cy + y, color);
        display_set_pixel(cx + y, cy + x, color);
        display_set_pixel(cx - y, cy + x, color);
        display_set_pixel(cx - x, cy + y, color);
        display_set_pixel(cx - x, cy - y, color);
        display_set_pixel(cx - y, cy - x, color);
        display_set_pixel(cx + y, cy - x, color);
        display_set_pixel(cx + x, cy - y, color);

        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         VSYNC AND POWER MANAGEMENT                        */
/*===========================================================================*/

/**
 * display_enable_vsync - Enable/disable VSync
 * @enable: Enable VSync
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_enable_vsync(bool enable)
{
    g_display_driver.vsync_enabled = enable ? 1 : 0;
    printk("[DISPLAY] VSync %s\n", enable ? "enabled" : "disabled");
    return 0;
}

/**
 * display_wait_vsync - Wait for VSync
 *
 * Returns: 0 on success
 */
int display_wait_vsync(void)
{
    /* In real implementation, would wait for vertical blank */
    return 0;
}

/**
 * display_set_power_state - Set display power state
 * @state: Power state (0=On, 1=Standby, 2=Suspend, 3=Off)
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_set_power_state(u32 state)
{
    g_display_driver.power_state = state;
    printk("[DISPLAY] Power state: %d\n", state);
    return 0;
}

/**
 * display_set_brightness - Set display brightness
 * @brightness: Brightness (0-100)
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_set_brightness(u32 brightness)
{
    if (brightness > 100) {
        return -EINVAL;
    }

    g_display_driver.brightness = brightness;
    printk("[DISPLAY] Brightness: %d%%\n", brightness);
    return 0;
}

/*===========================================================================*/
/*                         MULTI-MONITOR SUPPORT                             */
/*===========================================================================*/

/**
 * display_get_monitor_count - Get number of connected monitors
 *
 * Returns: Monitor count
 */
int display_get_monitor_count(void)
{
    u32 count = 0;
    
    for (u32 i = 0; i < g_display_driver.adapter_count; i++) {
        display_adapter_t *adapter = &g_display_driver.adapters[i];
        if (adapter->is_active) {
            /* Count connected connectors */
            display_connector_t connectors[8];
            u32 connector_count = 8;
            
            if (display_get_connectors(adapter->adapter_id, connectors, &connector_count) == 0) {
                for (u32 j = 0; j < connector_count; j++) {
                    if (connectors[j].is_connected) {
                        count++;
                    }
                }
            }
        }
    }

    return count;
}

/**
 * display_clone_monitor - Clone one monitor to another
 * @source: Source monitor index
 * @target: Target monitor index
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_clone_monitor(u32 source, u32 target)
{
    (void)source; (void)target;

    /* In real implementation, would clone display output */
    printk("[DISPLAY] Cloning monitor %d to %d\n", source, target);
    return 0;
}

/**
 * display_extend_monitor - Extend display to another monitor
 * @source: Source monitor index
 * @target: Target monitor index
 * @position: Position (0=Left, 1=Right, 2=Above, 3=Below)
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_extend_monitor(u32 source, u32 target, u32 position)
{
    (void)source; (void)target; (void)position;

    /* In real implementation, would extend display */
    printk("[DISPLAY] Extending monitor %d to %d (position: %d)\n", source, target, position);
    return 0;
}

/*===========================================================================*/
/*                         HIDPI SUPPORT                                     */
/*===========================================================================*/

/**
 * display_set_scale_factor - Set display scale factor
 * @adapter_id: Adapter ID
 * @scale_x: Horizontal scale (16.16 fixed point)
 * @scale_y: Vertical scale (16.16 fixed point)
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_set_scale_factor(u32 adapter_id, u32 scale_x, u32 scale_y)
{
    display_adapter_t *adapter = display_get_adapter(adapter_id);
    if (!adapter) {
        return -ENOENT;
    }

    printk("[DISPLAY] Setting scale factor: %d.%04d x %d.%04d\n", 
           scale_x >> 16, (scale_x & 0xFFFF) * 10000 / 65536,
           scale_y >> 16, (scale_y & 0xFFFF) * 10000 / 65536);
    
    return 0;
}

/**
 * display_get_dpi - Get display DPI
 * @adapter_id: Adapter ID
 * @dpi_x: Horizontal DPI pointer
 * @dpi_y: Vertical DPI pointer
 *
 * Returns: 0 on success, negative error code on failure
 */
int display_get_dpi(u32 adapter_id, u32 *dpi_x, u32 *dpi_y)
{
    display_adapter_t *adapter = display_get_adapter(adapter_id);
    if (!adapter || !dpi_x || !dpi_y) {
        return -EINVAL;
    }

    /* Default DPI (standard monitor) */
    *dpi_x = 96;
    *dpi_y = 96;

    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * display_get_type_name - Get display type name
 * @type: Display type
 *
 * Returns: Type name string
 */
const char *display_get_type_name(u32 type)
{
    switch (type) {
        case DISPLAY_TYPE_VGA:          return "VGA";
        case DISPLAY_TYPE_SVGA:         return "SVGA";
        case DISPLAY_TYPE_VESA:         return "VESA";
        case DISPLAY_TYPE_FRAMEBUFFER:  return "Framebuffer";
        case DISPLAY_TYPE_DISPLAYPORT:  return "DisplayPort";
        case DISPLAY_TYPE_HDMI:         return "HDMI";
        case DISPLAY_TYPE_DVI:          return "DVI";
        case DISPLAY_TYPE_LVDS:         return "LVDS";
        case DISPLAY_TYPE_eDP:          return "eDP";
        case DISPLAY_TYPE_TOUCHSCREEN:  return "Touchscreen";
        default:                        return "Unknown";
    }
}

/**
 * display_get_connector_type_name - Get connector type name
 * @type: Connector type
 *
 * Returns: Connector type name string
 */
const char *display_get_connector_type_name(u32 type)
{
    switch (type) {
        case CONNECTOR_TYPE_VGA:    return "VGA";
        case CONNECTOR_TYPE_DVI_I:  return "DVI-I";
        case CONNECTOR_TYPE_DVI_D:  return "DVI-D";
        case CONNECTOR_TYPE_DP:     return "DisplayPort";
        case CONNECTOR_TYPE_HDMI_A: return "HDMI-A";
        case CONNECTOR_TYPE_HDMI_B: return "HDMI-B";
        case CONNECTOR_TYPE_eDP:    return "eDP";
        case CONNECTOR_TYPE_LVDS:   return "LVDS";
        case CONNECTOR_TYPE_DSI:    return "DSI";
        case CONNECTOR_TYPE_USB_C:  return "USB-C";
        default:                    return "Unknown";
    }
}

/**
 * display_get_pixel_format_name - Get pixel format name
 * @format: Pixel format
 *
 * Returns: Format name string
 */
const char *display_get_pixel_format_name(u32 format)
{
    switch (format) {
        case PIXEL_FORMAT_RGB565:   return "RGB565";
        case PIXEL_FORMAT_RGB888:   return "RGB888";
        case PIXEL_FORMAT_BGR888:   return "BGR888";
        case PIXEL_FORMAT_ARGB8888: return "ARGB8888";
        case PIXEL_FORMAT_ABGR8888: return "ABGR8888";
        case PIXEL_FORMAT_XRGB8888: return "XRGB8888";
        case PIXEL_FORMAT_XBGR8888: return "XBGR8888";
        case PIXEL_FORMAT_RGBX8888: return "RGBX8888";
        case PIXEL_FORMAT_BGRX8888: return "BGRX8888";
        default:                    return "Unknown";
    }
}

/**
 * display_get_format_bpp - Get bits per pixel for format
 * @format: Pixel format
 *
 * Returns: Bits per pixel
 */
u32 display_get_format_bpp(u32 format)
{
    switch (format) {
        case PIXEL_FORMAT_RGB565:   return 16;
        case PIXEL_FORMAT_RGB888:   return 24;
        case PIXEL_FORMAT_BGR888:   return 24;
        case PIXEL_FORMAT_ARGB8888: return 32;
        case PIXEL_FORMAT_ABGR8888: return 32;
        case PIXEL_FORMAT_XRGB8888: return 32;
        case PIXEL_FORMAT_XBGR8888: return 32;
        case PIXEL_FORMAT_RGBX8888: return 32;
        case PIXEL_FORMAT_BGRX8888: return 32;
        default:                    return 32;
    }
}

/**
 * display_pack_color - Pack color components into pixel value
 * @r: Red
 * @g: Green
 * @b: Blue
 * @format: Pixel format
 *
 * Returns: Packed color value
 */
u32 display_pack_color(u8 r, u8 g, u8 b, u32 format)
{
    switch (format) {
        case PIXEL_FORMAT_RGB565:
            return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        case PIXEL_FORMAT_RGB888:
            return (r << 16) | (g << 8) | b;
        case PIXEL_FORMAT_BGR888:
            return (b << 16) | (g << 8) | r;
        case PIXEL_FORMAT_ARGB8888:
            return (0xFF << 24) | (r << 16) | (g << 8) | b;
        case PIXEL_FORMAT_XRGB8888:
            return (0xFF << 24) | (r << 16) | (g << 8) | b;
        default:
            return (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
}

/**
 * display_get_primary_framebuffer - Get primary framebuffer
 *
 * Returns: Primary framebuffer pointer, or NULL if not found
 */
display_framebuffer_t *display_get_primary_framebuffer(void)
{
    if (g_display_driver.framebuffer_count == 0) {
        return NULL;
    }

    return &g_display_driver.framebuffers[0];
}

/**
 * display_get_driver - Get display driver instance
 *
 * Returns: Pointer to driver instance
 */
display_driver_t *display_get_driver(void)
{
    return &g_display_driver;
}
