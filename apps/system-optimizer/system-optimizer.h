/*
 * NEXUS OS - System Optimizer
 * apps/system-optimizer/system-optimizer.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * System optimization features:
 * - Memory optimization
 * - CPU optimization
 * - Disk cleanup
 * - Startup optimization
 * - Service management
 * - Power optimization
 */

#ifndef _NEXUS_SYSTEM_OPTIMIZER_H
#define _NEXUS_SYSTEM_OPTIMIZER_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         OPTIMIZATION STRUCTURES                           */
/*===========================================================================*/

/**
 * optimization_result_t - Optimization result
 */
typedef struct {
    char name[128];
    char description[256];
    bool success;
    u64 time_saved_ms;
    u64 memory_freed;
    u64 disk_freed;
    float performance_gain;
    char message[512];
} optimization_result_t;

/**
 * optimizer_config_t - Optimizer configuration
 */
typedef struct {
    /* Memory */
    bool auto_clean_cache;
    bool compact_memory;
    bool swap_optimization;
    
    /* CPU */
    bool scheduler_optimization;
    bool irq_balancing;
    bool cpu_governor_opt;
    
    /* Disk */
    bool auto_clean_temp;
    bool auto_clean_logs;
    bool auto_clean_cache;
    bool trim_ssd;
    
    /* Network */
    bool tcp_optimization;
    bool dns_cache_opt;
    
    /* Power */
    bool power_saving_mode;
    bool auto_sleep;
    
    /* Startup */
    bool startup_optimization;
    bool service_optimization;
} optimizer_config_t;

/*===========================================================================*/
/*                         SYSTEM OPTIMIZER API                              */
/*===========================================================================*/

/* Initialization */
int system_optimizer_init(optimizer_config_t *config);
int system_optimizer_shutdown(void);

/* Memory optimization */
int optimizer_clean_memory(void);
int optimizer_clean_cache(void);
int optimizer_compact_memory(void);
int optimizer_optimize_swap(void);

/* Disk optimization */
int optimizer_clean_temp_files(void);
int optimizer_clean_logs(void);
int optimizer_clean_package_cache(void);
int optimizer_trim_ssd(void);
int optimizer_defrag_disk(const char *device);

/* CPU optimization */
int optimizer_set_cpu_governor(const char *governor);
int optimizer_balance_irq(void);
int optimizer_optimize_scheduler(void);

/* Network optimization */
int optimizer_optimize_tcp(void);
int optimizer_clean_dns_cache(void);
int optimizer_optimize_network_buffers(void);

/* Power optimization */
int optimizer_enable_power_saving(void);
int optimizer_disable_power_saving(void);
int optimizer_optimize_brightness(void);
int optimizer_optimize_sleep(void);

/* Startup optimization */
int optimizer_analyze_startup(void);
int optimizer_disable_startup_service(const char *service);
int optimizer_enable_startup_service(const char *service);

/* Service management */
int optimizer_list_services(void);
int optimizer_start_service(const char *service);
int optimizer_stop_service(const char *service);
int optimizer_restart_service(const char *service);
int optimizer_enable_service(const char *service);
int optimizer_disable_service(const char *service);

/* Full optimization */
int optimizer_run_full(void);
int optimizer_run_quick(void);
int optimizer_run_custom(const char **optimizations, int count);

/* Analysis */
int optimizer_analyze_system(void);
int optimizer_get_recommendations(void);
int optimizer_get_score(void);

/* Scheduling */
int optimizer_schedule_optimization(int interval_minutes);
int optimizer_cancel_scheduled(void);

/* Results */
int optimizer_get_results(optimization_result_t **results, int *count);
int optimizer_clear_results(void);

#endif /* _NEXUS_SYSTEM_OPTIMIZER_H */
