/*
 * NEXUS OS - DSI (Display Serial Interface) Driver
 * drivers/video/dsi.c
 *
 * MIPI DSI display interface driver
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         DSI CONFIGURATION                                 */
/*===========================================================================*/

#define DSI_MAX_DEVICES           4
#define DSI_MAX_LANES             4
#define DSI_MAX_VC                4

/* DSI Data Types */
#define DSI_DT_DCS_SHORT_WRITE        0x05
#define DSI_DT_DCS_SHORT_WRITE_PARAM  0x15
#define DSI_DT_DCS_READ               0x06
#define DSI_DT_DCS_LONG_WRITE         0x39
#define DSI_DT_GENERIC_SHORT_WRITE    0x03
#define DSI_DT_GENERIC_READ           0x04
#define DSI_DT_GENERIC_LONG_WRITE     0x29
#define DSI_DT_PIXEL_STREAM           0x24

/* DSI Commands */
#define DSI_CMD_NOP                   0x00
#define DSI_CMD_SWRESET               0x01
#define DSI_CMD_RDDID                 0x04
#define DSI_CMD_RDDST                 0x09
#define DSI_CMD_SLPIN                 0x10
#define DSI_CMD_SLPOUT                0x11
#define DSI_CMD_PTLON                 0x12
#define DSI_CMD_NORON                 0x13
#define DSI_CMD_INVOFF                0x20
#define DSI_CMD_INVON                 0x21
#define DSI_CMD_DISPOFF               0x28
#define DSI_CMD_DISPON                0x29
#define DSI_CMD_CASET                 0x2A
#define DSI_CMD_RASET                 0x2B
#define DSI_CMD_RAMWR                 0x2C
#define DSI_CMD_RAMRD                 0x2E
#define DSI_CMD_COLMOD                0x3A
#define DSI_CMD_MADCTL                0x36
#define DSI_CMD_VSCSAD                0x37

/* MADCTL Flags */
#define DSI_MADCTL_MY                 0x80
#define DSI_MADCTL_MX                 0x40
#define DSI_MADCTL_MV                 0x20
#define DSI_MADCTL_ML                 0x10
#define DSI_MADCTL_RGB                0x00
#define DSI_MADCTL_BGR                0x08

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u8 channel;
    u8 data_type;
    u8 data[4];
    u32 data_len;
} dsi_short_packet_t;

typedef struct {
    u8 channel;
    u8 data_type;
    u16 data_len;
    u8 *payload;
} dsi_long_packet_t;

typedef struct {
    u32 lane_count;
    u32 bitrate;        /* Mbps per lane */
    u32 hfp;            /* Horizontal front porch */
    u32 hbp;            /* Horizontal back porch */
    u32 hsa;            /* Horizontal sync active */
    u32 vfp;            /* Vertical front porch */
    u32 vbp;            /* Vertical back porch */
    u32 vsa;            /* Vertical sync active */
} dsi_timing_t;

typedef struct {
    u32 device_id;
    char name[64];
    bool is_connected;
    bool is_enabled;
    u32 format;         /* Pixel format */
    u32 width;
    u32 height;
    dsi_timing_t timing;
    u8 madctl;
    u8 colmod;
    void *driver_data;
} dsi_device_t;

typedef struct {
    bool initialized;
    dsi_device_t devices[DSI_MAX_DEVICES];
    u32 device_count;
    dsi_device_t *current_device;
    u32 lanes;
    u32 bitrate;
    bool lp_mode;
    spinlock_t lock;
} dsi_driver_t;

static dsi_driver_t g_dsi;

/*===========================================================================*/
/*                         LOW-LEVEL DSI OPERATIONS                          */
/*===========================================================================*/

static void dsi_write_reg(u32 reg, u32 value)
{
    /* In real implementation, would write to DSI controller */
    (void)reg; (void)value;
}

static u32 dsi_read_reg(u32 reg)
{
    /* In real implementation, would read from DSI controller */
    (void)reg;
    return 0;
}

static void dsi_send_short_packet(dsi_short_packet_t *pkt)
{
    /* Send short packet on DSI link */
    /* Format: [Channel:2][Data Type:6][ECC:8][Data0:8][Data1:8][Checksum:16] */
    (void)pkt;
}

static void dsi_send_long_packet(dsi_long_packet_t *pkt)
{
    /* Send long packet on DSI link */
    /* Header: [Channel:2][Data Type:6][Word Count:16][ECC:8] */
    /* Payload: [Data0...DataN] */
    /* Checksum: [16-bit] */
    (void)pkt;
}

static int dsi_send_command(u8 cmd, u8 *params, u32 param_len)
{
    dsi_device_t *dev = g_dsi.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    if (param_len <= 2) {
        /* Short packet */
        dsi_short_packet_t pkt;
        pkt.channel = 0;
        pkt.data_type = DSI_DT_DCS_SHORT_WRITE_PARAM;
        pkt.data[0] = cmd;
        pkt.data[1] = param_len > 0 ? params[0] : 0;
        pkt.data[2] = param_len > 1 ? params[1] : 0;
        pkt.data_len = param_len;
        
        dsi_send_short_packet(&pkt);
    } else {
        /* Long packet */
        dsi_long_packet_t pkt;
        pkt.channel = 0;
        pkt.data_type = DSI_DT_DCS_LONG_WRITE;
        pkt.data_len = param_len + 1;  /* Include command */
        pkt.payload = kmalloc(pkt.data_len);
        
        if (!pkt.payload) return -ENOMEM;
        
        pkt.payload[0] = cmd;
        if (params && param_len > 0) {
            memcpy(&pkt.payload[1], params, param_len);
        }
        
        dsi_send_long_packet(&pkt);
        kfree(pkt.payload);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         DSI COMMANDS                                      */
/*===========================================================================*/

static int dsi_cmd_nop(void)
{
    return dsi_send_command(DSI_CMD_NOP, NULL, 0);
}

static int dsi_cmd_swreset(void)
{
    printk("[DSI] Software reset\n");
    return dsi_send_command(DSI_CMD_SWRESET, NULL, 0);
}

static int dsi_cmd_slpout(void)
{
    printk("[DSI] Sleep out\n");
    return dsi_send_command(DSI_CMD_SLPOUT, NULL, 0);
}

static int dsi_cmd_slpin(void)
{
    printk("[DSI] Sleep in\n");
    return dsi_send_command(DSI_CMD_SLPIN, NULL, 0);
}

static int dsi_cmd_dispon(void)
{
    printk("[DSI] Display on\n");
    return dsi_send_command(DSI_CMD_DISPON, NULL, 0);
}

static int dsi_cmd_dispoff(void)
{
    printk("[DSI] Display off\n");
    return dsi_send_command(DSI_CMD_DISPOFF, NULL, 0);
}

static int dsi_cmd_set_address(u16 x, u16 y, u16 w, u16 h)
{
    u8 caset_params[4] = {
        (x >> 8) & 0xFF, x & 0xFF,
        ((x + w - 1) >> 8) & 0xFF, (x + w - 1) & 0xFF
    };
    
    u8 raset_params[4] = {
        (y >> 8) & 0xFF, y & 0xFF,
        ((y + h - 1) >> 8) & 0xFF, (y + h - 1) & 0xFF
    };
    
    dsi_send_command(DSI_CMD_CASET, caset_params, 4);
    dsi_send_command(DSI_CMD_RASET, raset_params, 4);
    
    return 0;
}

static int dsi_cmd_set_pixel_format(u8 format)
{
    u8 param = format;
    printk("[DSI] Set pixel format: 0x%02X\n", format);
    return dsi_send_command(DSI_CMD_COLMOD, &param, 1);
}

static int dsi_cmd_set_madctl(u8 madctl)
{
    printk("[DSI] Set MADCTL: 0x%02X\n", madctl);
    return dsi_send_command(DSI_CMD_MADCTL, &madctl, 1);
}

static int dsi_cmd_memory_write(void *data, u32 size)
{
    dsi_device_t *dev = g_dsi.current_device;
    if (!dev) return -ENODEV;
    
    dsi_long_packet_t pkt;
    pkt.channel = 0;
    pkt.data_type = DSI_DT_PIXEL_STREAM;
    pkt.data_len = size;
    pkt.payload = (u8 *)data;
    
    dsi_send_long_packet(&pkt);
    return 0;
}

/*===========================================================================*/
/*                         DEVICE INITIALIZATION                             */
/*===========================================================================*/

static int dsi_init_device(dsi_device_t *dev)
{
    printk("[DSI] Initializing device: %s\n", dev->name);
    
    /* Wait for device to be ready */
    mdelay(120);
    
    /* Software reset */
    dsi_cmd_swreset();
    mdelay(120);
    
    /* Sleep out */
    dsi_cmd_slpout();
    mdelay(120);
    
    /* Set pixel format */
    dsi_cmd_set_pixel_format(dev->colmod);
    
    /* Set MADCTL */
    dsi_cmd_set_madctl(dev->madctl);
    
    /* Set display area */
    dsi_cmd_set_address(0, 0, dev->width, dev->height);
    
    /* Turn on display */
    dsi_cmd_dispon();
    mdelay(50);
    
    dev->is_enabled = true;
    return 0;
}

/*===========================================================================*/
/*                         PUBLIC API                                        */
/*===========================================================================*/

int dsi_register_device(const char *name, u32 width, u32 height, 
                         u32 lanes, u32 bitrate)
{
    spinlock_lock(&g_dsi.lock);
    
    if (g_dsi.device_count >= DSI_MAX_DEVICES) {
        spinlock_unlock(&g_dsi.lock);
        return -ENOMEM;
    }
    
    dsi_device_t *dev = &g_dsi.devices[g_dsi.device_count++];
    memset(dev, 0, sizeof(dsi_device_t));
    
    dev->device_id = g_dsi.device_count;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->width = width;
    dev->height = height;
    dev->is_connected = true;
    
    /* Default settings */
    dev->format = 24;  /* RGB888 */
    dev->colmod = 0x77;  /* 24-bit RGB */
    dev->madctl = DSI_MADCTL_MX | DSI_MADCTL_MY | DSI_MADCTL_RGB;
    
    /* Timing */
    dev->timing.lane_count = lanes;
    dev->timing.bitrate = bitrate;
    
    /* Set as current if first */
    if (g_dsi.device_count == 1) {
        g_dsi.current_device = dev;
    }
    
    spinlock_unlock(&g_dsi.lock);
    
    printk("[DSI] Registered device: %s (%dx%d, %d lanes, %d Mbps)\n",
           name, width, height, lanes, bitrate);
    
    /* Initialize device */
    dsi_init_device(dev);
    
    return dev->device_id;
}

int dsi_unregister_device(u32 device_id)
{
    dsi_device_t *dev = NULL;
    s32 dev_idx = -1;
    
    for (u32 i = 0; i < g_dsi.device_count; i++) {
        if (g_dsi.devices[i].device_id == device_id) {
            dev = &g_dsi.devices[i];
            dev_idx = i;
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    /* Turn off display */
    dsi_cmd_dispoff();
    dsi_cmd_slpin();
    
    spinlock_lock(&g_dsi.lock);
    
    /* Shift devices */
    for (u32 i = dev_idx; i < g_dsi.device_count - 1; i++) {
        g_dsi.devices[i] = g_dsi.devices[i + 1];
    }
    g_dsi.device_count--;
    
    if (g_dsi.current_device == dev) {
        g_dsi.current_device = g_dsi.device_count > 0 ? &g_dsi.devices[0] : NULL;
    }
    
    spinlock_unlock(&g_dsi.lock);
    
    return 0;
}

int dsi_set_pixel(u32 x, u32 y, u32 color)
{
    dsi_device_t *dev = g_dsi.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    if (x >= dev->width || y >= dev->height) return -EINVAL;
    
    /* Set address */
    dsi_cmd_set_address(x, y, 1, 1);
    
    /* Write pixel */
    u8 pixel[3];
    pixel[0] = (color >> 16) & 0xFF;  /* R */
    pixel[1] = (color >> 8) & 0xFF;   /* G */
    pixel[2] = color & 0xFF;          /* B */
    
    return dsi_cmd_memory_write(pixel, 3);
}

int dsi_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    dsi_device_t *dev = g_dsi.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    /* Set address */
    dsi_cmd_set_address(x, y, w, h);
    
    /* Allocate buffer */
    u32 size = w * h * 3;
    u8 *buffer = kmalloc(size);
    if (!buffer) return -ENOMEM;
    
    /* Fill buffer with color */
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;
    
    for (u32 i = 0; i < w * h; i++) {
        buffer[i * 3 + 0] = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }
    
    /* Write to display */
    dsi_cmd_memory_write(buffer, size);
    kfree(buffer);
    
    return 0;
}

int dsi_blit(void *image, u32 x, u32 y, u32 w, u32 h)
{
    dsi_device_t *dev = g_dsi.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    /* Set address */
    dsi_cmd_set_address(x, y, w, h);
    
    /* Write image data */
    return dsi_cmd_memory_write(image, w * h * 3);
}

int dsi_set_brightness(u32 level)
{
    u8 param = level & 0xFF;
    return dsi_send_command(0x51, &param, 1);  /* Write display brightness */
}

int dsi_set_orientation(u32 orientation)
{
    dsi_device_t *dev = g_dsi.current_device;
    if (!dev) return -ENODEV;
    
    u8 madctl = 0;
    
    switch (orientation) {
        case 0:  /* 0 degrees */
            madctl = DSI_MADCTL_MX | DSI_MADCTL_MY | DSI_MADCTL_RGB;
            break;
        case 90:  /* 90 degrees */
            madctl = DSI_MADCTL_MV | DSI_MADCTL_MY | DSI_MADCTL_RGB;
            break;
        case 180:  /* 180 degrees */
            madctl = DSI_MADCTL_RGB;
            break;
        case 270:  /* 270 degrees */
            madctl = DSI_MADCTL_MV | DSI_MADCTL_MX | DSI_MADCTL_RGB;
            break;
    }
    
    dev->madctl = madctl;
    return dsi_cmd_set_madctl(madctl);
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int dsi_init(void)
{
    printk("[DSI] ========================================\n");
    printk("[DSI] NEXUS OS DSI Driver\n");
    printk("[DSI] ========================================\n");
    
    memset(&g_dsi, 0, sizeof(dsi_driver_t));
    spinlock_init(&g_dsi.lock);
    
    g_dsi.lanes = 4;
    g_dsi.bitrate = 1000;  /* 1 Gbps per lane */
    
    printk("[DSI] DSI driver initialized\n");
    return 0;
}

int dsi_shutdown(void)
{
    printk("[DSI] Shutting down DSI driver...\n");
    
    /* Turn off all devices */
    for (u32 i = 0; i < g_dsi.device_count; i++) {
        if (g_dsi.devices[i].is_enabled) {
            dsi_cmd_dispoff();
            dsi_cmd_slpin();
        }
    }
    
    g_dsi.device_count = 0;
    g_dsi.current_device = NULL;
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

dsi_driver_t *dsi_get_driver(void)
{
    return &g_dsi;
}

dsi_device_t *dsi_get_device(u32 device_id)
{
    for (u32 i = 0; i < g_dsi.device_count; i++) {
        if (g_dsi.devices[i].device_id == device_id) {
            return &g_dsi.devices[i];
        }
    }
    return NULL;
}

int dsi_list_devices(dsi_device_t *devices, u32 *count)
{
    if (!devices || !count) return -EINVAL;
    
    u32 copy = (*count < g_dsi.device_count) ? *count : g_dsi.device_count;
    memcpy(devices, g_dsi.devices, sizeof(dsi_device_t) * copy);
    *count = copy;
    
    return 0;
}
