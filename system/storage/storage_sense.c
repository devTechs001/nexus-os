/*
 * NEXUS OS - Storage Sense
 * system/storage/storage_sense.c
 *
 * Automatic storage management, cleanup, optimization
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         STORAGE SENSE CONFIGURATION                       */
/*===========================================================================*/

#define STORAGE_MAX_DRIVES        16
#define STORAGE_MAX_RULES         32
#define STORAGE_MAX_SCHEDULES     8

/* Cleanup Rules */
#define RULE_DELETE_TEMP_FILES    0x01
#define RULE_DELETE_RECYCLE_BIN   0x02
#define RULE_DELETE_DOWNLOADS     0x03
#define RULE_DELETE_OLD_UPDATES   0x04
#define RULE_COMPRESS_OLD_FILES   0x05
#define RULE_DELETE_DUP_FILES     0x06

/* Schedule Types */
#define SCHEDULE_DISABLED         0
#define SCHEDULE_DAILY            1
#define SCHEDULE_WEEKLY           2
#define SCHEDULE_MONTHLY          3
#define SCHEDULE_LOW_SPACE        4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 drive_id;
    char mount_point[64];
    char label[32];
    u64 total_size;
    u64 used_size;
    u64 free_size;
    u64 system_size;
    u64 app_size;
    u64 user_size;
    u64 temp_size;
    u32 file_count;
    bool is_ssd;
    bool is_enabled;
} storage_drive_t;

typedef struct {
    u32 rule_id;
    u32 type;
    char name[64];
    bool is_enabled;
    u32 threshold_days;         /* Delete files older than X days */
    u64 threshold_size;         /* Or larger than X bytes */
    char path_filter[256];      /* Only apply to this path */
    u32 files_affected;
    u64 space_recoverable;
} storage_rule_t;

typedef struct {
    u32 schedule_id;
    u32 type;
    bool is_enabled;
    u32 hour;                   /* Run at this hour */
    u32 day_of_week;            /* For weekly */
    u32 day_of_month;           /* For monthly */
    u32 free_space_threshold;   /* For low space trigger (%) */
    u64 last_run;
    u32 runs_completed;
} storage_schedule_t;

typedef struct {
    bool initialized;
    storage_drive_t drives[STORAGE_MAX_DRIVES];
    u32 drive_count;
    storage_rule_t rules[STORAGE_MAX_RULES];
    u32 rule_count;
    storage_schedule_t schedules[STORAGE_MAX_SCHEDULES];
    u32 schedule_count;
    u32 active_schedule;
    bool is_cleaning;
    u64 total_space_freed;
    u32 total_files_cleaned;
    spinlock_t lock;
} storage_sense_t;

static storage_sense_t g_storage;

/*===========================================================================*/
/*                         STORAGE SENSE CORE                                */
/*===========================================================================*/

int storage_sense_init(void)
{
    printk("[STORAGE] ========================================\n");
    printk("[STORAGE] NEXUS OS Storage Sense\n");
    printk("[STORAGE] ========================================\n");
    
    memset(&g_storage, 0, sizeof(storage_sense_t));
    spinlock_init(&g_storage.lock);
    
    /* Register storage drives */
    storage_drive_t *drive;
    
    /* System drive */
    drive = &g_storage.drives[g_storage.drive_count++];
    drive->drive_id = 1;
    strcpy(drive->mount_point, "/");
    strcpy(drive->label, "System");
    drive->total_size = 500ULL * 1024 * 1024 * 1024;  /* 500GB */
    drive->used_size = 200ULL * 1024 * 1024 * 1024;   /* 200GB used */
    drive->free_size = 300ULL * 1024 * 1024 * 1024;   /* 300GB free */
    drive->system_size = 50ULL * 1024 * 1024 * 1024;
    drive->app_size = 80ULL * 1024 * 1024 * 1024;
    drive->user_size = 60ULL * 1024 * 1024 * 1024;
    drive->temp_size = 10ULL * 1024 * 1024 * 1024;
    drive->is_ssd = true;
    drive->is_enabled = true;
    
    /* Data drive */
    drive = &g_storage.drives[g_storage.drive_count++];
    drive->drive_id = 2;
    strcpy(drive->mount_point, "/data");
    strcpy(drive->label, "Data");
    drive->total_size = 1000ULL * 1024 * 1024 * 1024;  /* 1TB */
    drive->used_size = 400ULL * 1024 * 1024 * 1024;
    drive->free_size = 600ULL * 1024 * 1024 * 1024;
    drive->is_ssd = false;
    drive->is_enabled = true;
    
    /* Register cleanup rules */
    storage_rule_t *rule;
    
    rule = &g_storage.rules[g_storage.rule_count++];
    rule->rule_id = 1;
    rule->type = RULE_DELETE_TEMP_FILES;
    strcpy(rule->name, "Delete Temporary Files");
    rule->is_enabled = true;
    rule->threshold_days = 1;
    rule->path_filter[0] = '/';
    strcpy(&rule->path_filter[1], "tmp");
    
    rule = &g_storage.rules[g_storage.rule_count++];
    rule->rule_id = 2;
    rule->type = RULE_DELETE_RECYCLE_BIN;
    strcpy(rule->name, "Empty Recycle Bin");
    rule->is_enabled = true;
    rule->threshold_days = 30;
    
    rule = &g_storage.rules[g_storage.rule_count++];
    rule->rule_id = 3;
    rule->type = RULE_DELETE_DOWNLOADS;
    strcpy(rule->name, "Clean Downloads");
    rule->is_enabled = false;
    rule->threshold_days = 30;
    
    rule = &g_storage.rules[g_storage.rule_count++];
    rule->rule_id = 4;
    rule->type = RULE_DELETE_OLD_UPDATES;
    strcpy(rule->name, "Delete Old Updates");
    rule->is_enabled = true;
    rule->threshold_days = 7;
    
    /* Register schedules */
    storage_schedule_t *schedule;
    
    schedule = &g_storage.schedules[g_storage.schedule_count++];
    schedule->schedule_id = 1;
    schedule->type = SCHEDULE_WEEKLY;
    schedule->is_enabled = true;
    schedule->hour = 3;  /* 3 AM */
    schedule->day_of_week = 0;  /* Sunday */
    
    schedule = &g_storage.schedules[g_storage.schedule_count++];
    schedule->schedule_id = 2;
    schedule->type = SCHEDULE_LOW_SPACE;
    schedule->is_enabled = true;
    schedule->free_space_threshold = 10;  /* When free space < 10% */
    
    g_storage.active_schedule = 1;
    
    printk("[STORAGE] Storage Sense initialized\n");
    printk("[STORAGE]   Drives: %d\n", g_storage.drive_count);
    printk("[STORAGE]   Rules: %d\n", g_storage.rule_count);
    printk("[STORAGE]   Schedules: %d\n", g_storage.schedule_count);
    
    return 0;
}

/*===========================================================================*/
/*                         DRIVE ANALYSIS                                    */
/*===========================================================================*/

int storage_get_drive_info(u32 drive_id, storage_drive_t *info)
{
    if (drive_id >= g_storage.drive_count || !info) return -EINVAL;
    
    *info = g_storage.drives[drive_id];
    return 0;
}

int storage_analyze_drive(u32 drive_id)
{
    if (drive_id >= g_storage.drive_count) return -EINVAL;
    
    storage_drive_t *drive = &g_storage.drives[drive_id];
    
    printk("[STORAGE] Analyzing %s (%s)...\n", drive->mount_point, drive->label);
    
    /* In real implementation, would scan filesystem */
    /* Update usage statistics */
    drive->file_count = (u32)(drive->used_size / 4096);  /* Estimate */
    
    printk("[STORAGE] %s: %d GB used of %d GB (%d%% free)\n",
           drive->label,
           (u32)(drive->used_size / (1024 * 1024 * 1024)),
           (u32)(drive->total_size / (1024 * 1024 * 1024)),
           (u32)(drive->free_size * 100 / drive->total_size));
    
    return 0;
}

/*===========================================================================*/
/*                         CLEANUP OPERATIONS                                */
/*===========================================================================*/

int storage_cleanup_rule(u32 rule_id)
{
    if (rule_id >= g_storage.rule_count) return -EINVAL;
    
    storage_rule_t *rule = &g_storage.rules[rule_id];
    
    if (!rule->is_enabled) {
        printk("[STORAGE] Rule '%s' is disabled\n", rule->name);
        return 0;
    }
    
    printk("[STORAGE] Running rule: %s\n", rule->name);
    
    u32 files_deleted = 0;
    u64 space_freed = 0;
    
    /* In real implementation, would apply rule */
    /* Mock: simulate cleanup */
    switch (rule->type) {
        case RULE_DELETE_TEMP_FILES:
            files_deleted = 1500;
            space_freed = 2ULL * 1024 * 1024 * 1024;  /* 2GB */
            break;
        case RULE_DELETE_RECYCLE_BIN:
            files_deleted = 500;
            space_freed = 1ULL * 1024 * 1024 * 1024;  /* 1GB */
            break;
        case RULE_DELETE_OLD_UPDATES:
            files_deleted = 100;
            space_freed = 5ULL * 1024 * 1024 * 1024;  /* 5GB */
            break;
    }
    
    rule->files_affected = files_deleted;
    rule->space_recoverable = space_freed;
    
    g_storage.total_space_freed += space_freed;
    g_storage.total_files_cleaned += files_deleted;
    
    printk("[STORAGE] Rule complete: deleted %d files, freed %d MB\n",
           files_deleted, (u32)(space_freed / (1024 * 1024)));
    
    return 0;
}

int storage_cleanup_all(void)
{
    printk("[STORAGE] Running full storage cleanup...\n");
    
    g_storage.is_cleaning = true;
    
    u64 total_freed = 0;
    u32 total_deleted = 0;
    
    for (u32 i = 0; i < g_storage.rule_count; i++) {
        if (g_storage.rules[i].is_enabled) {
            storage_cleanup_rule(i);
            total_freed += g_storage.rules[i].space_recoverable;
            total_deleted += g_storage.rules[i].files_affected;
        }
    }
    
    g_storage.is_cleaning = false;
    
    printk("[STORAGE] Cleanup complete: freed %d MB\n",
           (u32)(total_freed / (1024 * 1024)));
    
    return 0;
}

/*===========================================================================*/
/*                         SCHEDULE MANAGEMENT                               */
/*===========================================================================*/

int storage_set_schedule(u32 schedule_id, u32 type, u32 hour, u32 day)
{
    if (schedule_id >= g_storage.schedule_count) return -EINVAL;
    
    storage_schedule_t *schedule = &g_storage.schedules[schedule_id];
    
    schedule->type = type;
    schedule->hour = hour;
    
    if (type == SCHEDULE_WEEKLY) {
        schedule->day_of_week = day;
    } else if (type == SCHEDULE_MONTHLY) {
        schedule->day_of_month = day;
    }
    
    printk("[STORAGE] Schedule updated: type=%d, hour=%d, day=%d\n",
           type, hour, day);
    
    return 0;
}

int storage_enable_schedule(u32 schedule_id, bool enabled)
{
    if (schedule_id >= g_storage.schedule_count) return -EINVAL;
    
    g_storage.schedules[schedule_id].is_enabled = enabled;
    g_storage.active_schedule = enabled ? schedule_id : 0;
    
    printk("[STORAGE] Schedule %d %s\n", 
           schedule_id, enabled ? "enabled" : "disabled");
    
    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

int storage_get_stats(u64 *total_freed, u32 *files_cleaned, u32 *runs)
{
    if (total_freed) *total_freed = g_storage.total_space_freed;
    if (files_cleaned) *files_cleaned = g_storage.total_files_cleaned;
    
    u32 runs_count = 0;
    for (u32 i = 0; i < g_storage.schedule_count; i++) {
        runs_count += g_storage.schedules[i].runs_completed;
    }
    if (runs) *runs = runs_count;
    
    return 0;
}

int storage_get_recommendations(char *recommendations, u32 size)
{
    if (!recommendations || size == 0) return -EINVAL;
    
    recommendations[0] = '\0';
    
    /* Check for low space */
    for (u32 i = 0; i < g_storage.drive_count; i++) {
        storage_drive_t *drive = &g_storage.drives[i];
        u32 free_percent = (u32)(drive->free_size * 100 / drive->total_size);
        
        if (free_percent < 10) {
            strncat(recommendations, 
                    "CRITICAL: Low disk space! Run cleanup now.\n",
                    size - strlen(recommendations) - 1);
        } else if (free_percent < 20) {
            strncat(recommendations,
                    "WARNING: Disk space running low. Consider cleanup.\n",
                    size - strlen(recommendations) - 1);
        }
        
        /* Check for large temp files */
        if (drive->temp_size > 5ULL * 1024 * 1024 * 1024) {
            strncat(recommendations,
                    "INFO: Large temporary files detected. Clean to free space.\n",
                    size - strlen(recommendations) - 1);
        }
    }
    
    return 0;
}

storage_sense_t *storage_sense_get(void)
{
    return &g_storage;
}
