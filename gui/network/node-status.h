/*
 * NEXUS OS - Node Status Dashboard
 * gui/network/node-status.h
 *
 * Track personal node activity, tasks, and rewards
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _NODE_STATUS_H
#define _NODE_STATUS_H

#include "../../gui/gui.h"
#include "../../gui/widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"

/*===========================================================================*/
/*                         NODE STATUS CONFIGURATION                         */
/*===========================================================================*/

#define NODE_STATUS_VERSION         "1.0.0"
#define NODE_STATUS_MAX_TASKS       256
#define NODE_STATUS_MAX_REWARDS     512

/*===========================================================================*/
/*                         TASK STATUS                                       */
/*===========================================================================*/

#define TASK_STATUS_PENDING         0
#define TASK_STATUS_RUNNING         1
#define TASK_STATUS_COMPLETED       2
#define TASK_STATUS_FAILED          3
#define TASK_STATUS_TIMEOUT         4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Task Information
 */
typedef struct {
    u32 task_id;                    /* Task ID */
    char task_name[128];            /* Task Name */
    char task_type[64];             /* Task Type */
    u32 status;                     /* Task Status */
    u64 created_time;               /* Created Time */
    u64 start_time;                 /* Start Time */
    u64 end_time;                   /* End Time */
    u32 progress;                   /* Progress (%) */
    char description[512];          /* Description */
    u64 reward;                     /* Reward */
    u32 priority;                   /* Priority */
    char error_message[256];        /* Error Message */
} task_info_t;

/**
 * Reward Information
 */
typedef struct {
    u32 reward_id;                  /* Reward ID */
    u64 amount;                     /* Amount */
    char currency[32];              /* Currency */
    u64 timestamp;                  /* Timestamp */
    char source[128];               /* Reward Source */
    u32 task_id;                    /* Associated Task ID */
    bool is_claimed;                /* Is Claimed */
    u64 claim_time;                 /* Claim Time */
} reward_info_t;

/**
 * Node Statistics
 */
typedef struct {
    u64 uptime;                     /* Total Uptime */
    u32 tasks_total;                /* Total Tasks */
    u32 tasks_completed;            /* Completed Tasks */
    u32 tasks_failed;               /* Failed Tasks */
    u32 tasks_pending;              /* Pending Tasks */
    u64 rewards_total;              /* Total Rewards */
    u64 rewards_claimed;            /* Claimed Rewards */
    u64 rewards_pending;            /* Pending Rewards */
    u64 bandwidth_upload;           /* Total Upload */
    u64 bandwidth_download;         /* Total Download */
    u32 avg_latency;                /* Average Latency */
    u32 reliability_score;          /* Reliability Score */
    u32 rank;                       /* Node Rank */
} node_statistics_t;

/**
 * Node Status Dashboard State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    bool visible;                   /* Is Visible */
    
    /* Window */
    window_t *dashboard_window;     /* Dashboard Window */
    widget_t *dashboard_widget;     /* Dashboard Widget */
    
    /* Node Info */
    char node_address[128];         /* Node Address */
    char node_name[128];            /* Node Name */
    u32 node_status;                /* Node Status */
    u64 node_uptime;                /* Node Uptime */
    
    /* Tasks */
    task_info_t tasks[NODE_STATUS_MAX_TASKS]; /* Tasks */
    u32 task_count;                 /* Task Count */
    u32 active_task_count;          /* Active Task Count */
    
    /* Rewards */
    reward_info_t rewards[NODE_STATUS_MAX_REWARDS]; /* Rewards */
    u32 reward_count;               /* Reward Count */
    u64 total_rewards;              /* Total Rewards */
    u64 pending_rewards;            /* Pending Rewards */
    
    /* Statistics */
    node_statistics_t stats;        /* Node Statistics */
    
    /* Charts */
    u32 hourly_tasks[24];           /* Hourly Tasks (24h) */
    u64 hourly_rewards[24];         /* Hourly Rewards (24h) */
    u32 daily_tasks[30];            /* Daily Tasks (30d) */
    u64 daily_rewards[30];          /* Daily Rewards (30d) */
    
    /* Settings */
    bool auto_refresh;              /* Auto Refresh */
    u32 refresh_interval;           /* Refresh Interval */
    bool show_notifications;        /* Show Notifications */
    bool auto_claim_rewards;        /* Auto Claim Rewards */
    
    /* Callbacks */
    void (*on_task_completed)(task_info_t *);
    void (*on_reward_received)(reward_info_t *);
    void (*on_status_changed)(u32 status);
    
    /* User Data */
    void *user_data;
    
} node_status_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Dashboard lifecycle */
int node_status_init(node_status_t *status);
int node_status_show(node_status_t *status);
int node_status_hide(node_status_t *status);
int node_status_toggle(node_status_t *status);
int node_status_shutdown(node_status_t *status);

/* Node management */
int node_status_set_node(node_status_t *status, const char *address, const char *name);
int node_status_get_node_info(node_status_t *status, char *address, char *name);
int node_status_start_node(node_status_t *status);
int node_status_stop_node(node_status_t *status);
int node_status_restart_node(node_status_t *status);

/* Task management */
int node_status_get_tasks(node_status_t *status, task_info_t *tasks, u32 *count);
int node_status_get_task(node_status_t *status, u32 task_id, task_info_t *task);
int node_status_cancel_task(node_status_t *status, u32 task_id);
int node_status_retry_task(node_status_t *status, u32 task_id);
int node_status_get_active_tasks(node_status_t *status, task_info_t *tasks, u32 *count);
int node_status_get_completed_tasks(node_status_t *status, task_info_t *tasks, u32 *count);

/* Reward management */
int node_status_get_rewards(node_status_t *status, reward_info_t *rewards, u32 *count);
int node_status_get_reward(node_status_t *status, u32 reward_id, reward_info_t *reward);
int node_status_claim_reward(node_status_t *status, u32 reward_id);
int node_status_claim_all_rewards(node_status_t *status);
int node_status_get_pending_rewards(node_status_t *status, reward_info_t *rewards, u32 *count);

/* Statistics */
int node_status_get_statistics(node_status_t *status, node_statistics_t *stats);
int node_status_get_uptime(node_status_t *status, u64 *uptime);
int node_status_get_reliability(node_status_t *status, u32 *score);
int node_status_get_rank(node_status_t *status, u32 *rank);

/* Charts */
int node_status_get_hourly_data(node_status_t *status, u32 *tasks, u64 *rewards);
int node_status_get_daily_data(node_status_t *status, u32 *tasks, u64 *rewards);

/* Settings */
int node_status_set_auto_refresh(node_status_t *status, bool enabled, u32 interval);
int node_status_set_notifications(node_status_t *status, bool enabled);
int node_status_set_auto_claim(node_status_t *status, bool enabled);

/* Export */
int node_status_export_tasks(node_status_t *status, const char *path, const char *format);
int node_status_export_rewards(node_status_t *status, const char *path, const char *format);
int node_status_export_statistics(node_status_t *status, const char *path);

/* Utility functions */
const char *node_status_get_task_status_name(u32 status);
const char *node_status_get_node_status_name(u32 status);
node_status_t *node_status_get_instance(void);

#endif /* _NODE_STATUS_H */
