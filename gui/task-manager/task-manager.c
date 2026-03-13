/*
 * NEXUS OS - Task Manager
 * gui/task-manager/task-manager.c
 *
 * Process monitoring, resource usage, task management
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../gui.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         TASK MANAGER CONFIGURATION                        */
/*===========================================================================*/

#define TASK_MAX_PROCESSES        1024
#define TASK_MAX_HISTORY          100
#define TASK_REFRESH_INTERVAL     1000  /* ms */

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 pid;
    u32 ppid;
    char name[64];
    char user[32];
    u32 state;                  /* Running, Sleeping, Stopped, Zombie */
    u32 priority;
    u32 nice;
    u64 cpu_time;
    u64 memory;
    u64 virtual_memory;
    u32 threads;
    u64 start_time;
    char command[256];
    float cpu_percent;
    float memory_percent;
} process_info_t;

typedef struct {
    u64 timestamp;
    float cpu_usage;
    float memory_usage;
    u32 process_count;
    u64 disk_read;
    u64 disk_write;
    u64 network_rx;
    u64 network_tx;
} system_history_t;

typedef struct {
    bool is_open;
    rect_t window_rect;
    process_info_t *processes;
    u32 process_count;
    u32 selected_process;
    u32 sort_column;
    bool sort_ascending;
    u32 filter_state;
    char filter_text[64];
    system_history_t history[TASK_MAX_HISTORY];
    u32 history_index;
    float current_cpu_usage;
    float current_memory_usage;
    float current_swap_usage;
    u32 total_processes;
    u32 running_processes;
    u32 total_threads;
    u64 uptime;
    u64 total_memory;
    u64 used_memory;
    u64 total_swap;
    u64 used_swap;
    u64 disk_read_total;
    u64 disk_write_total;
    u64 network_rx_total;
    u64 network_tx_total;
    void (*on_process_kill)(u32 pid);
    void (*on_process_priority)(u32 pid, u32 priority);
} task_manager_t;

static task_manager_t g_taskmgr;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int task_manager_init(void)
{
    printk("[TASKMGR] Initializing task manager...\n");
    
    memset(&g_taskmgr, 0, sizeof(task_manager_t));
    g_taskmgr.window_rect.width = 900;
    g_taskmgr.window_rect.height = 600;
    
    /* Allocate process array */
    g_taskmgr.processes = kmalloc(sizeof(process_info_t) * TASK_MAX_PROCESSES);
    if (!g_taskmgr.processes) {
        return -ENOMEM;
    }
    
    g_taskmgr.sort_column = 2;  /* Sort by memory by default */
    g_taskmgr.sort_ascending = false;
    
    printk("[TASKMGR] Task manager initialized\n");
    return 0;
}

int task_manager_shutdown(void)
{
    printk("[TASKMGR] Shutting down task manager...\n");
    
    if (g_taskmgr.processes) {
        kfree(g_taskmgr.processes);
    }
    
    g_taskmgr.is_open = false;
    return 0;
}

/*===========================================================================*/
/*                         PROCESS ENUMERATION                               */
/*===========================================================================*/

int task_manager_refresh(void)
{
    /* In real implementation, would read from /proc or kernel */
    /* Mock: create sample processes */
    
    g_taskmgr.process_count = 0;
    g_taskmgr.running_processes = 0;
    g_taskmgr.total_threads = 0;
    
    /* System idle */
    process_info_t *proc = &g_taskmgr.processes[g_taskmgr.process_count++];
    proc->pid = 0;
    proc->ppid = 0;
    strcpy(proc->name, "[kworker]");
    strcpy(proc->user, "root");
    proc->state = 1;  /* Sleeping */
    proc->priority = 20;
    proc->cpu_time = ktime_get_sec() * 1000;
    proc->memory = 0;
    proc->threads = 1;
    proc->cpu_percent = 0.5f;
    proc->memory_percent = 0.0f;
    
    /* Init */
    proc = &g_taskmgr.processes[g_taskmgr.process_count++];
    proc->pid = 1;
    proc->ppid = 0;
    strcpy(proc->name, "init");
    strcpy(proc->user, "root");
    proc->state = 1;
    proc->priority = 20;
    proc->cpu_time = ktime_get_sec() * 500;
    proc->memory = 4 * 1024 * 1024;
    proc->threads = 1;
    proc->cpu_percent = 0.1f;
    proc->memory_percent = 0.05f;
    g_taskmgr.running_processes++;
    g_taskmgr.total_threads++;
    
    /* System services */
    proc = &g_taskmgr.processes[g_taskmgr.process_count++];
    proc->pid = 2;
    proc->ppid = 1;
    strcpy(proc->name, "systemd");
    strcpy(proc->user, "root");
    proc->state = 1;
    proc->priority = 20;
    proc->cpu_time = ktime_get_sec() * 800;
    proc->memory = 12 * 1024 * 1024;
    proc->threads = 3;
    proc->cpu_percent = 0.3f;
    proc->memory_percent = 0.15f;
    g_taskmgr.running_processes++;
    g_taskmgr.total_threads += 3;
    
    /* Desktop environment */
    proc = &g_taskmgr.processes[g_taskmgr.process_count++];
    proc->pid = 100;
    proc->ppid = 2;
    strcpy(proc->name, "desktop-env");
    strcpy(proc->user, "user");
    proc->state = 1;
    proc->priority = 20;
    proc->cpu_time = ktime_get_sec() * 2000;
    proc->memory = 128 * 1024 * 1024;
    proc->threads = 8;
    proc->cpu_percent = 2.5f;
    proc->memory_percent = 1.6f;
    g_taskmgr.running_processes++;
    g_taskmgr.total_threads += 8;
    
    /* Web browser */
    proc = &g_taskmgr.processes[g_taskmgr.process_count++];
    proc->pid = 200;
    proc->ppid = 2;
    strcpy(proc->name, "browser");
    strcpy(proc->user, "user");
    proc->state = 0;  /* Running */
    proc->priority = 20;
    proc->cpu_time = ktime_get_sec() * 5000;
    proc->memory = 512 * 1024 * 1024;
    proc->threads = 24;
    proc->cpu_percent = 5.2f;
    proc->memory_percent = 6.4f;
    g_taskmgr.running_processes++;
    g_taskmgr.total_threads += 24;
    
    /* Terminal */
    proc = &g_taskmgr.processes[g_taskmgr.process_count++];
    proc->pid = 300;
    proc->ppid = 2;
    strcpy(proc->name, "terminal");
    strcpy(proc->user, "user");
    proc->state = 1;
    proc->priority = 20;
    proc->cpu_time = ktime_get_sec() * 100;
    proc->memory = 32 * 1024 * 1024;
    proc->threads = 2;
    proc->cpu_percent = 0.1f;
    proc->memory_percent = 0.4f;
    g_taskmgr.total_threads += 2;
    
    /* Update totals */
    g_taskmgr.total_processes = g_taskmgr.process_count;
    g_taskmgr.current_cpu_usage = 8.7f;
    g_taskmgr.current_memory_usage = 8.65f;
    g_taskmgr.total_memory = 8ULL * 1024 * 1024 * 1024;
    g_taskmgr.used_memory = (u64)(g_taskmgr.total_memory * g_taskmgr.current_memory_usage / 100);
    g_taskmgr.uptime = ktime_get_sec();
    
    /* Add to history */
    u32 idx = g_taskmgr.history_index++ % TASK_MAX_HISTORY;
    system_history_t *hist = &g_taskmgr.history[idx];
    hist->timestamp = ktime_get_sec();
    hist->cpu_usage = g_taskmgr.current_cpu_usage;
    hist->memory_usage = g_taskmgr.current_memory_usage;
    hist->process_count = g_taskmgr.process_count;
    
    return g_taskmgr.process_count;
}

/*===========================================================================*/
/*                         PROCESS MANAGEMENT                                */
/*===========================================================================*/

int task_manager_kill_process(u32 pid)
{
    printk("[TASKMGR] Killing process %d...\n", pid);
    
    /* In real implementation, would send SIGKILL */
    
    if (g_taskmgr.on_process_kill) {
        g_taskmgr.on_process_kill(pid);
    }
    
    return 0;
}

int task_manager_terminate_process(u32 pid)
{
    printk("[TASKMGR] Terminating process %d...\n", pid);
    
    /* In real implementation, would send SIGTERM */
    
    return 0;
}

int task_manager_set_priority(u32 pid, u32 priority)
{
    printk("[TASKMGR] Setting priority for process %d: %d\n", pid, priority);
    
    /* In real implementation, would use setpriority() */
    
    if (g_taskmgr.on_process_priority) {
        g_taskmgr.on_process_priority(pid, priority);
    }
    
    return 0;
}

int task_manager_suspend_process(u32 pid)
{
    printk("[TASKMGR] Suspending process %d...\n", pid);
    
    /* In real implementation, would send SIGSTOP */
    
    return 0;
}

int task_manager_resume_process(u32 pid)
{
    printk("[TASKMGR] Resuming process %d...\n", pid);
    
    /* In real implementation, would send SIGCONT */
    
    return 0;
}

/*===========================================================================*/
/*                         SORTING & FILTERING                               */
/*===========================================================================*/

int task_manager_sort(u32 column, bool ascending)
{
    g_taskmgr.sort_column = column;
    g_taskmgr.sort_ascending = ascending;
    
    /* In real implementation, would sort process array */
    /* Using simple bubble sort for mock */
    
    printk("[TASKMGR] Sorted by column %d (%s)\n", 
           column, ascending ? "ascending" : "descending");
    
    return 0;
}

int task_manager_filter(u32 state, const char *text)
{
    g_taskmgr.filter_state = state;
    if (text) {
        strncpy(g_taskmgr.filter_text, text, sizeof(g_taskmgr.filter_text) - 1);
    }
    
    printk("[TASKMGR] Filter applied: state=%d, text=%s\n", state, text);
    
    /* Would filter process list */
    return 0;
}

/*===========================================================================*/
/*                         SYSTEM INFO                                       */
/*===========================================================================*/

int task_manager_get_system_info(float *cpu, float *memory, u32 *processes, u64 *uptime)
{
    if (cpu) *cpu = g_taskmgr.current_cpu_usage;
    if (memory) *memory = g_taskmgr.current_memory_usage;
    if (processes) *processes = g_taskmgr.total_processes;
    if (uptime) *uptime = g_taskmgr.uptime;
    
    return 0;
}

int task_manager_get_process_info(u32 pid, process_info_t *info)
{
    if (!info) return -EINVAL;
    
    for (u32 i = 0; i < g_taskmgr.process_count; i++) {
        if (g_taskmgr.processes[i].pid == pid) {
            *info = g_taskmgr.processes[i];
            return 0;
        }
    }
    
    return -ENOENT;
}

/*===========================================================================*/
/*                         RENDERING                                         */
/*===========================================================================*/

int task_manager_render(void *surface)
{
    if (!g_taskmgr.is_open || !surface) return -EINVAL;
    
    /* Refresh data */
    task_manager_refresh();
    
    /* Render window frame */
    /* Render menu bar */
    /* Render process list */
    /* Render CPU graph */
    /* Render memory graph */
    /* Render status bar */
    
    (void)surface;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

int task_manager_open(void)
{
    g_taskmgr.is_open = true;
    task_manager_refresh();
    printk("[TASKMGR] Opened\n");
    return 0;
}

int task_manager_close(void)
{
    g_taskmgr.is_open = false;
    printk("[TASKMGR] Closed\n");
    return 0;
}

task_manager_t *task_manager_get(void)
{
    return &g_taskmgr;
}

const char *process_get_state_name(u32 state)
{
    switch (state) {
        case 0: return "Running";
        case 1: return "Sleeping";
        case 2: return "Stopped";
        case 3: return "Zombie";
        default: return "Unknown";
    }
}
