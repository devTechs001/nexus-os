/*
 * NEXUS OS - Display Manager
 * drivers/display/display_manager.c
 *
 * Display mode setting, hotplug detection, EDID parsing
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "display.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"

/*===========================================================================*/
/*                         DISPLAY MANAGER CONFIGURATION                     */
/*===========================================================================*/

#define DM_MAX_DISPLAYS             16
#define DM_MAX_MODES                256
#define DM_MAX_TIMINGS              128
#define EDID_BUFFER_SIZE            256

/*===========================================================================*/
/*                         EDID STRUCTURES                                   */
/*===========================================================================*/

typedef struct __packed {
    u8 header[8];                   /* 00 FF FF FF FF FF FF 00 */
    u16 vendor_id;                  /* Vendor ID */
    u16 product_id;                 /* Product ID */
    u32 serial;                     /* Serial Number */
    u8 week;                        /* Week of manufacture */
    u8 year;                        /* Year of manufacture */
    u8 version;                     /* EDID Version */
    u8 revision;                    /* EDID Revision */
    u8 video_input;                 /* Video Input Definition */
    u8 screen_width;                /* Max horizontal size (cm) */
    u8 screen_height;               /* Max vertical size (cm) */
    u8 gamma;                       /* Gamma */
    u8 features;                    /* Feature Support */
    u8 red_low;                     /* Red X LSB */
    u8 red_high;                    /* Red X MSB */
    u8 green_low;                   /* Green Y LSB */
    u8 green_high;                  /* Green Y MSB */
    u8 blue_low;                    /* Blue X LSB */
    u8 blue_high;                   /* Blue X MSB */
    u8 white_low;                   /* White X LSB */
    u8 white_high;                  /* White X MSB */
    u8 established_timings[3];      /* Established Timings */
    u8 standard_timings[16];        /* Standard Timings (8 bytes each) */
    u8 detailed_timings[72];        /* Detailed Timing Descriptors */
    u8 extensions;                  /* Number of extensions */
    u8 checksum;                    /* Checksum */
} edid_block_t;

typedef struct {
    u32 pixel_clock;                /* kHz */
    u16 h_active;
    u16 h_blanking;
    u16 h_sync_offset;
    u16 h_sync_width;
    u16 v_active;
    u16 v_blanking;
    u16 v_sync_offset;
    u16 v_sync_width;
    u32 h_freq;                     /* Hz */
    u32 v_freq;                     /* Hz */
    char name[32];
} display_timing_t;

/*===========================================================================*/
/*                         DISPLAY MANAGER STATE                             */
/*===========================================================================*/

typedef struct {
    bool initialized;
    display_adapter_t adapters[DM_MAX_DISPLAYS];
    u32 adapter_count;
    display_mode_t modes[DM_MAX_MODES];
    u32 mode_count;
    display_timing_t timings[DM_MAX_TIMINGS];
    u32 timing_count;
    edid_block_t edid_blocks[DM_MAX_DISPLAYS];
    bool hotplug_enabled;
    void (*on_hotplug)(u32 connector_id, bool connected);
    u32 last_hotplug_state;
} display_manager_t;

static display_manager_t g_dm;

/*===========================================================================*/
/*                         EDID PARSING                                      */
/*===========================================================================*/

static u8 edid_checksum(const u8 *data)
{
    u8 sum = 0;
    for (int i = 0; i < 128; i++) {
        sum += data[i];
    }
    return sum;
}

static int parse_edid(const u8 *edid, display_mode_t *modes, u32 *mode_count)
{
    if (!edid || !modes || !mode_count) return -EINVAL;
    
    edid_block_t *block = (edid_block_t *)edid;
    
    /* Validate EDID */
    if (edid_checksum(edid) != 0) {
        printk("[DM] Invalid EDID checksum\n");
        return -EINVAL;
    }
    
    /* Check header */
    if (edid[0] != 0x00 || edid[1] != 0xFF || edid[2] != 0xFF ||
        edid[3] != 0xFF || edid[4] != 0xFF || edid[5] != 0xFF ||
        edid[6] != 0xFF || edid[7] != 0x00) {
        printk("[DM] Invalid EDID header\n");
        return -EINVAL;
    }
    
    *mode_count = 0;
    
    /* Parse established timings */
    if (block->established_timings[0] & 0x80) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 720, .vdisplay = 400, .vrefresh = 70,
            .name = "720x400@70"
        };
    }
    if (block->established_timings[0] & 0x40) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 720, .vdisplay = 400, .vrefresh = 88,
            .name = "720x400@88"
        };
    }
    if (block->established_timings[0] & 0x20) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 640, .vdisplay = 480, .vrefresh = 60,
            .name = "640x480@60"
        };
    }
    if (block->established_timings[0] & 0x10) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 640, .vdisplay = 480, .vrefresh = 67,
            .name = "640x480@67"
        };
    }
    if (block->established_timings[0] & 0x08) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 640, .vdisplay = 480, .vrefresh = 72,
            .name = "640x480@72"
        };
    }
    if (block->established_timings[0] & 0x04) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 640, .vdisplay = 480, .vrefresh = 75,
            .name = "640x480@75"
        };
    }
    if (block->established_timings[0] & 0x02) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 800, .vdisplay = 600, .vrefresh = 56,
            .name = "800x600@56"
        };
    }
    if (block->established_timings[0] & 0x01) {
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = 800, .vdisplay = 600, .vrefresh = 60,
            .name = "800x600@60"
        };
    }
    
    /* Parse standard timings */
    for (int i = 0; i < 8 && block->standard_timings[i * 2] != 0; i++) {
        u8 h = block->standard_timings[i * 2];
        u8 v = block->standard_timings[i * 2 + 1];
        
        if (h == 0x01 && v == 0x01) continue;
        
        u16 width = (h + 31) * 8;
        u16 height;
        
        switch ((v >> 6) & 0x03) {
            case 0: height = width * 16 / 10; break;  /* 16:10 */
            case 1: height = width * 4 / 3; break;    /* 4:3 */
            case 2: height = width * 5 / 4; break;    /* 5:4 */
            case 3: height = width * 16 / 9; break;   /* 16:9 */
            default: height = width;
        }
        
        u8 refresh = (v & 0x3F) + 60;
        
        modes[(*mode_count)++] = (display_mode_t){
            .hdisplay = width,
            .vdisplay = height,
            .vrefresh = refresh,
            .is_preferred = (i == 0)
        };
    }
    
    return 0;
}

int dm_read_edid(u32 connector_id, u8 *edid, u32 *size)
{
    if (!edid || !size) return -EINVAL;
    if (*size < EDID_BUFFER_SIZE) return -ENOMEM;
    
    /* In real implementation, would read via DDC/I2C */
    /* Mock EDID data for testing */
    u8 mock_edid[128] = {
        0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
        0x06, 0xB3, 0x44, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x28, 0x1E, 0x01, 0x03, 0x80, 0x30, 0x1B, 0x78,
        0x2E, 0xEE, 0x95, 0xA3, 0x54, 0x4C, 0x99, 0x26,
        0x0F, 0x50, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
        0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
        0x45, 0x00, 0x0F, 0x28, 0x21, 0x00, 0x00, 0x1E,
        0x00, 0x00, 0x00, 0xFC, 0x00, 0x50, 0x32, 0x34,
        0x36, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00,
        0x00, 0x00, 0xFD, 0x00, 0x30, 0x4C, 0x1E, 0x53,
        0x11, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x01, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97
    };
    
    memcpy(edid, mock_edid, 128);
    *size = 128;
    
    /* Store EDID */
    if (connector_id < DM_MAX_DISPLAYS) {
        memcpy(&g_dm.edid_blocks[connector_id], edid, 128);
    }
    
    return 0;
}

int dm_parse_edid(u32 connector_id, display_mode_t *modes, u32 *mode_count)
{
    u8 edid[128];
    u32 size = 128;
    
    int ret = dm_read_edid(connector_id, edid, &size);
    if (ret < 0) return ret;
    
    return parse_edid(edid, modes, mode_count);
}

/*===========================================================================*/
/*                         HOTPLUG DETECTION                                 */
/*===========================================================================*/

static void check_hotplug(void)
{
    u32 current_state = 0;
    
    /* Check each connector for changes */
    for (u32 i = 0; i < g_dm.adapter_count; i++) {
        display_adapter_t *adapter = &g_dm.adapters[i];
        
        /* In real implementation, would check HPD (Hot Plug Detect) pin */
        /* Mock: assume all adapters are connected */
        if (adapter->is_active) {
            current_state |= (1 << i);
        }
    }
    
    /* Detect changes */
    u32 changed = current_state ^ g_dm.last_hotplug_state;
    
    if (changed && g_dm.on_hotplug) {
        for (u32 i = 0; i < g_dm.adapter_count; i++) {
            if (changed & (1 << i)) {
                bool connected = (current_state & (1 << i)) != 0;
                g_dm.on_hotplug(i, connected);
                printk("[DM] Hotplug: Connector %d %s\n", i, 
                       connected ? "connected" : "disconnected");
            }
        }
    }
    
    g_dm.last_hotplug_state = current_state;
}

int dm_enable_hotplug(void (*callback)(u32, bool))
{
    g_dm.on_hotplug = callback;
    g_dm.hotplug_enabled = true;
    printk("[DM] Hotplug detection enabled\n");
    return 0;
}

int dm_disable_hotplug(void)
{
    g_dm.hotplug_enabled = false;
    g_dm.on_hotplug = NULL;
    return 0;
}

/*===========================================================================*/
/*                         RESOLUTION HANDLER                                */
/*===========================================================================*/

int dm_get_supported_resolutions(display_mode_t *modes, u32 *count)
{
    if (!modes || !count) return -EINVAL;
    
    /* Common resolutions */
    display_mode_t common_modes[] = {
        {0, 0, 0, 640, 480, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "640x480@60", true, false, false},
        {0, 0, 0, 800, 600, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "800x600@60", false, false, false},
        {0, 0, 0, 1024, 768, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1024x768@60", false, false, false},
        {0, 0, 0, 1280, 720, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1280x720@60", false, false, false},
        {0, 0, 0, 1280, 960, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1280x960@60", false, false, false},
        {0, 0, 0, 1280, 1024, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1280x1024@60", false, false, false},
        {0, 0, 0, 1366, 768, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1366x768@60", false, false, false},
        {0, 0, 0, 1440, 900, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1440x900@60", false, false, false},
        {0, 0, 0, 1600, 900, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1600x900@60", false, false, false},
        {0, 0, 0, 1680, 1050, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1680x1050@60", false, false, false},
        {0, 0, 0, 1920, 1080, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1920x1080@60", false, false, false},
        {0, 0, 0, 1920, 1080, 120, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1920x1080@120", false, false, false},
        {0, 0, 0, 1920, 1200, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "1920x1200@60", false, false, false},
        {0, 0, 0, 2560, 1080, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "2560x1080@60", false, false, false},
        {0, 0, 0, 2560, 1440, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "2560x1440@60", false, false, false},
        {0, 0, 0, 2560, 1440, 144, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "2560x1440@144", false, false, false},
        {0, 0, 0, 3440, 1440, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "3440x1440@60", false, false, false},
        {0, 0, 0, 3840, 2160, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "3840x2160@60", false, false, false},
        {0, 0, 0, 3840, 2160, 120, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "3840x2160@120", false, false, false},
        {0, 0, 0, 7680, 4320, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "7680x4320@60", false, false, false},
    };
    
    u32 copy_count = (*count < 20) ? *count : 20;
    memcpy(modes, common_modes, sizeof(display_mode_t) * copy_count);
    *count = copy_count;
    
    return 0;
}

int dm_set_resolution(u32 adapter_id, u32 width, u32 height, u32 refresh)
{
    display_adapter_t *adapter = display_get_adapter(adapter_id);
    if (!adapter) return -ENOENT;
    
    printk("[DM] Setting resolution: %dx%d@%dHz\n", width, height, refresh);
    
    /* Find matching mode */
    display_mode_t modes[32];
    u32 count = 32;
    
    int ret = dm_get_supported_resolutions(modes, &count);
    if (ret < 0) return ret;
    
    for (u32 i = 0; i < count; i++) {
        if (modes[i].hdisplay == width && modes[i].vdisplay == height &&
            modes[i].vrefresh == refresh) {
            return display_set_mode(0, adapter_id, &modes[i]);
        }
    }
    
    return -EINVAL;  /* Mode not found */
}

int dm_get_current_resolution(u32 adapter_id, u32 *width, u32 *height, u32 *refresh)
{
    display_adapter_t *adapter = display_get_adapter(adapter_id);
    if (!adapter || !width || !height || !refresh) return -EINVAL;
    
    /* Get from current mode */
    display_mode_t mode;
    if (display_get_current_mode(0, &mode) == 0) {
        *width = mode.hdisplay;
        *height = mode.vdisplay;
        *refresh = mode.vrefresh;
        return 0;
    }
    
    /* Default */
    *width = 1920;
    *height = 1080;
    *refresh = 60;
    
    return 0;
}

/*===========================================================================*/
/*                         DISPLAY TIMING                                    */
/*===========================================================================*/

int dm_calculate_timing(u32 width, u32 height, u32 refresh, display_timing_t *timing)
{
    if (!timing) return -EINVAL;
    
    /* CVT-RB (Coordinated Video Timing - Reduced Blanking) */
    u32 h_blank = 160;
    u32 v_blank = 3;
    u32 h_sync = 32;
    u32 v_sync = 5;
    
    timing->h_active = width;
    timing->v_active = height;
    timing->h_blanking = h_blank;
    timing->v_blanking = v_blank;
    timing->h_sync_offset = 8;
    timing->h_sync_width = h_sync;
    timing->v_sync_offset = 3;
    timing->v_sync_width = v_sync;
    
    timing->h_freq = refresh * (height + v_blank) * (width + h_blank) / 1000;
    timing->v_freq = refresh;
    timing->pixel_clock = timing->h_freq * (width + h_blank) / 1000;
    
    snprintf(timing->name, sizeof(timing->name), "%dx%d@%d", 
             width, height, refresh);
    
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int dm_init(void)
{
    printk("[DM] Initializing Display Manager...\n");
    
    memset(&g_dm, 0, sizeof(display_manager_t));
    g_dm.initialized = true;
    
    /* Copy adapters from display driver */
    display_driver_t *disp = display_get_driver();
    if (disp) {
        g_dm.adapter_count = disp->adapter_count;
        for (u32 i = 0; i < disp->adapter_count; i++) {
            g_dm.adapters[i] = disp->adapters[i];
        }
    }
    
    /* Generate common timings */
    dm_calculate_timing(1920, 1080, 60, &g_dm.timings[g_dm.timing_count++]);
    dm_calculate_timing(2560, 1440, 60, &g_dm.timings[g_dm.timing_count++]);
    dm_calculate_timing(3840, 2160, 60, &g_dm.timings[g_dm.timing_count++]);
    
    printk("[DM] Display Manager initialized (%d adapters)\n", g_dm.adapter_count);
    return 0;
}

int dm_shutdown(void)
{
    printk("[DM] Shutting down Display Manager...\n");
    g_dm.initialized = false;
    return 0;
}

display_manager_t *dm_get_manager(void)
{
    return &g_dm;
}
