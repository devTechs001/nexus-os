/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2024 NEXUS Development Team
 *
 * smp.c - Symmetric Multiprocessing Support
 */

#include "../include/kernel.h"

/*===========================================================================*/
/*                         SMP CONFIGURATION                                 */
/*===========================================================================*/

#ifdef CONFIG_SMP

/*===========================================================================*/
/*                         SMP DATA STRUCTURES                               */
/*===========================================================================*/

/**
 * cpu_info - Per-CPU information
 */
typedef struct cpu_info {
    u32 cpu_id;
    u32 apic_id;
    u32 acpi_id;
    u32 flags;
    
    /* CPU State */
    volatile u32 state;
    void *stack;
    void *irq_stack;
    
    /* Statistics */
    u64 ticks;
    u64 idle_ticks;
    u64 kernel_ticks;
    u64 user_ticks;
    
    /* Per-CPU data */
    percpu_data_t *percpu;
} cpu_info_t;

/* CPU States */
#define CPU_STATE_STOPPED   0
#define CPU_STATE_STARTING  1
#define CPU_STATE_RUNNING   2
#define CPU_STATE_OFFLINE   3
#define CPU_STATE_DEAD      4

/* CPU Flags */
#define CPU_FLAG_PRESENT    0x00000001
#define CPU_FLAG_ENABLED    0x00000002
#define CPU_FLAG_BOOT       0x00000004
#define CPU_FLAG_HLT        0x00000008
#define CPU_FLAG_MWAIT      0x00000010

/*===========================================================================*/
/*                         GLOBAL SMP VARIABLES                              */
/*===========================================================================*/

static cpu_info_t cpu_info[MAX_CPUS];
static u32 num_present_cpus = 0;
static u32 num_online_cpus = 0;
static u32 boot_cpu = 0;

static spinlock_t cpu_lock = SPINLOCK_UNLOCKED;

/*===========================================================================*/
/*                         SMP INITIALIZATION                                */
/*===========================================================================*/

/**
 * smp_init - Initialize SMP subsystem
 */
void smp_init(void)
{
    pr_info("Initializing SMP subsystem...\n");
    
    /* Initialize boot CPU */
    cpu_info[boot_cpu].cpu_id = boot_cpu;
    cpu_info[boot_cpu].apic_id = boot_cpu;
    cpu_info[boot_cpu].flags = CPU_FLAG_PRESENT | CPU_FLAG_ENABLED | CPU_FLAG_BOOT;
    cpu_info[boot_cpu].state = CPU_STATE_RUNNING;
    
    num_present_cpus = 1;
    num_online_cpus = 1;
    
    pr_info("Boot CPU: %u (APIC ID: %u)\n", boot_cpu, cpu_info[boot_cpu].apic_id);
}

/**
 * smp_prepare_cpus - Prepare CPUs for startup
 */
void smp_prepare_cpus(void)
{
    pr_info("Preparing CPUs for startup...\n");
    
    /* Detect and enumerate CPUs */
    arch_smp_detect();
    
    pr_info("Found %u CPU(s)\n", num_present_cpus);
}

/**
 * smp_cpus_done - CPUs startup complete
 */
void smp_cpus_done(void)
{
    pr_info("All CPUs started (%u online)\n", num_online_cpus);
    
    /* Enable scheduler on all CPUs */
    scheduler_enable_all();
}

/*===========================================================================*/
/*                         CPU HOTPLUG                                       */
/*===========================================================================*/

/**
 * cpu_up - Bring up a CPU
 */
int cpu_up(u32 cpu)
{
    int ret;
    
    if (cpu >= MAX_CPUS) {
        return -EINVAL;
    }
    
    spin_lock(&cpu_lock);
    
    if (cpu_info[cpu].state != CPU_STATE_OFFLINE) {
        spin_unlock(&cpu_lock);
        return -EBUSY;
    }
    
    cpu_info[cpu].state = CPU_STATE_STARTING;
    spin_unlock(&cpu_lock);
    
    pr_info("Bringing up CPU %u...\n", cpu);
    
    /* Architecture-specific CPU startup */
    ret = arch_cpu_up(cpu);
    if (ret) {
        cpu_info[cpu].state = CPU_STATE_OFFLINE;
        return ret;
    }
    
    spin_lock(&cpu_lock);
    cpu_info[cpu].state = CPU_STATE_RUNNING;
    num_online_cpus++;
    spin_unlock(&cpu_lock);
    
    pr_info("CPU %u is now online\n", cpu);
    
    return 0;
}

/**
 * cpu_down - Take down a CPU
 */
int cpu_down(u32 cpu)
{
    int ret;
    
    if (cpu >= MAX_CPUS) {
        return -EINVAL;
    }
    
    /* Don't offline boot CPU */
    if (cpu == boot_cpu) {
        return -EPERM;
    }
    
    spin_lock(&cpu_lock);
    
    if (cpu_info[cpu].state != CPU_STATE_RUNNING) {
        spin_unlock(&cpu_lock);
        return -EBUSY;
    }
    
    cpu_info[cpu].state = CPU_STATE_OFFLINE;
    spin_unlock(&cpu_lock);
    
    pr_info("Taking down CPU %u...\n", cpu);
    
    /* Migrate tasks off this CPU */
    scheduler_migrate_tasks(cpu);
    
    /* Architecture-specific CPU shutdown */
    ret = arch_cpu_down(cpu);
    if (ret) {
        spin_lock(&cpu_lock);
        cpu_info[cpu].state = CPU_STATE_RUNNING;
        spin_unlock(&cpu_lock);
        return ret;
    }
    
    spin_lock(&cpu_lock);
    num_online_cpus--;
    spin_unlock(&cpu_lock);
    
    pr_info("CPU %u is now offline\n", cpu);
    
    return 0;
}

/*===========================================================================*/
/*                         IPI (Inter-Processor Interrupt)                   */
/*===========================================================================*/

/**
 * IPI Message Types
 */
typedef enum ipi_type {
    IPI_RESCHEDULE      = 0,
    IPI_CALL_FUNC       = 1,
    IPI_CPU_STOP        = 2,
    IPI_CPU_HALT        = 3,
    IPI_WAKEUP          = 4,
    IPI_INVALIDATE_TLB  = 5,
} ipi_type_t;

/**
 * IPI Message
 */
typedef struct ipi_message {
    u32 type;
    void *data;
    u32 flags;
} ipi_message_t;

/**
 * arch_send_ipi - Send IPI to specific CPU
 */
void arch_send_ipi(u32 cpu, u32 vector);

/**
 * send_ipi - Send IPI to CPU
 */
void send_ipi(u32 cpu, ipi_type_t type)
{
    if (cpu >= MAX_CPUS || cpu == get_cpu_id()) {
        return;
    }
    
    arch_send_ipi(cpu, type);
}

/**
 * send_ipi_all - Send IPI to all CPUs
 */
void send_ipi_all(ipi_type_t type)
{
    u32 cpu;
    
    for (cpu = 0; cpu < MAX_CPUS; cpu++) {
        if (cpu != get_cpu_id() && cpu_info[cpu].state == CPU_STATE_RUNNING) {
            send_ipi(cpu, type);
        }
    }
}

/**
 * send_ipi_mask - Send IPI to CPU mask
 */
void send_ipi_mask(u64 mask, ipi_type_t type)
{
    u32 cpu;
    
    for (cpu = 0; cpu < MAX_CPUS; cpu++) {
        if ((mask & (1ULL << cpu)) && cpu != get_cpu_id()) {
            send_ipi(cpu, type);
        }
    }
}

/*===========================================================================*/
/*                         SMP SCHEDULER SUPPORT                             */
/*===========================================================================*/

/**
 * scheduler_enable_all - Enable scheduler on all CPUs
 */
void scheduler_enable_all(void)
{
    u32 cpu;
    
    for (cpu = 0; cpu < MAX_CPUS; cpu++) {
        if (cpu_info[cpu].state == CPU_STATE_RUNNING) {
            /* Enable scheduler on this CPU */
            arch_scheduler_enable(cpu);
        }
    }
}

/**
 * scheduler_migrate_tasks - Migrate tasks from CPU
 */
void scheduler_migrate_tasks(u32 cpu)
{
    /* Migrate runnable tasks to other CPUs */
    pr_debug("Migrating tasks from CPU %u\n", cpu);
    
    /* Architecture-specific migration */
    arch_scheduler_migrate(cpu);
}

/*===========================================================================*/
/*                         CPU TOPOLOGY                                      */
/*===========================================================================*/

/**
 * cpu_topology - CPU topology information
 */
typedef struct cpu_topology {
    u32 package_id;
    u32 core_id;
    u32 thread_id;
    u32 numa_node;
} cpu_topology_t;

static cpu_topology_t cpu_topology[MAX_CPUS];

/**
 * topology_init - Initialize CPU topology
 */
void topology_init(void)
{
    u32 cpu;
    
    pr_info("Initializing CPU topology...\n");
    
    for (cpu = 0; cpu < num_present_cpus; cpu++) {
        /* Parse topology from ACPI/Device Tree */
        arch_topology_parse(cpu, &cpu_topology[cpu]);
        
        pr_debug("CPU %u: Package %u, Core %u, Thread %u, NUMA %u\n",
                 cpu,
                 cpu_topology[cpu].package_id,
                 cpu_topology[cpu].core_id,
                 cpu_topology[cpu].thread_id,
                 cpu_topology[cpu].numa_node);
    }
}

/**
 * topology_physical_package_id - Get physical package ID
 */
u32 topology_physical_package_id(u32 cpu)
{
    if (cpu >= MAX_CPUS) {
        return 0;
    }
    return cpu_topology[cpu].package_id;
}

/**
 * topology_core_id - Get core ID
 */
u32 topology_core_id(u32 cpu)
{
    if (cpu >= MAX_CPUS) {
        return 0;
    }
    return cpu_topology[cpu].core_id;
}

/**
 * topology_thread_id - Get thread ID
 */
u32 topology_thread_id(u32 cpu)
{
    if (cpu >= MAX_CPUS) {
        return 0;
    }
    return cpu_topology[cpu].thread_id;
}

/**
 * topology_numa_node - Get NUMA node
 */
u32 topology_numa_node(u32 cpu)
{
    if (cpu >= MAX_CPUS) {
        return 0;
    }
    return cpu_topology[cpu].numa_node;
}

/*===========================================================================*/
/*                         CPU STATISTICS                                    */
/*===========================================================================*/

/**
 * cpu_get_stats - Get CPU statistics
 */
void cpu_get_stats(u32 cpu, u64 *ticks, u64 *idle, u64 *kernel, u64 *user)
{
    if (cpu >= MAX_CPUS) {
        return;
    }
    
    if (ticks) *ticks = cpu_info[cpu].ticks;
    if (idle) *idle = cpu_info[cpu].idle_ticks;
    if (kernel) *kernel = cpu_info[cpu].kernel_ticks;
    if (user) *user = cpu_info[cpu].user_ticks;
}

/**
 * cpu_update_stats - Update CPU statistics
 */
void cpu_update_stats(u32 cpu, u64 idle_delta, u64 kernel_delta, u64 user_delta)
{
    if (cpu >= MAX_CPUS) {
        return;
    }
    
    cpu_info[cpu].idle_ticks += idle_delta;
    cpu_info[cpu].kernel_ticks += kernel_delta;
    cpu_info[cpu].user_ticks += user_delta;
    cpu_info[cpu].ticks += idle_delta + kernel_delta + user_delta;
}

/*===========================================================================*/
/*                         SMP DEBUGGING                                     */
/*===========================================================================*/

#ifdef CONFIG_DEBUG_SMP

/**
 * smp_debug_print - Print SMP debug information
 */
void smp_debug_print(void)
{
    u32 cpu;
    
    printk("\n=== SMP Debug Information ===\n");
    printk("Present CPUs: %u\n", num_present_cpus);
    printk("Online CPUs: %u\n", num_online_cpus);
    printk("\n");
    
    for (cpu = 0; cpu < MAX_CPUS; cpu++) {
        if (cpu_info[cpu].flags & CPU_FLAG_PRESENT) {
            printk("CPU %u: APIC ID %u, State %u, Flags 0x%x\n",
                   cpu_info[cpu].cpu_id,
                   cpu_info[cpu].apic_id,
                   cpu_info[cpu].state,
                   cpu_info[cpu].flags);
        }
    }
    
    printk("\n");
}

#endif /* CONFIG_DEBUG_SMP */

/*===========================================================================*/
/*                         ARCHITECTURE INTERFACES                           */
/*===========================================================================*/

/* These functions must be implemented by architecture-specific code */

extern void arch_smp_detect(void);
extern int arch_cpu_up(u32 cpu);
extern int arch_cpu_down(u32 cpu);
extern void arch_send_ipi(u32 cpu, u32 vector);
extern void arch_scheduler_enable(u32 cpu);
extern void arch_scheduler_migrate(u32 cpu);
extern void arch_topology_parse(u32 cpu, cpu_topology_t *topology);

#endif /* CONFIG_SMP */
