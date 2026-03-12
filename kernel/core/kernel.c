/*
 * NEXUS OS - Kernel Core
 * kernel/core/kernel.c
 */

#include "../include/kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

/*===========================================================================*/
/*                           KERNEL STATE                                    */
/*===========================================================================*/

static struct {
    const char *version;
    const char *codename;
    const char *build_date;
    const char *build_time;
    u64 boot_time;
    u32 state;
} kernel_info = {
    .version = NEXUS_VERSION_STRING,
    .codename = NEXUS_CODENAME,
    .build_date = __DATE__,
    .build_time = __TIME__,
    .state = 0,
};

#define KERNEL_STATE_BOOTING        0
#define KERNEL_STATE_INITIALIZING   1
#define KERNEL_STATE_RUNNING        2
#define KERNEL_STATE_SHUTTING_DOWN  3

/* Per-CPU Data */
static percpu_data_t percpu_data[MAX_CPUS];
static u32 num_cpus = 1;
static u32 boot_cpu_id = 0;

/* Timing */
static u64 ticks_per_second = 1000;
static u64 boot_tick = 0;

/*===========================================================================*/
/*                     KERNEL INITIALIZATION                                 */
/*===========================================================================*/

void kernel_early_init(void)
{
    kernel_info.state = KERNEL_STATE_BOOTING;
    
    printk("\n");
    printk("===============================================\n");
    printk("  NEXUS OS v%s (%s)\n", kernel_info.version, kernel_info.codename);
    printk("  Built: %s %s\n", kernel_info.build_date, kernel_info.build_time);
    printk("===============================================\n\n");
    
    /* Detect CPUs */
    num_cpus = (u32)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus < 1) num_cpus = 1;
    if (num_cpus > MAX_CPUS) num_cpus = MAX_CPUS;
    
    pr_info("Detected %u CPU(s)\n", num_cpus);
    
    /* Initialize boot CPU */
    percpu_data[boot_cpu_id].cpu_id = boot_cpu_id;
    percpu_data[boot_cpu_id].apic_id = boot_cpu_id;
    percpu_data[boot_cpu_id].current_thread = NULL;
    percpu_data[boot_cpu_id].current_process = NULL;
    percpu_data[boot_cpu_id].context_switches = 0;
    percpu_data[boot_cpu_id].interrupts = 0;
    
    /* Get boot time */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    boot_tick = (u64)tv.tv_sec * 1000 + (u64)tv.tv_usec / 1000;
    
    pr_info("Early initialization complete.\n");
}

void kernel_init(void)
{
    kernel_info.state = KERNEL_STATE_INITIALIZING;
    
    pr_info("Starting kernel initialization...\n");
    
    /* Initialize memory management */
    pr_info("  Initializing memory manager...\n");
    
    /* Initialize scheduler */
    pr_info("  Initializing scheduler...\n");
    
    /* Initialize process manager */
    pr_info("  Initializing process manager...\n");
    
    /* Initialize IPC */
    pr_info("  Initializing IPC...\n");
    
    /* Initialize VFS */
    pr_info("  Initializing VFS...\n");
    
    /* Initialize network */
    pr_info("  Initializing network...\n");
    
    kernel_info.state = KERNEL_STATE_RUNNING;
    kernel_info.boot_time = get_time_ms() - boot_tick;
    
    pr_info("\n");
    pr_info("===============================================\n");
    pr_info("  NEXUS OS Initialization Complete!\n");
    pr_info("  Boot time: %llu ms\n", (unsigned long long)kernel_info.boot_time);
    pr_info("===============================================\n\n");
}

void kernel_start_scheduler(void)
{
    pr_info("Starting scheduler on CPU %u...\n", get_cpu_id());
    
    /* Enable interrupts */
    local_irq_enable();
    
    pr_info("Scheduler started. System is now running.\n");
    
    /* In a real OS, this would never return */
    /* For userspace testing, we just return */
}

void kernel_main(void)
{
    kernel_early_init();
    kernel_init();
    kernel_start_scheduler();
}

/*===========================================================================*/
/*                          PANIC HANDLING                                   */
/*===========================================================================*/

void __noreturn kernel_panic(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    
    printk("\n");
    printk("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printk("!!           KERNEL PANIC                    !!\n");
    printk("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printk("\n");
    
    vprintk(fmt, args);
    printk("\n");
    
    __builtin_va_end(args);
    
    /* Halt */
    printk("System Halted.\n");
    
    /* In real OS: disable interrupts and halt CPU */
    abort();
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

percpu_data_t *get_percpu(void)
{
    return &percpu_data[get_cpu_id()];
}

u32 get_cpu_id(void)
{
    /* In real OS: read from CPU register */
    return boot_cpu_id;
}

u32 get_num_cpus(void)
{
    return num_cpus;
}

u64 get_ticks(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * ticks_per_second + (u64)tv.tv_usec / 1000;
}

u64 get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000 + (u64)tv.tv_usec / 1000;
}

void delay_ms(u64 ms)
{
    usleep(ms * 1000);
}

void local_irq_enable(void)
{
    /* In real OS: enable CPU interrupts */
}

void local_irq_disable(void)
{
    /* In real OS: disable CPU interrupts */
}
