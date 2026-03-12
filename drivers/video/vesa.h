/*
 * NEXUS OS - VESA/VGA Graphics Driver
 * drivers/video/vesa.h
 *
 * VESA VBE and VGA graphics driver for GUI support
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _VESA_H
#define _VESA_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         VESA CONFIGURATION                                */
/*===========================================================================*/

#define VESA_MAX_MODES              128
#define VESA_MAX_BANKS              16
#define VESA_BUFFER_SIZE            (16 * 1024 * 1024)

/*===========================================================================*/
/*                         VESA CONSTANTS                                    */
/*===========================================================================*/

#define VESA_SIGNATURE              0x02534542  /* "VBE2" */
#define VESA_MODE_SUPPORTED         0x0001
#define VESA_MODE_TTY_SUPPORTED     0x0004
#define VESA_MODE_GRAPHICS          0x0010
#define VESA_MODE_COLOR             0x0008
#define VESA_MODE_LINEAR            0x0080

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * VESA Info Block
 */
typedef struct __packed {
    u8 signature[4];                /* "VBE2" */
    u16 version;                    /* VESA Version */
    u32 oem_string;                 /* OEM String */
    u32 capabilities;               /* Capabilities */
    u32 video_mode;                 /* Video Mode List */
    u16 total_memory;               /* Total Memory (64KB blocks) */
    u16 oem_software_rev;           /* OEM Software Revision */
    u32 oem_vendor_name;            /* OEM Vendor Name */
    u32 oem_product_name;           /* OEM Product Name */
    u32 oem_product_rev;            /* OEM Product Revision */
    u8 reserved[222];               /* Reserved */
    u8 oem_data[256];               /* OEM Data */
} vesa_info_t;

/**
 * VESA Mode Info
 */
typedef struct __packed {
    u16 mode_attributes;            /* Mode Attributes */
    u8 win_a_attributes;            /* Window A Attributes */
    u8 win_b_attributes;            /* Window B Attributes */
    u16 win_granularity;            /* Window Granularity */
    u16 win_size;                   /* Window Size */
    u16 win_a_start;                /* Window A Start */
    u16 win_b_start;                /* Window B Start */
    u32 win_func_ptr;               /* Window Function Pointer */
    u16 bytes_per_scan_line;        /* Bytes Per Scan Line */
    u16 x_resolution;               /* X Resolution */
    u16 y_resolution;               /* Y Resolution */
    u8 x_char_size;                 /* X Character Size */
    u8 y_char_size;                 /* Y Character Size */
    u8 number_of_planes;            /* Number Of Planes */
    u8 bits_per_pixel;              /* Bits Per Pixel */
    u8 number_of_banks;             /* Number Of Banks */
    u8 memory_model;                /* Memory Model */
    u8 bank_size;                   /* Bank Size */
    u8 number_of_image_pages;       /* Number Of Image Pages */
    u8 reserved0;                   /* Reserved */
    u8 red_mask_size;               /* Red Mask Size */
    u8 red_field_position;          /* Red Field Position */
    u8 green_mask_size;             /* Green Mask Size */
    u8 green_field_position;        /* Green Field Position */
    u8 blue_mask_size;              /* Blue Mask Size */
    u8 blue_field_position;         /* Blue Field Position */
    u8 reserved_mask_size;          /* Reserved Mask Size */
    u8 reserved_field_position;     /* Reserved Field Position */
    u8 direct_color_mode_info;      /* Direct Color Mode Info */
    u32 phys_base_ptr;              /* Physical Base Pointer */
    u32 offscreen_mem_offset;       /* Offscreen Memory Offset */
    u16 offscreen_mem_size;         /* Offscreen Memory Size */
    u8 reserved1[206];              /* Reserved */
} vesa_mode_info_t;

/**
 * VGA Register Set
 */
typedef struct {
    u8 misc_output;                 /* Misc Output Register */
    u8 seq_clock_mode;              /* Sequencer Clock Mode */
    u8 seq_memory_mode;             /* Sequencer Memory Mode */
    u8 crtc_horiz_total;            /* CRTC Horiz Total */
    u8 crtc_horiz_display;          /* CRTC Horiz Display End */
    u8 crtc_horiz_blank_start;      /* CRTC Horiz Blank Start */
    u8 crtc_horiz_blank_end;        /* CRTC Horiz Blank End */
    u8 crtc_horiz_retrace_start;    /* CRTC Horiz Retrace Start */
    u8 crtc_horiz_retrace_end;      /* CRTC Horiz Retrace End */
    u8 crtc_vert_total;             /* CRTC Vert Total */
    u8 crtc_overflow;               /* CRTC Overflow */
    u8 crtc_max_scan_line;          /* CRTC Max Scan Line */
    u8 crtc_cursor_start;           /* CRTC Cursor Start */
    u8 crtc_cursor_end;             /* CRTC Cursor End */
    u8 crtc_start_address_high;     /* CRTC Start Address High */
    u8 crtc_start_address_low;      /* CRTC Start Address Low */
    u8 crtc_cursor_location_high;   /* CRTC Cursor Location High */
    u8 crtc_cursor_location_low;    /* CRTC Cursor Location Low */
    u8 crtc_vert_retrace_start;     /* CRTC Vert Retrace Start */
    u8 crtc_vert_retrace_end;       /* CRTC Vert Retrace End */
    u8 crtc_vert_display_end;       /* CRTC Vert Display End */
    u8 crtc_offset;                 /* CRTC Offset */
    u8 crtc_underline_location;     /* CRTC Underline Location */
    u8 crtc_vert_blank_start;       /* CRTC Vert Blank Start */
    u8 crtc_vert_blank_end;         /* CRTC Vert Blank End */
    u8 crtc_mode_control;           /* CRTC Mode Control */
    u8 crtc_line_compare;           /* CRTC Line Compare */
    u8 gr_set_reset;                /* Graphics Set/Reset */
    u8 gr_enable_set_reset;         /* Graphics Enable Set/Reset */
    u8 gr_color_compare;            /* Graphics Color Compare */
    u8 gr_data_rotate;              /* Graphics Data Rotate */
    u8 gr_read_map_select;          /* Graphics Read Map Select */
    u8 gr_mode;                     /* Graphics Mode */
    u8 gr_miscellaneous;            /* Graphics Miscellaneous */
    u8 gr_color_dont_care;          /* Graphics Color Don't Care */
    u8 gr_bit_mask;                 /* Graphics Bit Mask */
    u8 ac_mode_control;             /* Attribute Controller Mode Control */
    u8 ac_overscan_color;           /* Attribute Controller Overscan Color */
    u8 ac_color_plane_enable;       /* Attribute Controller Color Plane Enable */
    u8 ac_horizontal_pixel_shift;   /* Attribute Controller Horizontal Pixel Shift */
    u8 ac_line_compare;             /* Attribute Controller Line Compare */
} vga_registers_t;

/**
 * Framebuffer Info
 */
typedef struct {
    void *base;                     /* Base Address */
    phys_addr_t phys_base;          /* Physical Base Address */
    u32 size;                       /* Size */
    u32 width;                      /* Width */
    u32 height;                     /* Height */
    u32 stride;                     /* Stride */
    u32 bpp;                        /* Bits Per Pixel */
    u32 format;                     /* Pixel Format */
    bool is_linear;                 /* Is Linear */
} framebuffer_info_t;

/**
 * Video Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    vesa_info_t vesa_info;          /* VESA Info */
    vesa_mode_info_t modes[VESA_MAX_MODES]; /* Mode Info */
    u32 mode_count;                 /* Mode Count */
    u32 current_mode;               /* Current Mode */
    u16 original_mode;              /* Original Video Mode */
    framebuffer_info_t framebuffer; /* Framebuffer Info */
    vga_registers_t saved_regs;     /* Saved VGA Registers */
    bool is_vesa;                   /* Is VESA */
    bool is_vga;                    /* Is VGA */
    u32 video_memory;               /* Video Memory */
    void (*set_pixel)(u32 x, u32 y, u32 color);
    u32 (*get_pixel)(u32 x, u32 y);
    void (*fill_rect)(u32 x, u32 y, u32 w, u32 h, u32 color);
    void (*blit)(void *src, u32 x, u32 y, u32 w, u32 h);
} video_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int video_init(void);
int video_shutdown(void);
bool video_is_initialized(void);

/* VESA functions */
int vesa_get_info(vesa_info_t *info);
int vesa_get_mode_info(u16 mode, vesa_mode_info_t *info);
int vesa_set_mode(u16 mode);
int vesa_get_current_mode(void);
int vesa_list_modes(vesa_mode_info_t *modes, u32 *count);

/* VGA functions */
int vga_save_state(vga_registers_t *regs);
int vga_restore_state(vga_registers_t *regs);
int vga_set_palette(u8 index, u8 r, u8 g, u8 b);
int vga_get_palette(u8 index, u8 *r, u8 *g, u8 *b);
int vga_clear_screen(u8 color);

/* Framebuffer functions */
framebuffer_info_t *video_get_framebuffer(void);
int video_set_pixel(u32 x, u32 y, u32 color);
u32 video_get_pixel(u32 x, u32 y);
int video_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
int video_draw_line(u32 x0, u32 y0, u32 x1, u32 y1, u32 color);
int video_blit(void *src, u32 x, u32 y, u32 w, u32 h);
int video_scroll(s32 dx, s32 dy);

/* Mode setting */
int video_set_mode(u32 width, u32 height, u32 bpp);
int video_get_best_mode(u32 width, u32 height, u32 bpp);
int video_restore_mode(void);

/* Memory management */
void *video_alloc_memory(u32 size);
void video_free_memory(void *ptr);
int video_map_memory(void);
int video_unmap_memory(void);

/* Acceleration */
int video_enable_acceleration(bool enable);
bool video_has_acceleration(void);
int video_wait_vsync(void);
int video_set_vsync(bool enable);

/* Utility functions */
const char *video_get_driver_name(void);
u32 video_get_memory_size(void);
u32 video_pack_color(u8 r, u8 g, u8 b);
void video_unpack_color(u32 color, u8 *r, u8 *g, u8 *b);

/* Global instance */
video_driver_t *video_get_driver(void);

#endif /* _VESA_H */
