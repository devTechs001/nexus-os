/*
 * NEXUS OS - Live Node Map
 * gui/network/node-map.h
 *
 * Real-time visualization of active network nodes worldwide
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _NODE_MAP_H
#define _NODE_MAP_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         NODE MAP CONFIGURATION                            */
/*===========================================================================*/

#define NODE_MAP_VERSION            "1.0.0"
#define NODE_MAP_MAX_NODES          1024
#define NODE_MAP_MAX_REGIONS        16

/*===========================================================================*/
/*                         NODE STATUS                                       */
/*===========================================================================*/

#define NODE_STATUS_OFFLINE         0
#define NODE_STATUS_ONLINE          1
#define NODE_STATUS_SYNCING         2
#define NODE_STATUS_VALIDATING      3
#define NODE_STATUS_MINING          4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Node Information
 */
typedef struct {
    u32 node_id;                    /* Node ID */
    char node_address[128];         /* Node Address */
    char region[64];                /* Region */
    char country[64];               /* Country */
    char city[64];                  /* City */
    f32 latitude;                   /* Latitude */
    f32 longitude;                  /* Longitude */
    u32 status;                     /* Node Status */
    u64 uptime;                     /* Uptime (seconds) */
    u32 tasks_completed;            /* Tasks Completed */
    u32 tasks_pending;              /* Tasks Pending */
    u64 rewards_earned;             /* Rewards Earned */
    u32 bandwidth_upload;           /* Upload Bandwidth (KB/s) */
    u32 bandwidth_download;         /* Download Bandwidth (KB/s) */
    u32 latency;                    /* Latency (ms) */
    u64 last_seen;                  /* Last Seen */
    char version[32];               /* Node Version */
    bool is_mine;                   /* Is My Node */
} node_info_t;

/**
 * Network Region
 */
typedef struct {
    char name[64];                  /* Region Name */
    u32 total_nodes;                /* Total Nodes */
    u32 online_nodes;               /* Online Nodes */
    u32 offline_nodes;              /* Offline Nodes */
    u64 total_bandwidth;            /* Total Bandwidth */
    u64 total_tasks;                /* Total Tasks */
    f32 avg_latency;                /* Average Latency */
} network_region_t;

/**
 * Node Map State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *map_window;           /* Map Window */
    widget_t *map_widget;           /* Map Widget */
    
    /* Nodes */
    node_info_t *nodes;             /* Node List */
    u32 node_count;                 /* Node Count */
    u32 max_nodes;                  /* Max Nodes */
    node_info_t *selected_node;     /* Selected Node */
    
    /* Regions */
    network_region_t regions[NODE_MAP_MAX_REGIONS]; /* Regions */
    u32 region_count;               /* Region Count */
    
    /* Statistics */
    u32 total_nodes;                /* Total Nodes */
    u32 online_nodes;               /* Online Nodes */
    u32 offline_nodes;              /* Offline Nodes */
    u64 total_bandwidth;            /* Total Bandwidth */
    u64 total_tasks_24h;            /* Tasks (24h) */
    u64 total_rewards;              /* Total Rewards */
    
    /* Map Settings */
    bool show_offline;              /* Show Offline Nodes */
    bool show_latency;              /* Show Latency Lines */
    bool show_bandwidth;            /* Show Bandwidth */
    bool auto_refresh;              /* Auto Refresh */
    u32 refresh_interval;           /* Refresh Interval (seconds) */
    u32 map_zoom;                   /* Map Zoom Level */
    f32 map_center_lat;             /* Map Center Latitude */
    f32 map_center_lon;             /* Map Center Longitude */
    
    /* Filter */
    u32 filter_status;              /* Filter by Status */
    char filter_region[64];         /* Filter by Region */
    u32 filter_min_uptime;          /* Filter by Min Uptime */
    
    /* Callbacks */
    void (*on_node_selected)(node_info_t *);
    void (*on_node_double_click)(node_info_t *);
    void (*on_map_refresh)(void);
    
    /* User Data */
    void *user_data;
    
} node_map_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Node map lifecycle */
int node_map_init(node_map_t *map);
int node_map_show(node_map_t *map);
int node_map_hide(node_map_t *map);
int node_map_toggle(node_map_t *map);
int node_map_shutdown(node_map_t *map);

/* Node management */
int node_map_add_node(node_map_t *map, node_info_t *node);
int node_map_remove_node(node_map_t *map, u32 node_id);
int node_map_update_node(node_map_t *map, node_info_t *node);
node_info_t *node_map_get_node(node_map_t *map, u32 node_id);
int node_map_select_node(node_map_t *map, u32 node_id);
int node_map_get_nodes(node_map_t *map, node_info_t *nodes, u32 *count);

/* Node operations */
int node_map_ping_node(node_map_t *map, u32 node_id);
int node_map_view_node_details(node_map_t *map, u32 node_id);
int node_map_view_node_tasks(node_map_t *map, u32 node_id);
int node_map_view_node_rewards(node_map_t *map, u32 node_id);

/* Region management */
int node_map_get_region_stats(node_map_t *map, const char *region, network_region_t *stats);
int node_map_get_all_regions(node_map_t *map, network_region_t *regions, u32 *count);

/* Statistics */
int node_map_get_total_stats(node_map_t *map, u32 *total, u32 *online, u32 *offline);
int node_map_get_bandwidth_stats(node_map_t *map, u64 *upload, u64 *download);
int node_map_get_reward_stats(node_map_t *map, u64 *total, u64 *today, u64 *this_week);

/* Filter */
int node_map_set_filter(node_map_t *map, u32 status, const char *region, u32 min_uptime);
int node_map_clear_filter(node_map_t *map);

/* Map settings */
int node_map_set_zoom(node_map_t *map, u32 zoom);
int node_map_set_center(node_map_t *map, f32 lat, f32 lon);
int node_map_toggle_offline(node_map_t *map, bool show);
int node_map_toggle_latency(node_map_t *map, bool show);
int node_map_toggle_bandwidth(node_map_t *map, bool show);
int node_map_set_auto_refresh(node_map_t *map, bool enabled, u32 interval);

/* Export */
int node_map_export_nodes(node_map_t *map, const char *path, const char *format);
int node_map_export_stats(node_map_t *map, const char *path);

/* Utility functions */
const char *node_map_get_status_name(u32 status);
node_map_t *node_map_get_instance(void);

#endif /* _NODE_MAP_H */
