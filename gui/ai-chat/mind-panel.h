/*
 * NEXUS OS - AI Internal Mind Panel
 * gui/ai-chat/mind-panel.h
 *
 * Shows AI reasoning, analysis, and thought processes
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _MIND_PANEL_H
#define _MIND_PANEL_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         MIND PANEL CONFIGURATION                          */
/*===========================================================================*/

#define MIND_PANEL_VERSION          "1.0.0"
#define MIND_PANEL_MAX_STEPS        64

/*===========================================================================*/
/*                         REASONING STEP TYPES                              */
/*===========================================================================*/

#define REASONING_STEP_ANALYSIS     0
#define REASONING_STEP_PLANNING     1
#define REASONING_STEP_EXECUTION    2
#define REASONING_STEP_VERIFICATION 3
#define REASONING_STEP_CONCLUSION   4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Reasoning Step
 */
typedef struct {
    u32 step_id;                    /* Step ID */
    u32 type;                       /* Step Type */
    char title[256];                /* Step Title */
    char content[2048];             /* Step Content */
    u32 confidence;                 /* Confidence (%) */
    u64 timestamp;                  /* Timestamp */
    u32 duration_ms;                /* Duration (ms) */
    bool is_complete;               /* Is Complete */
    bool is_visible;                /* Is Visible */
} reasoning_step_t;

/**
 * Mind Panel State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    bool collapsed;                 /* Is Collapsed */
    
    /* Window */
    window_t *panel_window;         /* Panel Window */
    widget_t *panel_widget;         /* Panel Widget */
    
    /* Reasoning */
    reasoning_step_t steps[MIND_PANEL_MAX_STEPS]; /* Steps */
    u32 step_count;                 /* Step Count */
    u32 current_step;               /* Current Step */
    bool is_thinking;               /* Is Thinking */
    
    /* Analysis */
    char query_analysis[1024];      /* Query Analysis */
    char intent[256];               /* Detected Intent */
    char entities[512];             /* Extracted Entities */
    char context[1024];             /* Context Used */
    
    /* Planning */
    char plan[2048];                /* Execution Plan */
    u32 plan_steps;                 /* Plan Steps */
    
    /* Confidence */
    u32 overall_confidence;         /* Overall Confidence */
    u32 accuracy_score;             /* Accuracy Score */
    
    /* Settings */
    bool auto_expand;               /* Auto Expand Steps */
    bool show_timestamps;           /* Show Timestamps */
    bool show_confidence;           /* Show Confidence */
    bool show_duration;             /* Show Duration */
    
    /* Callbacks */
    void (*on_step_expanded)(u32);
    void (*on_step_collapsed)(u32);
    void (*on_thinking_started)(void);
    void (*on_thinking_completed)(void);
    
    /* User Data */
    void *user_data;
    
} mind_panel_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Mind panel lifecycle */
int mind_panel_init(mind_panel_t *panel);
int mind_panel_show(mind_panel_t *panel);
int mind_panel_hide(mind_panel_t *panel);
int mind_panel_toggle(mind_panel_t *panel);
int mind_panel_collapse(mind_panel_t *panel);
int mind_panel_expand(mind_panel_t *panel);
int mind_panel_shutdown(mind_panel_t *panel);

/* Reasoning steps */
int mind_panel_add_step(mind_panel_t *panel, u32 type, const char *title, const char *content);
int mind_panel_update_step(mind_panel_t *panel, u32 step_id, const char *content);
int mind_panel_complete_step(mind_panel_t *panel, u32 step_id, u32 confidence);
int mind_panel_clear_steps(mind_panel_t *panel);
int mind_panel_expand_step(mind_panel_t *panel, u32 step_id);
int mind_panel_collapse_step(mind_panel_t *panel, u32 step_id);

/* Thinking process */
int mind_panel_start_thinking(mind_panel_t *panel);
int mind_panel_stop_thinking(mind_panel_t *panel);
int mind_panel_set_analysis(mind_panel_t *panel, const char *analysis);
int mind_panel_set_intent(mind_panel_t *panel, const char *intent);
int mind_panel_set_entities(mind_panel_t *panel, const char *entities);
int mind_panel_set_context(mind_panel_t *panel, const char *context);
int mind_panel_set_plan(mind_panel_t *panel, const char *plan);

/* Confidence */
int mind_panel_set_confidence(mind_panel_t *panel, u32 confidence);
int mind_panel_set_accuracy(mind_panel_t *panel, u32 accuracy);

/* Settings */
int mind_panel_set_auto_expand(mind_panel_t *panel, bool enabled);
int mind_panel_set_show_timestamps(mind_panel_t *panel, bool show);
int mind_panel_set_show_confidence(mind_panel_t *panel, bool show);
int mind_panel_set_show_duration(mind_panel_t *panel, bool show);

/* Utility functions */
const char *mind_panel_get_step_type_name(u32 type);
mind_panel_t *mind_panel_get_instance(void);

#endif /* _MIND_PANEL_H */
