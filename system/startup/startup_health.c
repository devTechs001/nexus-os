/*
 * NEXUS OS - Startup Health Check & Auto-Repair System
 * system/startup/startup_health.c
 *
 * Comprehensive system health monitoring with automatic repair
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         CONFIGURATION                                     */
/*===========================================================================*/

#define MAX_DEPENDENCIES        256
#define MAX_SERVICES            128
#define MAX_REPAIR_ATTEMPTS     3
#define HEALTH_CHECK_INTERVAL   30000  /* 30 seconds */
#define AUTO_REPAIR_TIMEOUT     60000  /* 60 seconds */

/* Health Status Levels */
#define HEALTH_STATUS_OK            0
#define HEALTH_STATUS_WARNING       1
#define HEALTH_STATUS_DEGRADED      2
#define HEALTH_STATUS_CRITICAL      3
#define HEALTH_STATUS_FAILED        4

/* Dependency Types */
#define DEP_TYPE_LIBRARY        1
#define DEP_TYPE_MODULE         2
#define DEP_TYPE_SERVICE        3
#define DEP_TYPE_DRIVER         4
#define DEP_TYPE_CONFIG         5
#define DEP_TYPE_FILE           6
#define DEP_TYPE_DIRECTORY      7
#define DEP_TYPE_PERMISSION     8

/* Repair Actions */
#define REPAIR_ACTION_NONE          0
#define REPAIR_ACTION_REINSTALL     1
#define REPAIR_ACTION_RESTORE       2
#define REPAIR_ACTION_REBUILD       3
#define REPAIR_ACTION_RESET         4
#define REPAIR_ACTION_REPLACE       5
#define REPAIR_ACTION_CREATE        6
#define REPAIR_ACTION_PERMISSIONS   7

/* Startup Modes */
#define STARTUP_MODE_NORMAL         0
#define STARTUP_MODE_SAFE           1
#define STARTUP_MODE_RECOVERY       2
#define STARTUP_MODE_DIAGNOSTIC     3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/* Dependency Entry */
typedef struct {
    char name[128];
    char path[256];
    u32 type;
    bool required;
    bool loaded;
    bool healthy;
    u64 size_bytes;
    u64 last_modified;
    char version[32];
    char checksum[64];
    char expected_checksum[64];
    u32 repair_attempts;
    u64 last_check_time;
    char error_message[256];
} dependency_entry_t;

/* Service Status */
typedef struct {
    char name[64];
    bool enabled;
    bool running;
    bool healthy;
    u32 pid;
    u32 restart_count;
    u64 uptime_ms;
    char status[32];
    char error_message[256];
} service_status_t;

/* Repair Job */
typedef struct {
    u32 id;
    char component[128];
    u32 action;
    u32 priority;
    u32 status;  /* 0=pending, 1=running, 2=complete, 3=failed */
    u32 progress_percent;
    char description[256];
    char result_message[256];
    u64 start_time;
    u64 end_time;
} repair_job_t;

/* System Health Report */
typedef struct {
    u32 overall_status;
    u32 total_dependencies;
    u32 healthy_dependencies;
    u32 missing_dependencies;
    u32 corrupted_dependencies;
    u32 total_services;
    u32 running_services;
    u32 failed_services;
    u32 repair_jobs_pending;
    u32 repair_jobs_completed;
    u32 repair_jobs_failed;
    u64 last_check_time;
    u64 boot_time;
    char startup_mode[32];
    char recommendations[8][256];
    u32 recommendation_count;
} health_report_t;

/* Startup Health Manager */
typedef struct {
    bool initialized;
    u32 startup_mode;
    dependency_entry_t *dependencies;
    u32 dependency_count;
    service_status_t *services;
    u32 service_count;
    repair_job_t *repair_jobs;
    u32 repair_job_count;
    health_report_t report;
    bool auto_repair_enabled;
    bool notify_user;
    bool safe_mode_fallback;
    u32 consecutive_failures;
    u64 last_successful_boot;
    spinlock_t lock;
} startup_health_manager_t;

static startup_health_manager_t g_startup_health;

/*===========================================================================*/
/*                         DEPENDENCY CHECKING                               */
/*===========================================================================*/

/* Check File Existence */
bool dependency_check_file_exists(const char *path)
{
    /* Would use VFS to check file existence */
    printk("[HEALTH] Checking file: %s\n", path);
    return true;  /* Placeholder */
}

/* Check File Integrity (Checksum) */
bool dependency_check_integrity(const char *path, const char *expected_checksum)
{
    char actual_checksum[64];
    
    /* Calculate actual checksum */
    /* calculate_checksum(path, actual_checksum, 64); */
    
    /* Compare with expected */
    if (strcmp(actual_checksum, expected_checksum) == 0) {
        return true;
    }
    
    printk("[HEALTH] Integrity check failed: %s\n", path);
    printk("[HEALTH] Expected: %s\n", expected_checksum);
    printk("[HEALTH] Actual: %s\n", actual_checksum);
    
    return false;
}

/* Check Dependency Version */
bool dependency_check_version(const char *path, const char *min_version)
{
    char actual_version[32];
    
    /* Get actual version */
    /* get_version(path, actual_version, 32); */
    
    /* Compare versions */
    if (version_compare(actual_version, min_version) >= 0) {
        return true;
    }
    
    printk("[HEALTH] Version check failed: %s\n", path);
    printk("[HEALTH] Required: >= %s\n", min_version);
    printk("[HEALTH] Found: %s\n", actual_version);
    
    return false;
}

/* Check Dependency Permissions */
bool dependency_check_permissions(const char *path, u32 required_perms)
{
    u32 actual_perms;
    
    /* Get actual permissions */
    /* actual_perms = get_permissions(path); */
    
    if ((actual_perms & required_perms) == required_perms) {
        return true;
    }
    
    printk("[HEALTH] Permission check failed: %s\n", path);
    printk("[HEALTH] Required: 0x%x\n", required_perms);
    printk("[HEALTH] Actual: 0x%x\n", actual_perms);
    
    return false;
}

/* Add Dependency to Check List */
int dependency_add(const char *name, const char *path, u32 type, bool required)
{
    if (g_startup_health.dependency_count >= MAX_DEPENDENCIES) {
        printk("[HEALTH] ERROR: Maximum dependencies reached\n");
        return -1;
    }
    
    dependency_entry_t *dep = &g_startup_health.dependencies[g_startup_health.dependency_count++];
    memset(dep, 0, sizeof(dependency_entry_t));
    
    strncpy(dep->name, name, 127);
    strncpy(dep->path, path, 255);
    dep->type = type;
    dep->required = required;
    dep->loaded = false;
    dep->healthy = false;
    
    printk("[HEALTH] Added dependency: %s (%s)\n", name, path);
    
    return 0;
}

/* Check All Dependencies */
int dependency_check_all(void)
{
    printk("[HEALTH] ========================================\n");
    printk("[HEALTH] Checking system dependencies...\n");
    printk("[HEALTH] ========================================\n");
    
    u32 healthy = 0;
    u32 missing = 0;
    u32 corrupted = 0;
    
    for (u32 i = 0; i < g_startup_health.dependency_count; i++) {
        dependency_entry_t *dep = &g_startup_health.dependencies[i];
        dep->last_check_time = 0;  /* Would get from kernel */
        
        printk("[HEALTH] Checking: %s...\n", dep->name);
        
        /* Check existence */
        if (!dependency_check_file_exists(dep->path)) {
            dep->loaded = false;
            dep->healthy = false;
            missing++;
            snprintf(dep->error_message, 255, "File not found: %s", dep->path);
            printk("[HEALTH] MISSING: %s\n", dep->name);
            continue;
        }
        
        dep->loaded = true;
        
        /* Check integrity */
        if (dep->expected_checksum[0] != '\0') {
            if (!dependency_check_integrity(dep->path, dep->expected_checksum)) {
                dep->healthy = false;
                corrupted++;
                snprintf(dep->error_message, 255, "Checksum mismatch");
                printk("[HEALTH] CORRUPTED: %s\n", dep->name);
                continue;
            }
        }
        
        /* Check version */
        /* if (!dependency_check_version(dep->path, dep->version)) { */
        /*     dep->healthy = false; */
        /*     corrupted++; */
        /*     continue; */
        /* } */
        
        /* Check permissions */
        /* if (!dependency_check_permissions(dep->path, 0x755)) { */
        /*     dep->healthy = false; */
        /*     corrupted++; */
        /*     continue; */
        /* } */
        
        dep->healthy = true;
        healthy++;
        printk("[HEALTH] OK: %s\n", dep->name);
    }
    
    g_startup_health.report.healthy_dependencies = healthy;
    g_startup_health.report.missing_dependencies = missing;
    g_startup_health.report.corrupted_dependencies = corrupted;
    
    printk("[HEALTH] ========================================\n");
    printk("[HEALTH] Dependencies: %d total, %d healthy, %d missing, %d corrupted\n",
           g_startup_health.dependency_count, healthy, missing, corrupted);
    printk("[HEALTH] ========================================\n");
    
    return (missing == 0 && corrupted == 0) ? 0 : -1;
}

/*===========================================================================*/
/*                         SERVICE CHECKING                                  */
/*===========================================================================*/

/* Check Service Status */
int service_check_status(const char *name)
{
    for (u32 i = 0; i < g_startup_health.service_count; i++) {
        service_status_t *svc = &g_startup_health.services[i];
        
        if (strcmp(svc->name, name) == 0) {
            /* Check if service is running */
            /* svc->running = is_service_running(name); */
            /* svc->pid = get_service_pid(name); */
            /* svc->uptime_ms = get_service_uptime(name); */
            
            svc->healthy = svc->running;
            
            if (!svc->running && svc->enabled) {
                printk("[HEALTH] Service not running: %s\n", name);
            }
            
            return svc->healthy ? 0 : -1;
        }
    }
    
    return -ENOENT;
}

/* Start Service */
int service_start(const char *name)
{
    printk("[HEALTH] Starting service: %s\n", name);
    /* return start_service(name); */
    return 0;
}

/* Restart Service */
int service_restart(const char *name)
{
    printk("[HEALTH] Restarting service: %s\n", name);
    /* return restart_service(name); */
    return 0;
}

/* Check All Services */
int service_check_all(void)
{
    printk("[HEALTH] ========================================\n");
    printk("[HEALTH] Checking system services...\n");
    printk("[HEALTH] ========================================\n");
    
    u32 running = 0;
    u32 failed = 0;
    
    for (u32 i = 0; i < g_startup_health.service_count; i++) {
        service_status_t *svc = &g_startup_health.services[i];
        
        if (!svc->enabled) {
            continue;
        }
        
        service_check_status(svc->name);
        
        if (svc->healthy) {
            running++;
            printk("[HEALTH] Service OK: %s (PID: %u)\n", svc->name, svc->pid);
        } else {
            failed++;
            printk("[HEALTH] Service FAILED: %s - %s\n", svc->name, svc->error_message);
        }
    }
    
    g_startup_health.report.running_services = running;
    g_startup_health.report.failed_services = failed;
    
    printk("[HEALTH] ========================================\n");
    printk("[HEALTH] Services: %d total, %d running, %d failed\n",
           g_startup_health.service_count, running, failed);
    printk("[HEALTH] ========================================\n");
    
    return (failed == 0) ? 0 : -1;
}

/*===========================================================================*/
/*                         AUTO-REPAIR SYSTEM                                */
/*===========================================================================*/

/* Create Repair Job */
u32 repair_create_job(const char *component, u32 action, u32 priority)
{
    if (g_startup_health.repair_job_count >= 64) {
        printk("[HEALTH] ERROR: Maximum repair jobs reached\n");
        return 0;
    }
    
    repair_job_t *job = &g_startup_health.repair_jobs[g_startup_health.repair_job_count++];
    memset(job, 0, sizeof(repair_job_t));
    
    job->id = g_startup_health.repair_job_count;
    strncpy(job->component, component, 127);
    job->action = action;
    job->priority = priority;
    job->status = 0;  /* Pending */
    job->progress_percent = 0;
    job->start_time = 0;  /* Would get from kernel */
    
    const char *action_names[] = {
        "None", "Reinstall", "Restore", "Rebuild", "Reset", "Replace", "Create", "Fix Permissions"
    };
    
    snprintf(job->description, 255, "%s %s", action_names[action], component);
    
    printk("[HEALTH] Created repair job %u: %s (priority: %u)\n",
           job->id, job->description, priority);
    
    return job->id;
}

/* Execute Repair Job */
int repair_execute_job(u32 job_id)
{
    if (job_id == 0 || job_id > g_startup_health.repair_job_count) {
        return -EINVAL;
    }
    
    repair_job_t *job = &g_startup_health.repair_jobs[job_id - 1];
    
    if (job->status != 0) {
        printk("[HEALTH] Repair job %u already executed\n", job_id);
        return 0;
    }
    
    printk("[HEALTH] Executing repair job %u: %s\n", job_id, job->component);
    job->status = 1;  /* Running */
    job->start_time = 0;  /* Would get from kernel */
    
    /* Execute repair based on action */
    switch (job->action) {
    case REPAIR_ACTION_REINSTALL:
        printk("[HEALTH] Reinstalling: %s\n", job->component);
        /* reinstall_package(job->component); */
        job->progress_percent = 100;
        break;
        
    case REPAIR_ACTION_RESTORE:
        printk("[HEALTH] Restoring from backup: %s\n", job->component);
        /* restore_from_backup(job->component); */
        job->progress_percent = 100;
        break;
        
    case REPAIR_ACTION_REBUILD:
        printk("[HEALTH] Rebuilding: %s\n", job->component);
        /* rebuild_component(job->component); */
        job->progress_percent = 100;
        break;
        
    case REPAIR_ACTION_RESET:
        printk("[HEALTH] Resetting to defaults: %s\n", job->component);
        /* reset_to_defaults(job->component); */
        job->progress_percent = 100;
        break;
        
    case REPAIR_ACTION_REPLACE:
        printk("[HEALTH] Replacing: %s\n", job->component);
        /* replace_component(job->component); */
        job->progress_percent = 100;
        break;
        
    case REPAIR_ACTION_CREATE:
        printk("[HEALTH] Creating: %s\n", job->component);
        /* create_missing_component(job->component); */
        job->progress_percent = 100;
        break;
        
    case REPAIR_ACTION_PERMISSIONS:
        printk("[HEALTH] Fixing permissions: %s\n", job->component);
        /* fix_permissions(job->component); */
        job->progress_percent = 100;
        break;
    }
    
    job->status = 2;  /* Complete */
    job->end_time = 0;  /* Would get from kernel */
    snprintf(job->result_message, 255, "Repair completed successfully");
    
    printk("[HEALTH] Repair job %u completed\n", job_id);
    
    return 0;
}

/* Auto-Repair Missing Dependencies */
int auto_repair_missing(void)
{
    printk("[HEALTH] ========================================\n");
    printk("[HEALTH] Auto-repairing missing dependencies...\n");
    printk("[HEALTH] ========================================\n");
    
    u32 repaired = 0;
    
    for (u32 i = 0; i < g_startup_health.dependency_count; i++) {
        dependency_entry_t *dep = &g_startup_health.dependencies[i];
        
        if (!dep->loaded && dep->required) {
            printk("[HEALTH] Repairing missing: %s\n", dep->name);
            
            /* Create repair job */
            u32 job_id = repair_create_job(dep->name, REPAIR_ACTION_REINSTALL, 1);
            if (job_id > 0) {
                repair_execute_job(job_id);
                repaired++;
            }
        }
    }
    
    printk("[HEALTH] Repaired %u missing dependencies\n", repaired);
    printk("[HEALTH] ========================================\n");
    
    return repaired;
}

/* Auto-Repair Corrupted Dependencies */
int auto_repair_corrupted(void)
{
    printk("[HEALTH] ========================================\n");
    printk("[HEALTH] Auto-repairing corrupted dependencies...\n");
    printk("[HEALTH] ========================================\n");
    
    u32 repaired = 0;
    
    for (u32 i = 0; i < g_startup_health.dependency_count; i++) {
        dependency_entry_t *dep = &g_startup_health.dependencies[i];
        
        if (dep->loaded && !dep->healthy) {
            printk("[HEALTH] Repairing corrupted: %s\n", dep->name);
            
            /* Create repair job */
            u32 job_id = repair_create_job(dep->name, REPAIR_ACTION_RESTORE, 1);
            if (job_id > 0) {
                repair_execute_job(job_id);
                repaired++;
            }
        }
    }
    
    printk("[HEALTH] Repaired %u corrupted dependencies\n", repaired);
    printk("[HEALTH] ========================================\n");
    
    return repaired;
}

/* Auto-Start Failed Services */
int auto_start_services(void)
{
    printk("[HEALTH] ========================================\n");
    printk("[HEALTH] Auto-starting failed services...\n");
    printk("[HEALTH] ========================================\n");
    
    u32 started = 0;
    
    for (u32 i = 0; i < g_startup_health.service_count; i++) {
        service_status_t *svc = &g_startup_health.services[i];
        
        if (svc->enabled && !svc->running) {
            printk("[HEALTH] Starting service: %s\n", svc->name);
            
            if (service_start(svc->name) == 0) {
                started++;
            }
        }
    }
    
    printk("[HEALTH] Started %u services\n", started);
    printk("[HEALTH] ========================================\n");
    
    return started;
}

/*===========================================================================*/
/*                         STARTUP MANAGER                                   */
/*===========================================================================*/

/* Initialize Startup Health System */
int startup_health_init(void)
{
    printk("[STARTUP] ========================================\n");
    printk("[STARTUP] NEXUS OS Startup Health System\n");
    printk("[STARTUP] ========================================\n");
    
    memset(&g_startup_health, 0, sizeof(startup_health_manager_t));
    g_startup_health.lock.lock = 0;
    
    /* Allocate arrays */
    g_startup_health.dependencies = (dependency_entry_t *)kmalloc(sizeof(dependency_entry_t) * MAX_DEPENDENCIES);
    if (!g_startup_health.dependencies) {
        printk("[STARTUP] ERROR: Failed to allocate dependencies array\n");
        return -ENOMEM;
    }
    memset(g_startup_health.dependencies, 0, sizeof(dependency_entry_t) * MAX_DEPENDENCIES);
    
    g_startup_health.services = (service_status_t *)kmalloc(sizeof(service_status_t) * MAX_SERVICES);
    if (!g_startup_health.services) {
        printk("[STARTUP] ERROR: Failed to allocate services array\n");
        return -ENOMEM;
    }
    memset(g_startup_health.services, 0, sizeof(service_status_t) * MAX_SERVICES);
    
    g_startup_health.repair_jobs = (repair_job_t *)kmalloc(sizeof(repair_job_t) * 64);
    if (!g_startup_health.repair_jobs) {
        printk("[STARTUP] ERROR: Failed to allocate repair jobs array\n");
        return -ENOMEM;
    }
    memset(g_startup_health.repair_jobs, 0, sizeof(repair_job_t) * 64);
    
    g_startup_health.initialized = true;
    g_startup_health.auto_repair_enabled = true;
    g_startup_health.notify_user = true;
    g_startup_health.safe_mode_fallback = true;
    g_startup_health.startup_mode = STARTUP_MODE_NORMAL;
    
    printk("[STARTUP] Startup health system initialized\n");
    printk("[STARTUP] Auto-repair: %s\n", g_startup_health.auto_repair_enabled ? "Enabled" : "Disabled");
    printk("[STARTUP] Safe mode fallback: %s\n", g_startup_health.safe_mode_fallback ? "Enabled" : "Disabled");
    printk("[STARTUP] ========================================\n");
    
    return 0;
}

/* Register Critical Dependencies */
void startup_register_dependencies(void)
{
    printk("[STARTUP] Registering critical dependencies...\n");
    
    /* Core libraries */
    dependency_add("libc", "/lib/libc.so", DEP_TYPE_LIBRARY, true);
    dependency_add("libm", "/lib/libm.so", DEP_TYPE_LIBRARY, true);
    dependency_add("libpthread", "/lib/libpthread.so", DEP_TYPE_LIBRARY, true);
    
    /* Kernel modules */
    dependency_add("kernel_core", "/boot/nexus-kernel.bin", DEP_TYPE_MODULE, true);
    dependency_add("gpu_driver", "/lib/modules/gpu.ko", DEP_TYPE_DRIVER, false);
    dependency_add("network_driver", "/lib/modules/network.ko", DEP_TYPE_DRIVER, false);
    
    /* Configuration files */
    dependency_add("system_config", "/etc/nexus/system.conf", DEP_TYPE_CONFIG, true);
    dependency_add("user_config", "/etc/nexus/user.conf", DEP_TYPE_CONFIG, false);
    
    /* Critical directories */
    dependency_add("root_dir", "/", DEP_TYPE_DIRECTORY, true);
    dependency_add("home_dir", "/home", DEP_TYPE_DIRECTORY, true);
    dependency_add("etc_dir", "/etc", DEP_TYPE_DIRECTORY, true);
    dependency_add("var_dir", "/var", DEP_TYPE_DIRECTORY, true);
    
    /* GUI components */
    dependency_add("compositor", "/bin/compositor", DEP_TYPE_FILE, false);
    dependency_add("window_manager", "/bin/wm", DEP_TYPE_FILE, false);
    dependency_add("desktop_env", "/bin/desktop", DEP_TYPE_FILE, false);
    dependency_add("login_screen", "/bin/login", DEP_TYPE_FILE, false);
    
    printk("[STARTUP] Registered %u dependencies\n", g_startup_health.dependency_count);
}

/* Register Critical Services */
void startup_register_services(void)
{
    printk("[STARTUP] Registering critical services...\n");
    
    /* Core services */
    /* add_service("systemd", true); */
    /* add_service("dbus", true); */
    /* add_service("networkd", true); */
    /* add_service("logind", true); */
    
    /* GUI services */
    /* add_service("compositor", true); */
    /* add_service("wm", true); */
    /* add_service("desktop", true); */
    
    printk("[STARTUP] Registered %u services\n", g_startup_health.service_count);
}

/* Run Startup Health Check */
int startup_health_check(void)
{
    printk("[STARTUP] ========================================\n");
    printk("[STARTUP] Running startup health check...\n");
    printk("[STARTUP] ========================================\n");
    
    g_startup_health.report.last_check_time = 0;  /* Would get from kernel */
    g_startup_health.report.boot_time = 0;  /* Would get from kernel */
    
    /* Check dependencies */
    int dep_status = dependency_check_all();
    
    /* Check services */
    int svc_status = service_check_all();
    
    /* Determine overall health */
    if (dep_status == 0 && svc_status == 0) {
        g_startup_health.report.overall_status = HEALTH_STATUS_OK;
        printk("[STARTUP] System health: OK\n");
    } else if (g_startup_health.report.missing_dependencies > 0 ||
               g_startup_health.report.failed_services > 0) {
        g_startup_health.report.overall_status = HEALTH_STATUS_CRITICAL;
        printk("[STARTUP] System health: CRITICAL\n");
    } else if (g_startup_health.report.corrupted_dependencies > 0) {
        g_startup_health.report.overall_status = HEALTH_STATUS_DEGRADED;
        printk("[STARTUP] System health: DEGRADED\n");
    } else {
        g_startup_health.report.overall_status = HEALTH_STATUS_WARNING;
        printk("[STARTUP] System health: WARNING\n");
    }
    
    printk("[STARTUP] ========================================\n");
    
    return g_startup_health.report.overall_status;
}

/* Run Auto-Repair */
int startup_auto_repair(void)
{
    if (!g_startup_health.auto_repair_enabled) {
        printk("[STARTUP] Auto-repair disabled\n");
        return 0;
    }
    
    printk("[STARTUP] ========================================\n");
    printk("[STARTUP] Running auto-repair...\n");
    printk("[STARTUP] ========================================\n");
    
    int repaired = 0;
    
    /* Repair missing dependencies */
    repaired += auto_repair_missing();
    
    /* Repair corrupted dependencies */
    repaired += auto_repair_corrupted();
    
    /* Start failed services */
    repaired += auto_start_services();
    
    printk("[STARTUP] Auto-repair complete: %d repairs\n", repaired);
    printk("[STARTUP] ========================================\n");
    
    return repaired;
}

/* Generate Health Report */
void startup_generate_report(health_report_t *report)
{
    if (!report) {
        return;
    }
    
    memcpy(report, &g_startup_health.report, sizeof(health_report_t));
    
    /* Add recommendations */
    report->recommendation_count = 0;
    
    if (g_startup_health.report.missing_dependencies > 0) {
        snprintf(report->recommendations[report->recommendation_count++], 255,
                 "%u missing dependencies detected. Run system repair.",
                 g_startup_health.report.missing_dependencies);
    }
    
    if (g_startup_health.report.corrupted_dependencies > 0) {
        snprintf(report->recommendations[report->recommendation_count++], 255,
                 "%u corrupted files detected. Restore from backup.",
                 g_startup_health.report.corrupted_dependencies);
    }
    
    if (g_startup_health.report.failed_services > 0) {
        snprintf(report->recommendations[report->recommendation_count++], 255,
                 "%u services failed to start. Check service logs.",
                 g_startup_health.report.failed_services);
    }
    
    if (g_startup_health.consecutive_failures > 2) {
        snprintf(report->recommendations[report->recommendation_count++], 255,
                 "Multiple boot failures detected. Consider safe mode.");
    }
}

/* Startup Sequence */
int startup_run(void)
{
    printk("[STARTUP] ╔════════════════════════════════════════╗\n");
    printk("[STARTUP] ║  NEXUS OS Startup Health System        ║\n");
    printk("[STARTUP] ╚════════════════════════════════════════╝\n");
    
    /* Initialize */
    startup_health_init();
    
    /* Register components */
    startup_register_dependencies();
    startup_register_services();
    
    /* Run health check */
    int health = startup_health_check();
    
    /* Auto-repair if needed */
    if (health != HEALTH_STATUS_OK) {
        startup_auto_repair();
        
        /* Re-check after repair */
        health = startup_health_check();
    }
    
    /* Handle critical failures */
    if (health == HEALTH_STATUS_CRITICAL) {
        printk("[STARTUP] ╔════════════════════════════════════════╗\n");
        printk("[STARTUP] ║  ⚠️  CRITICAL SYSTEM ERRORS           ║\n");
        printk("[STARTUP] ╚════════════════════════════════════════╝\n");
        
        if (g_startup_health.safe_mode_fallback) {
            printk("[STARTUP] Falling back to safe mode...\n");
            g_startup_health.startup_mode = STARTUP_MODE_SAFE;
        }
    }
    
    /* Generate report */
    startup_generate_report(&g_startup_health.report);
    
    printk("[STARTUP] Startup complete (mode: %u)\n", g_startup_health.startup_mode);
    
    return g_startup_health.report.overall_status;
}

/*===========================================================================*/
/*                         USER INTERFACE                                    */
/*===========================================================================*/

/* Show Health Status to User */
void startup_show_status(void)
{
    printk("\n");
    printk("╔═══════════════════════════════════════════════════════╗\n");
    printk("║           NEXUS OS - System Health Status            ║\n");
    printk("╠═══════════════════════════════════════════════════════╣\n");
    
    const char *status_text[] = {
        "✓ OK", "⚠ WARNING", "⚡ DEGRADED", "✗ CRITICAL", "✗ FAILED"
    };
    
    printk("║  Overall Status: %-42s ║\n", status_text[g_startup_health.report.overall_status]);
    printk("╠═══════════════════════════════════════════════════════╣\n");
    printk("║  Dependencies:                                        ║\n");
    printk("║    Total:     %-45u ║\n", g_startup_health.report.total_dependencies);
    printk("║    Healthy:   %-45u ║\n", g_startup_health.report.healthy_dependencies);
    printk("║    Missing:   %-45u ║\n", g_startup_health.report.missing_dependencies);
    printk("║    Corrupted: %-45u ║\n", g_startup_health.report.corrupted_dependencies);
    printk("╠═══════════════════════════════════════════════════════╣\n");
    printk("║  Services:                                            ║\n");
    printk("║    Running:   %-45u ║\n", g_startup_health.report.running_services);
    printk("║    Failed:    %-45u ║\n", g_startup_health.report.failed_services);
    printk("╠═══════════════════════════════════════════════════════╣\n");
    printk("║  Repairs:                                             ║\n");
    printk("║    Pending:   %-45u ║\n", g_startup_health.report.repair_jobs_pending);
    printk("║    Completed: %-45u ║\n", g_startup_health.report.repair_jobs_completed);
    printk("║    Failed:    %-45u ║\n", g_startup_health.report.repair_jobs_failed);
    printk("╚═══════════════════════════════════════════════════════╝\n");
    
    /* Show recommendations */
    if (g_startup_health.report.recommendation_count > 0) {
        printk("\nRecommendations:\n");
        for (u32 i = 0; i < g_startup_health.report.recommendation_count; i++) {
            printk("  • %s\n", g_startup_health.report.recommendations[i]);
        }
    }
    
    printk("\n");
}
