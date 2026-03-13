/*
 * NEXUS OS - AI Model Status Panel
 * gui/ai-chat/model-status.h
 *
 * Ollama server connection status and active model indicators
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _MODEL_STATUS_H
#define _MODEL_STATUS_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         MODEL STATUS CONFIGURATION                        */
/*===========================================================================*/

#define MODEL_STATUS_VERSION        "1.0.0"
#define MODEL_STATUS_MAX_MODELS     64
#define MODEL_STATUS_MAX_SERVERS    8

/*===========================================================================*/
/*                         SERVER STATUS                                     */
/*===========================================================================*/

#define SERVER_STATUS_OFFLINE       0
#define SERVER_STATUS_CONNECTING    1
#define SERVER_STATUS_ONLINE        2
#define SERVER_STATUS_ERROR         3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Model Information
 */
typedef struct {
    char name[128];                 /* Model Name */
    char size[32];                  /* Model Size */
    char format[32];                /* Model Format */
    char family[64];                /* Model Family */
    u32 parameters;                 /* Parameters (billions) */
    u32 quantization;               /* Quantization Level */
    u64 download_size;              /* Download Size */
    bool is_downloaded;             /* Is Downloaded */
    bool is_active;                 /* Is Active */
    u64 last_used;                  /* Last Used */
    u32 use_count;                  /* Use Count */
} model_info_t;

/**
 * Server Information
 */
typedef struct {
    char name[128];                 /* Server Name */
    char url[256];                  /* Server URL */
    u32 status;                     /* Server Status */
    u32 latency;                    /* Latency (ms) */
    char version[32];               /* Server Version */
    u32 active_models;              /* Active Models */
    u32 total_models;               /* Total Models */
    u64 uptime;                     /* Server Uptime */
    bool is_default;                /* Is Default */
} server_info_t;

/**
 * Model Status Panel State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *panel_window;         /* Panel Window */
    widget_t *panel_widget;         /* Panel Widget */
    
    /* Servers */
    server_info_t servers[MODEL_STATUS_MAX_SERVERS]; /* Servers */
    u32 server_count;               /* Server Count */
    u32 active_server;              /* Active Server */
    
    /* Models */
    model_info_t models[MODEL_STATUS_MAX_MODELS]; /* Models */
    u32 model_count;                /* Model Count */
    char active_model[128];         /* Active Model */
    
    /* Resource Usage */
    u32 gpu_memory_used;            /* GPU Memory Used (MB) */
    u32 gpu_memory_total;           /* GPU Memory Total (MB) */
    u32 ram_memory_used;            /* RAM Memory Used (MB) */
    u32 cpu_usage;                  /* CPU Usage (%) */
    u32 gpu_usage;                  /* GPU Usage (%) */
    
    /* Settings */
    bool auto_connect;              /* Auto Connect */
    bool show_notifications;        /* Show Notifications */
    u32 refresh_interval;           /* Refresh Interval */
    
    /* Callbacks */
    void (*on_server_changed)(u32);
    void (*on_model_changed)(const char *);
    void (*on_model_downloaded)(const char *);
    void (*on_connection_lost)(void);
    
    /* User Data */
    void *user_data;
    
} model_status_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Model status lifecycle */
int model_status_init(model_status_t *status);
int model_status_show(model_status_t *status);
int model_status_hide(model_status_t *status);
int model_status_toggle(model_status_t *status);
int model_status_shutdown(model_status_t *status);

/* Server management */
int model_status_add_server(model_status_t *status, const char *name, const char *url);
int model_status_remove_server(model_status_t *status, u32 server_id);
int model_status_set_active_server(model_status_t *status, u32 server_id);
int model_status_get_servers(model_status_t *status, server_info_t *servers, u32 *count);
int model_status_connect_server(model_status_t *status, u32 server_id);
int model_status_disconnect_server(model_status_t *status, u32 server_id);
int model_status_test_connection(model_status_t *status, u32 server_id);

/* Model management */
int model_status_get_models(model_status_t *status, model_info_t *models, u32 *count);
int model_status_get_model(model_status_t *status, const char *name, model_info_t *model);
int model_status_set_active_model(model_status_t *status, const char *name);
int model_status_get_active_model(model_status_t *status, model_info_t *model);

/* Model operations */
int model_status_download_model(model_status_t *status, const char *name);
int model_status_delete_model(model_status_t *status, const char *name);
int model_status_pull_model(model_status_t *status, const char *name);

/* Resource monitoring */
int model_status_get_resource_usage(model_status_t *status, 
                                     u32 *gpu_mem, u32 *ram_mem, 
                                     u32 *cpu, u32 *gpu);
int model_status_refresh_resources(model_status_t *status);

/* Settings */
int model_status_set_auto_connect(model_status_t *status, bool enabled);
int model_status_set_notifications(model_status_t *status, bool enabled);
int model_status_set_refresh_interval(model_status_t *status, u32 interval);

/* Utility functions */
const char *model_status_get_server_status_name(u32 status);
model_status_t *model_status_get_instance(void);

#endif /* _MODEL_STATUS_H */
