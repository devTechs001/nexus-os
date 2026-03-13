/*
 * NEXUS OS - AI Vision Analysis View
 * gui/ai-chat/vision-analysis.h
 *
 * Displays screenshot captures with AI interpretation results
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _VISION_ANALYSIS_H
#define _VISION_ANALYSIS_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         VISION CONFIGURATION                              */
/*===========================================================================*/

#define VISION_ANALYSIS_VERSION     "1.0.0"
#define VISION_MAX_OBJECTS          128
#define VISION_MAX_TEXT_REGIONS     64

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Detected Object
 */
typedef struct {
    u32 object_id;                  /* Object ID */
    char name[128];                 /* Object Name */
    f32 confidence;                 /* Confidence */
    rect_t bounding_box;            /* Bounding Box */
    char category[64];              /* Category */
} detected_object_t;

/**
 * Text Region (OCR)
 */
typedef struct {
    u32 region_id;                  /* Region ID */
    char text[512];                 /* Text Content */
    f32 confidence;                 /* Confidence */
    rect_t bounding_box;            /* Bounding Box */
    char language[32];              /* Language */
} text_region_t;

/**
 * Vision Analysis Result
 */
typedef struct {
    char image_path[512];           /* Image Path */
    u32 width;                      /* Image Width */
    u32 height;                     /* Image Height */
    
    /* Objects */
    detected_object_t objects[VISION_MAX_OBJECTS]; /* Objects */
    u32 object_count;               /* Object Count */
    
    /* Text */
    text_region_t text_regions[VISION_MAX_TEXT_REGIONS]; /* Text Regions */
    u32 text_region_count;          /* Text Region Count */
    char full_text[4096];           /* Full Extracted Text */
    
    /* Analysis */
    char description[2048];         /* Image Description */
    char scene_type[128];           /* Scene Type */
    char colors[256];               /* Dominant Colors */
    char tags[512];                 /* Tags */
    
    /* Confidence */
    f32 overall_confidence;         /* Overall Confidence */
    
    /* Timestamp */
    u64 analysis_time;              /* Analysis Time */
} vision_result_t;

/**
 * Vision Analysis View State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *view_window;          /* View Window */
    widget_t *view_widget;          /* View Widget */
    
    /* Image */
    widget_t *image_widget;         /* Image Widget */
    char current_image[512];        /* Current Image Path */
    
    /* Results */
    vision_result_t result;         /* Analysis Result */
    bool has_result;                /* Has Result */
    
    /* Overlay */
    bool show_bounding_boxes;       /* Show Bounding Boxes */
    bool show_labels;               /* Show Labels */
    bool show_confidence;           /* Show Confidence */
    bool show_text_regions;         /* Show Text Regions */
    
    /* Settings */
    f32 box_thickness;              /* Box Thickness */
    color_t box_color;              /* Box Color */
    u32 font_size;                  /* Font Size */
    
    /* Callbacks */
    void (*on_image_loaded)(const char *);
    void (*on_analysis_started)(void);
    void (*on_analysis_completed)(vision_result_t *);
    void (*on_object_clicked)(detected_object_t *);
    
    /* User Data */
    void *user_data;
    
} vision_analysis_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Vision lifecycle */
int vision_analysis_init(vision_analysis_t *vision);
int vision_analysis_show(vision_analysis_t *vision);
int vision_analysis_hide(vision_analysis_t *vision);
int vision_analysis_toggle(vision_analysis_t *vision);
int vision_analysis_shutdown(vision_analysis_t *vision);

/* Image operations */
int vision_analysis_load_image(vision_analysis_t *vision, const char *path);
int vision_analysis_capture_screen(vision_analysis_t *vision);
int vision_analysis_clear_image(vision_analysis_t *vision);

/* Analysis */
int vision_analysis_analyze(vision_analysis_t *vision);
int vision_analysis_get_result(vision_analysis_t *vision, vision_result_t *result);
int vision_analysis_cancel_analysis(vision_analysis_t *vision);

/* Object detection */
int vision_analysis_get_objects(vision_analysis_t *vision, detected_object_t *objects, u32 *count);
int vision_analysis_get_object(vision_analysis_t *vision, u32 object_id, detected_object_t *object);

/* OCR */
int vision_analysis_get_text(vision_analysis_t *vision, text_region_t *regions, u32 *count);
int vision_analysis_get_full_text(vision_analysis_t *vision, char *text, u32 size);

/* Overlay */
int vision_analysis_toggle_bounding_boxes(vision_analysis_t *vision, bool show);
int vision_analysis_toggle_labels(vision_analysis_t *vision, bool show);
int vision_analysis_toggle_confidence(vision_analysis_t *vision, bool show);
int vision_analysis_toggle_text_regions(vision_analysis_t *vision, bool show);
int vision_analysis_set_box_color(vision_analysis_t *vision, color_t color);
int vision_analysis_set_box_thickness(vision_analysis_t *vision, f32 thickness);

/* Export */
int vision_analysis_export_result(vision_analysis_t *vision, const char *path, const char *format);
int vision_analysis_export_image(vision_analysis_t *vision, const char *path);

/* Utility functions */
vision_analysis_t *vision_analysis_get_instance(void);

#endif /* _VISION_ANALYSIS_H */
