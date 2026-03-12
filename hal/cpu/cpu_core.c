/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/cpu/cpu_core.c - Core CPU Management
 *
 * Implements CPU initialization, detection, enable/disable functionality
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         CPU CORE DATA STRUCTURES                          */
/*===========================================================================*/

/**
 * cpu_core_data_t - Per-CPU core data
 */
typedef struct cpu_core_data {
    cpu_info_t info;                    /* CPU information */
    spinlock_t lock;                    /* Protection lock */
    bool initialized;                   /* Initialization flag */
    u64 boot_time;                      /* CPU boot timestamp */
    u64 last_schedule;                  /* Last schedule time */
    u64 idle_time;                      /* Total idle time */
    u64 active_time;                    /* Total active time */
} cpu_core_data_t;

/**
 * cpu_manager_t - CPU manager global data
 */
typedef struct cpu_manager {
    cpu_core_data_t cpus[MAX_CPUS];     /* CPU array */
    u32 num_cpus;                       /* Total CPU count */
    u32 num_present;                    /* Present CPU count */
    u32 num_online;                     /* Online CPU count */
    u32 boot_cpu;                       /* Boot CPU ID */
    spinlock_t lock;                    /* Global lock */
    bool initialized;                   /* Manager initialized */
} cpu_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static cpu_manager_t cpu_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         INTERNAL FUNCTIONS                                */
/*===========================================================================*/

/**
 * cpu_core_lock - Lock CPU core data
 * @cpu_id: CPU ID to lock
 */
static inline void cpu_core_lock(u32 cpu_id)
{
    if (likely(cpu_id < MAX_CPUS)) {
        spin_lock(&cpu_manager.cpus[cpu_id].lock);
    }
}

/**
 * cpu_core_unlock - Unlock CPU core data
 * @cpu_id: CPU ID to unlock
 */
static inline void cpu_core_unlock(u32 cpu_id)
{
    if (likely(cpu_id < MAX_CPUS)) {
        spin_unlock(&cpu_manager.cpus[cpu_id].lock);
    }
}

/**
 * cpu_validate_id - Validate CPU ID
 * @cpu_id: CPU ID to validate
 *
 * Returns: 0 on success, -EINVAL on failure
 */
static inline int cpu_validate_id(u32 cpu_id)
{
    if (unlikely(cpu_id >= MAX_CPUS)) {
        pr_err("CPU: Invalid CPU ID %u (max %u)\n", cpu_id, MAX_CPUS - 1);
        return -EINVAL;
    }
    return 0;
}

/**
 * cpu_arch_detect - Architecture-specific CPU detection
 * @cpu_id: CPU ID to detect
 *
 * Returns: 0 on success, negative error code on failure
 */
static int cpu_arch_detect(u32 cpu_id)
{
    cpu_core_data_t *core = &cpu_manager.cpus[cpu_id];
    int ret;

    /* Call architecture-specific detection */
#if defined(ARCH_X86_64)
    ret = x86_cpu_detect(cpu_id);
#elif defined(ARCH_ARM64)
    ret = arm_cpu_detect(cpu_id);
#else
    ret = cpu_detect(cpu_id);
#endif

    if (ret == 0) {
        core->info.present = true;
        atomic_inc(&cpu_manager.num_present);
    }

    return ret;
}

/**
 * cpu_arch_enable - Architecture-specific CPU enable
 * @cpu_id: CPU ID to enable
 *
 * Returns: 0 on success, negative error code on failure
 */
static int cpu_arch_enable(u32 cpu_id)
{
    int ret;

#if defined(ARCH_X86_64)
    ret = x86_cpu_enable(cpu_id);
#elif defined(ARCH_ARM64)
    ret = arm_cpu_enable(cpu_id);
#else
    ret = cpu_enable(cpu_id);
#endif

    return ret;
}

/**
 * cpu_arch_disable - Architecture-specific CPU disable
 * @cpu_id: CPU ID to disable
 *
 * Returns: 0 on success, negative error code on failure
 */
static int cpu_arch_disable(u32 cpu_id)
{
    int ret;

#if defined(ARCH_X86_64)
    ret = x86_cpu_disable(cpu_id);
#elif defined(ARCH_ARM64)
    ret = arm_cpu_disable(cpu_id);
#else
    ret = cpu_disable(cpu_id);
#endif

    return ret;
}

/*===========================================================================*/
/*                         CPU INITIALIZATION                                */
/*===========================================================================*/

/**
 * cpu_early_init - Early CPU initialization
 *
 * Called during early boot to initialize the boot CPU.
 * This function runs before memory management is fully initialized.
 */
void cpu_early_init(void)
{
    cpu_core_data_t *boot_core;
    u32 boot_cpu_id;

    pr_info("CPU: Early initialization...\n");

    /* Initialize CPU manager */
    spin_lock_init_named(&cpu_manager.lock, "cpu_manager");
    cpu_manager.boot_cpu = 0;
    cpu_manager.num_cpus = 1;
    cpu_manager.num_present = 0;
    cpu_manager.num_online = 0;
    cpu_manager.initialized = false;

    /* Initialize boot CPU */
    boot_cpu_id = cpu_manager.boot_cpu;
    boot_core = &cpu_manager.cpus[boot_cpu_id];

    spin_lock_init_named(&boot_core->lock, "cpu0_core");
    boot_core->info.cpu_id = boot_cpu_id;
    boot_core->info.online = false;
    boot_core->info.present = false;
    boot_core->info.enabled = false;
    boot_core->initialized = true;
    boot_core->boot_time = get_ticks();

    /* Detect boot CPU */
    cpu_arch_detect(boot_cpu_id);

    pr_info("CPU: Boot CPU %u detected\n", boot_cpu_id);
}

/**
 * cpu_init - Main CPU initialization
 *
 * Called during kernel initialization to set up all CPUs.
 */
void cpu_init(void)
{
    u32 i;
    int ret;

    pr_info("CPU: Initializing CPU subsystem...\n");

    spin_lock(&cpu_manager.lock);

    /* Detect all possible CPUs */
    for (i = 0; i < MAX_CPUS; i++) {
        cpu_core_data_t *core = &cpu_manager.cpus[i];

        if (i == cpu_manager.boot_cpu) {
            continue; /* Already initialized */
        }

        spin_lock_init_named(&core->lock, "cpu_core");
        core->info.cpu_id = i;
        core->info.online = false;
        core->info.present = false;
        core->info.enabled = false;
        core->initialized = true;

        /* Try to detect CPU */
        ret = cpu_arch_detect(i);
        if (ret == 0) {
            cpu_manager.num_cpus++;
            pr_debug("CPU: CPU %u detected\n", i);
        }
    }

    cpu_manager.initialized = true;
    spin_unlock(&cpu_manager.lock);

    pr_info("CPU: %u CPUs detected (%u present)\n",
            cpu_manager.num_cpus, cpu_manager.num_present);
}

/**
 * cpu_late_init - Late CPU initialization
 *
 * Called after memory and interrupt subsystems are initialized.
 * Brings up secondary CPUs.
 */
void cpu_late_init(void)
{
    u32 i;
    int ret;

    pr_info("CPU: Late initialization (bringing up secondary CPUs)...\n");

    /* Bring up all present CPUs */
    for (i = 0; i < MAX_CPUS; i++) {
        cpu_core_data_t *core = &cpu_manager.cpus[i];

        if (i == cpu_manager.boot_cpu) {
            continue; /* Boot CPU already online */
        }

        if (!core->info.present) {
            continue;
        }

        ret = cpu_online(i);
        if (ret == 0) {
            pr_info("CPU: CPU %u brought online\n", i);
        } else {
            pr_err("CPU: Failed to bring up CPU %u (error %d)\n", i, ret);
        }
    }

    pr_info("CPU: %u CPUs online\n", cpu_manager.num_online);
}

/*===========================================================================*/
/*                         CPU DETECTION                                     */
/*===========================================================================*/

/**
 * cpu_detect - Detect CPU information
 * @cpu_id: CPU ID to detect
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_detect(u32 cpu_id)
{
    cpu_core_data_t *core;
    int ret;

    ret = cpu_validate_id(cpu_id);
    if (ret < 0) {
        return ret;
    }

    core = &cpu_manager.cpus[cpu_id];

    spin_lock(&core->lock);

    if (core->info.present) {
        spin_unlock(&core->lock);
        return 0; /* Already detected */
    }

    /* Perform architecture-specific detection */
    ret = cpu_arch_detect(cpu_id);

    spin_unlock(&core->lock);

    return ret;
}

/*===========================================================================*/
/*                         CPU ENABLE/DISABLE                                */
/*===========================================================================*/

/**
 * cpu_enable - Enable a CPU
 * @cpu_id: CPU ID to enable
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_enable(u32 cpu_id)
{
    cpu_core_data_t *core;
    int ret;

    ret = cpu_validate_id(cpu_id);
    if (ret < 0) {
        return ret;
    }

    core = &cpu_manager.cpus[cpu_id];

    spin_lock(&core->lock);

    if (!core->info.present) {
        spin_unlock(&core->lock);
        return -ENODEV;
    }

    if (core->info.enabled) {
        spin_unlock(&core->lock);
        return 0; /* Already enabled */
    }

    /* Architecture-specific enable */
    ret = cpu_arch_enable(cpu_id);
    if (ret == 0) {
        core->info.enabled = true;
        pr_debug("CPU: CPU %u enabled\n", cpu_id);
    }

    spin_unlock(&core->lock);

    return ret;
}

/**
 * cpu_disable - Disable a CPU
 * @cpu_id: CPU ID to disable
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_disable(u32 cpu_id)
{
    cpu_core_data_t *core;
    int ret;

    ret = cpu_validate_id(cpu_id);
    if (ret < 0) {
        return ret;
    }

    /* Cannot disable boot CPU */
    if (cpu_id == cpu_manager.boot_cpu) {
        pr_err("CPU: Cannot disable boot CPU %u\n", cpu_id);
        return -EBUSY;
    }

    core = &cpu_manager.cpus[cpu_id];

    spin_lock(&core->lock);

    if (!core->info.enabled) {
        spin_unlock(&core->lock);
        return 0; /* Already disabled */
    }

    /* Architecture-specific disable */
    ret = cpu_arch_disable(cpu_id);
    if (ret == 0) {
        core->info.enabled = false;
        pr_debug("CPU: CPU %u disabled\n", cpu_id);
    }

    spin_unlock(&core->lock);

    return ret;
}

/**
 * cpu_online - Bring a CPU online
 * @cpu_id: CPU ID to bring online
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_online(u32 cpu_id)
{
    cpu_core_data_t *core;
    int ret;

    ret = cpu_validate_id(cpu_id);
    if (ret < 0) {
        return ret;
    }

    core = &cpu_manager.cpus[cpu_id];

    spin_lock(&cpu_manager.lock);
    spin_lock(&core->lock);

    if (!core->info.present) {
        spin_unlock(&core->lock);
        spin_unlock(&cpu_manager.lock);
        return -ENODEV;
    }

    if (core->info.online) {
        spin_unlock(&core->lock);
        spin_unlock(&cpu_manager.lock);
        return 0; /* Already online */
    }

    /* Enable the CPU */
    ret = cpu_enable(cpu_id);
    if (ret < 0) {
        spin_unlock(&core->lock);
        spin_unlock(&cpu_manager.lock);
        return ret;
    }

    /* Mark as online */
    core->info.online = true;
    cpu_manager.num_online++;

    spin_unlock(&core->lock);
    spin_unlock(&cpu_manager.lock);

    pr_info("CPU: CPU %u is now online\n", cpu_id);

    return 0;
}

/*===========================================================================*/
/*                         CPU INFORMATION ACCESS                            */
/*===========================================================================*/

/**
 * cpu_get_info - Get CPU information structure
 * @cpu_id: CPU ID
 *
 * Returns: Pointer to cpu_info_t, or NULL on failure
 */
cpu_info_t *cpu_get_info(u32 cpu_id)
{
    if (cpu_validate_id(cpu_id) < 0) {
        return NULL;
    }

    return &cpu_manager.cpus[cpu_id].info;
}

/**
 * cpu_get_features - Get CPU features
 * @cpu_id: CPU ID
 *
 * Returns: Pointer to cpu_features_t, or NULL on failure
 */
cpu_features_t *cpu_get_features(u32 cpu_id)
{
    cpu_info_t *info;

    info = cpu_get_info(cpu_id);
    if (!info) {
        return NULL;
    }

    return &info->features;
}

/**
 * cpu_get_id - Get current CPU ID
 *
 * Returns: Current CPU ID
 */
u32 cpu_get_id(void)
{
    /* Architecture-specific implementation */
#if defined(ARCH_X86_64)
    return x86_get_cpu_id();
#elif defined(ARCH_ARM64)
    return arm_get_cpu_id();
#else
    return 0;
#endif
}

/**
 * cpu_get_apic_id - Get current APIC ID
 *
 * Returns: Current APIC ID
 */
u32 cpu_get_apic_id(void)
{
    /* Architecture-specific implementation */
#if defined(ARCH_X86_64)
    return x86_get_apic_id();
#elif defined(ARCH_ARM64)
    return arm_get_mpidr();
#else
    return 0;
#endif
}

/**
 * cpu_get_num_cpus - Get number of online CPUs
 *
 * Returns: Number of online CPUs
 */
u32 cpu_get_num_cpus(void)
{
    return cpu_manager.num_online;
}

/*===========================================================================*/
/*                         CPU POWER MANAGEMENT                              */
/*===========================================================================*/

/**
 * cpu_halt - Halt the current CPU
 *
 * Puts the CPU into a halted state until an interrupt occurs.
 */
void cpu_halt(void)
{
#if defined(ARCH_X86_64)
    __asm__ __volatile__("hlt" ::: "memory");
#elif defined(ARCH_ARM64)
    __asm__ __volatile__("wfi" ::: "memory");
#endif
}

/**
 * cpu_shutdown - Shutdown the CPU
 *
 * Performs a graceful CPU shutdown.
 */
void cpu_shutdown(void)
{
    pr_info("CPU: Shutting down...\n");

    /* Disable interrupts */
    local_irq_disable();

    /* Halt the CPU */
    cpu_halt();

    /* Should not reach here */
    while (1) {
        cpu_halt();
    }
}

/**
 * cpu_reset - Reset the CPU
 *
 * Performs a CPU reset.
 */
void cpu_reset(void)
{
    pr_info("CPU: Resetting...\n");

#if defined(ARCH_X86_64)
    /* Triple fault to reset */
    __asm__ __volatile__(
        "cli\n\t"
        "lidt (%0)\n\t"
        "int3\n\t"
        :
        : "r"(0)
        : "memory"
    );
#elif defined(ARCH_ARM64)
    /* Reset via PSCI */
    arm_psci_system_reset();
#endif

    /* Should not reach here */
    while (1) {
        cpu_halt();
    }
}

/**
 * cpu_idle - Enter idle state
 *
 * Called by the scheduler when there is no work to do.
 */
void cpu_idle(void)
{
    cpu_core_data_t *core;
    u32 cpu_id;

    cpu_id = cpu_get_id();
    core = &cpu_manager.cpus[cpu_id];

    /* Update idle time */
    core->idle_time += get_ticks() - core->last_schedule;

    /* Enable interrupts and halt */
    local_irq_enable();
    cpu_halt();
    local_irq_disable();

    /* Update last schedule time */
    core->last_schedule = get_ticks();
}

/**
 * cpu_sleep - Sleep for specified duration
 * @ns: Duration in nanoseconds
 */
void cpu_sleep(u64 ns)
{
    u64 start, now, elapsed;

    start = get_time_ns();

    while (1) {
        now = get_time_ns();
        elapsed = now - start;

        if (elapsed >= ns) {
            break;
        }

        /* Halt until next interrupt */
        cpu_halt();
    }
}

/*===========================================================================*/
/*                         CPU FREQUENCY MANAGEMENT                          */
/*===========================================================================*/

/**
 * cpu_set_frequency - Set CPU frequency
 * @cpu_id: CPU ID
 * @freq: Frequency in MHz
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_set_frequency(u32 cpu_id, u32 freq)
{
    cpu_info_t *info;

    info = cpu_get_info(cpu_id);
    if (!info) {
        return -EINVAL;
    }

    /* Architecture-specific implementation */
#if defined(ARCH_X86_64)
    return x86_cpu_set_frequency(cpu_id, freq);
#elif defined(ARCH_ARM64)
    return arm_cpu_set_frequency(cpu_id, freq);
#else
    return -ENOSYS;
#endif
}

/**
 * cpu_get_frequency - Get CPU frequency
 * @cpu_id: CPU ID
 *
 * Returns: Current frequency in MHz, or 0 on failure
 */
u32 cpu_get_frequency(u32 cpu_id)
{
    cpu_info_t *info;

    info = cpu_get_info(cpu_id);
    if (!info) {
        return 0;
    }

    /* Architecture-specific implementation */
#if defined(ARCH_X86_64)
    return x86_cpu_get_frequency(cpu_id);
#elif defined(ARCH_ARM64)
    return arm_cpu_get_frequency(cpu_id);
#else
    return info->features.base_frequency_mhz;
#endif
}

/**
 * cpu_set_pstate - Set CPU P-state
 * @cpu_id: CPU ID
 * @pstate: P-state index
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_set_pstate(u32 cpu_id, u32 pstate)
{
    cpu_info_t *info;

    info = cpu_get_info(cpu_id);
    if (!info) {
        return -EINVAL;
    }

    if (pstate >= info->pstate_count) {
        return -EINVAL;
    }

    /* Architecture-specific implementation */
#if defined(ARCH_X86_64)
    return x86_cpu_set_pstate(cpu_id, pstate);
#elif defined(ARCH_ARM64)
    return arm_cpu_set_pstate(cpu_id, pstate);
#else
    return -ENOSYS;
#endif
}

/*===========================================================================*/
/*                         CPU TEMPERATURE MONITORING                        */
/*===========================================================================*/

/**
 * cpu_get_temperature - Get CPU temperature
 * @cpu_id: CPU ID
 *
 * Returns: Temperature in Celsius, or negative error code on failure
 */
s32 cpu_get_temperature(u32 cpu_id)
{
    cpu_info_t *info;

    info = cpu_get_info(cpu_id);
    if (!info) {
        return -EINVAL;
    }

    /* Architecture-specific implementation */
#if defined(ARCH_X86_64)
    return x86_cpu_get_temperature(cpu_id);
#elif defined(ARCH_ARM64)
    return arm_cpu_get_temperature(cpu_id);
#else
    return info->temperature;
#endif
}

/**
 * cpu_get_tjmax - Get CPU TjMax (maximum junction temperature)
 * @cpu_id: CPU ID
 *
 * Returns: TjMax in Celsius, or negative error code on failure
 */
s32 cpu_get_tjmax(u32 cpu_id)
{
    cpu_info_t *info;

    info = cpu_get_info(cpu_id);
    if (!info) {
        return -EINVAL;
    }

    /* Architecture-specific implementation */
#if defined(ARCH_X86_64)
    return x86_cpu_get_tjmax(cpu_id);
#elif defined(ARCH_ARM64)
    return arm_cpu_get_tjmax(cpu_id);
#else
    return info->tj_max;
#endif
}

/*===========================================================================*/
/*                         CPU STATISTICS                                    */
/*===========================================================================*/

/**
 * cpu_get_stats - Get CPU statistics
 * @cpu_id: CPU ID
 * @stats: Output statistics structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_get_stats(u32 cpu_id, struct cpu_stats *stats)
{
    cpu_core_data_t *core;

    if (cpu_validate_id(cpu_id) < 0) {
        return -EINVAL;
    }

    if (!stats) {
        return -EINVAL;
    }

    core = &cpu_manager.cpus[cpu_id];

    spin_lock(&core->lock);

    stats->idle_time = core->idle_time;
    stats->active_time = core->active_time;
    stats->boot_time = core->boot_time;
    stats->last_schedule = core->last_schedule;

    spin_unlock(&core->lock);

    return 0;
}

/*===========================================================================*/
/*                         MODULE INITIALIZATION                             */
/*===========================================================================*/

/* Forward declarations for architecture-specific functions */
#if defined(ARCH_X86_64)
extern int x86_cpu_detect(u32 cpu_id);
extern int x86_cpu_enable(u32 cpu_id);
extern int x86_cpu_disable(u32 cpu_id);
extern u32 x86_get_cpu_id(void);
extern u32 x86_get_apic_id(void);
extern int x86_cpu_set_frequency(u32 cpu_id, u32 freq);
extern u32 x86_cpu_get_frequency(u32 cpu_id);
extern int x86_cpu_set_pstate(u32 cpu_id, u32 pstate);
extern s32 x86_cpu_get_temperature(u32 cpu_id);
extern s32 x86_cpu_get_tjmax(u32 cpu_id);
#elif defined(ARCH_ARM64)
extern int arm_cpu_detect(u32 cpu_id);
extern int arm_cpu_enable(u32 cpu_id);
extern int arm_cpu_disable(u32 cpu_id);
extern u32 arm_get_cpu_id(void);
extern u32 arm_get_mpidr(void);
extern int arm_cpu_set_frequency(u32 cpu_id, u32 freq);
extern u32 arm_cpu_get_frequency(u32 cpu_id);
extern int arm_cpu_set_pstate(u32 cpu_id, u32 pstate);
extern s32 arm_cpu_get_temperature(u32 cpu_id);
extern s32 arm_cpu_get_tjmax(u32 cpu_id);
extern void arm_psci_system_reset(void);
#endif
