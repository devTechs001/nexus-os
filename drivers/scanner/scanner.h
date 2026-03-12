/*
 * NEXUS OS - Scanner Driver
 * drivers/scanner/scanner.h
 *
 * Scanner driver with SANE support
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _SCANNER_H
#define _SCANNER_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         SCANNER CONFIGURATION                             */
/*===========================================================================*/

#define SCANNER_MAX_DEVICES           16
#define SCANNER_MAX_RESOLUTION        9600
#define SCANNER_MAX_WIDTH_MM          216
#define SCANNER_MAX_HEIGHT_MM         297
#define SCANNER_MAX_BUFFER            (32 * 1024 * 1024)

/*===========================================================================*/
/*                         SCANNER TYPES                                     */
/*===========================================================================*/

#define SCANNER_TYPE_FLATBED          0
#define SCANNER_TYPE_SHEETFED         1
#define SCANNER_TYPE_HANDHELD         2
#define SCANNER_TYPE_DRUM             3
#define SCANNER_TYPE_FILM             4

/*===========================================================================*/
/*                         SCAN MODES                                        */
/*===========================================================================*/

#define SCAN_MODE_COLOR               0
#define SCAN_MODE_GRAYSCALE           1
#define SCAN_MODE_BLACKWHITE          2
#define SCAN_MODE_NEGATIVE            3
#define SCAN_MODE_SLIDE               4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Scanner Device Info
 */
typedef struct {
    u32 device_id;                    /* Device ID */
    char name[128];                   /* Device Name */
    char vendor[64];                  /* Vendor */
    char model[64];                   /* Model */
    u32 type;                         /* Scanner Type */
    u32 status;                       /* Status */
    u32 max_optical_resolution;       /* Max Optical Resolution (DPI) */
    u32 max_interpolated_resolution;  /* Max Interpolated Resolution */
    u32 bit_depth;                    /* Bit Depth */
    u32 max_width_mm;                 /* Max Scan Width (mm) */
    u32 max_height_mm;                /* Max Scan Height (mm) */
    bool has_adf;                     /* Has Auto Document Feeder */
    bool has_duplex;                  /* Has Duplex Scanning */
    bool has_transparency;            /* Has Transparency Unit */
    bool is_connected;                /* Is Connected */
    char device_path[256];            /* Device Path */
    void *driver_data;                /* Driver Data */
} scanner_device_t;

/**
 * Scan Settings
 */
typedef struct {
    u32 resolution;                   /* Resolution (DPI) */
    u32 mode;                         /* Scan Mode */
    u32 brightness;                   /* Brightness (0-100) */
    u32 contrast;                     /* Contrast (0-100) */
    u32 saturation;                   /* Saturation (0-100) */
    u32 gamma;                        /* Gamma (100-300) */
    u32 area_x;                       /* Scan Area X (mm) */
    u32 area_y;                       /* Scan Area Y (mm) */
    u32 area_width;                   /* Scan Area Width (mm) */
    u32 area_height;                  /* Scan Area Height (mm) */
    bool use_adf;                     /* Use ADF */
    bool use_duplex;                  /* Use Duplex */
    u32 page_count;                   /* Number of Pages */
    const char *output_format;        /* Output Format (png, jpg, pdf, tiff) */
    const char *output_path;          /* Output Path */
} scan_settings_t;

/**
 * Scan Progress
 */
typedef struct {
    u32 current_page;                 /* Current Page */
    u32 total_pages;                  /* Total Pages */
    u32 current_line;                 /* Current Scan Line */
    u32 total_lines;                  /* Total Lines */
    u32 percent_complete;             /* Percent Complete */
    u32 bytes_written;                /* Bytes Written */
    u32 estimated_time_remaining;     /* Estimated Time Remaining (seconds) */
    const char *status_message;       /* Status Message */
} scan_progress_t;

/**
 * Scanner Driver State
 */
typedef struct {
    bool initialized;                 /* Is Initialized */
    scanner_device_t devices[SCANNER_MAX_DEVICES]; /* Devices */
    u32 device_count;                 /* Device Count */
    u32 active_device;                /* Active Device */
    scan_settings_t current_settings; /* Current Settings */
    scan_progress_t progress;         /* Scan Progress */
    bool is_scanning;                 /* Is Scanning */
    bool is_calibrating;              /* Is Calibrating */
    void *scan_buffer;                /* Scan Buffer */
    u32 scan_buffer_size;             /* Scan Buffer Size */
    void (*on_scan_complete)(void *data, u32 size);
    void (*on_scan_progress)(scan_progress_t *progress);
    void (*on_scan_error)(const char *error);
    void *user_data;                  /* User Data */
} scanner_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int scanner_init(void);
int scanner_shutdown(void);
bool scanner_is_initialized(void);

/* Device management */
int scanner_enumerate_devices(void);
scanner_device_t *scanner_get_device(u32 device_id);
scanner_device_t *scanner_get_active_device(void);
int scanner_set_active_device(u32 device_id);
int scanner_list_devices(scanner_device_t *devices, u32 *count);

/* Scanner control */
int scanner_open(u32 device_id);
int scanner_close(u32 device_id);
int scanner_warmup(u32 device_id);
int scanner_calibrate(u32 device_id);
int scanner_self_test(u32 device_id);

/* Scan operations */
int scanner_scan(u32 device_id, scan_settings_t *settings);
int scanner_scan_preview(u32 device_id);
int scanner_scan_to_file(u32 device_id, scan_settings_t *settings, const char *path);
int scanner_cancel_scan(void);
int scanner_get_scan_data(void **data, u32 *size);

/* Settings */
int scanner_set_resolution(u32 device_id, u32 dpi);
int scanner_set_mode(u32 device_id, u32 mode);
int scanner_set_brightness(u32 device_id, u32 brightness);
int scanner_set_contrast(u32 device_id, u32 contrast);
int scanner_set_gamma(u32 device_id, u32 gamma);
int scanner_set_scan_area(u32 device_id, u32 x, u32 y, u32 width, u32 height);
int scanner_get_capabilities(u32 device_id, void *caps);

/* Progress */
int scanner_get_progress(scan_progress_t *progress);
int scanner_set_progress_callback(void (*callback)(scan_progress_t *));

/* Utility functions */
const char *scanner_get_type_name(u32 type);
const char *scanner_get_mode_name(u32 mode);
const char *scanner_get_status_name(u32 status);

/* Global instance */
scanner_driver_t *scanner_get_driver(void);

#endif /* _SCANNER_H */
