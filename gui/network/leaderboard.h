/*
 * NEXUS OS - Network Leaderboard
 * gui/network/leaderboard.h
 *
 * Contributor rankings and network participation stats
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _LEADERBOARD_H
#define _LEADERBOARD_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         LEADERBOARD CONFIGURATION                         */
/*===========================================================================*/

#define LEADERBOARD_VERSION         "1.0.0"
#define LEADERBOARD_MAX_ENTRIES     1000
#define LEADERBOARD_MAX_REGIONS     16

/*===========================================================================*/
/*                         RANKING CATEGORIES                                */
/*===========================================================================*/

#define RANK_CATEGORY_TOTAL         0
#define RANK_CATEGORY_REWARDS       1
#define RANK_CATEGORY_TASKS         2
#define RANK_CATEGORY_UPTIME        3
#define RANK_CATEGORY_BANDWIDTH     4
#define RANK_CATEGORY_RELIABILITY   5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Leaderboard Entry
 */
typedef struct {
    u32 rank;                       /* Rank */
    u32 prev_rank;                  /* Previous Rank */
    char node_address[128];         /* Node Address */
    char node_name[128];            /* Node Name */
    char region[64];                /* Region */
    char country[64];               /* Country */
    u64 total_rewards;              /* Total Rewards */
    u64 rewards_today;              /* Rewards Today */
    u64 rewards_this_week;          /* Rewards This Week */
    u32 tasks_completed;            /* Tasks Completed */
    u32 tasks_failed;               /* Tasks Failed */
    u64 uptime;                     /* Total Uptime */
    u64 bandwidth_upload;           /* Upload Bandwidth */
    u64 bandwidth_download;         /* Download Bandwidth */
    u32 reliability_score;          /* Reliability Score */
    u32 node_level;                 /* Node Level */
    u64 last_active;                /* Last Active */
    bool is_mine;                   /* Is My Node */
} leaderboard_entry_t;

/**
 * Regional Statistics
 */
typedef struct {
    char region[64];                /* Region Name */
    u32 total_nodes;                /* Total Nodes */
    u32 active_nodes;               /* Active Nodes */
    u64 total_rewards;              /* Total Rewards */
    u32 total_tasks;                /* Total Tasks */
    u32 top_node_rank;              /* Top Node Rank */
    f32 avg_uptime;                 /* Average Uptime */
    f32 avg_reliability;            /* Average Reliability */
} regional_stats_t;

/**
 * Network Statistics
 */
typedef struct {
    u32 total_nodes;                /* Total Nodes */
    u32 active_nodes;               /* Active Nodes */
    u64 total_rewards_distributed;  /* Total Rewards */
    u64 total_tasks_completed;      /* Total Tasks */
    u64 total_bandwidth;            /* Total Bandwidth */
    u32 network_uptime;             /* Network Uptime (%) */
    u64 network_start_time;         /* Network Start Time */
    char network_version[32];       /* Network Version */
} network_stats_t;

/**
 * Leaderboard State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *leaderboard_window;   /* Leaderboard Window */
    widget_t *leaderboard_widget;   /* Leaderboard Widget */
    
    /* Entries */
    leaderboard_entry_t entries[LEADERBOARD_MAX_ENTRIES]; /* Entries */
    u32 entry_count;                /* Entry Count */
    u32 my_rank;                    /* My Rank */
    
    /* Categories */
    u32 current_category;           /* Current Category */
    char category_name[64];         /* Category Name */
    
    /* Regions */
    regional_stats_t regions[LEADERBOARD_MAX_REGIONS]; /* Regions */
    u32 region_count;               /* Region Count */
    
    /* Network Stats */
    network_stats_t network_stats;  /* Network Statistics */
    
    /* Time Period */
    u32 time_period;                /* Time Period */
    char period_name[32];           /* Period Name */
    
    /* Filter */
    char filter_region[64];         /* Filter by Region */
    char filter_country[64];        /* Filter by Country */
    u32 filter_min_level;           /* Filter by Min Level */
    
    /* Sorting */
    u32 sort_by;                    /* Sort By */
    bool sort_ascending;            /* Sort Ascending */
    
    /* Callbacks */
    void (*on_entry_selected)(leaderboard_entry_t *);
    void (*on_refresh)(void);
    
    /* User Data */
    void *user_data;
    
} leaderboard_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Leaderboard lifecycle */
int leaderboard_init(leaderboard_t *board);
int leaderboard_show(leaderboard_t *board);
int leaderboard_hide(leaderboard_t *board);
int leaderboard_toggle(leaderboard_t *board);
int leaderboard_shutdown(leaderboard_t *board);

/* Data management */
int leaderboard_refresh(leaderboard_t *board);
int leaderboard_get_entries(leaderboard_t *board, leaderboard_entry_t *entries, u32 *count);
int leaderboard_get_entry(leaderboard_t *board, u32 rank, leaderboard_entry_t *entry);
int leaderboard_get_my_rank(leaderboard_t *board, leaderboard_entry_t *entry);

/* Category management */
int leaderboard_set_category(leaderboard_t *board, u32 category);
int leaderboard_get_category(leaderboard_t *board, u32 *category);
int leaderboard_get_categories(leaderboard_t *board, char names[][64], u32 *count);

/* Time period */
int leaderboard_set_period(leaderboard_t *board, u32 period);
int leaderboard_get_period(leaderboard_t *board, u32 *period);

/* Filter */
int leaderboard_set_filter(leaderboard_t *board, const char *region, const char *country, u32 min_level);
int leaderboard_clear_filter(leaderboard_t *board);

/* Sorting */
int leaderboard_set_sort(leaderboard_t *board, u32 sort_by, bool ascending);
int leaderboard_get_sort(leaderboard_t *board, u32 *sort_by, bool *ascending);

/* Regional stats */
int leaderboard_get_regional_stats(leaderboard_t *board, regional_stats_t *stats, u32 *count);
int leaderboard_get_region_stats(leaderboard_t *board, const char *region, regional_stats_t *stats);

/* Network stats */
int leaderboard_get_network_stats(leaderboard_t *board, network_stats_t *stats);

/* Export */
int leaderboard_export(leaderboard_t *board, const char *path, const char *format);
int leaderboard_export_my_stats(leaderboard_t *board, const char *path);

/* Utility functions */
const char *leaderboard_get_category_name(u32 category);
const char *leaderboard_get_period_name(u32 period);
leaderboard_t *leaderboard_get_instance(void);

#endif /* _LEADERBOARD_H */
