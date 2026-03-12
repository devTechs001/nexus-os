/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/cpu/cpu_topology.c - CPU Topology Enumeration
 *
 * Implements CPU topology discovery including cores, threads, NUMA nodes,
 * and cache hierarchy
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         TOPOLOGY DATA STRUCTURES                          */
/*===========================================================================*/

/**
 * cpu_thread_t - CPU thread information
 */
typedef struct cpu_thread {
    u32 thread_id;              /* Thread ID within core */
    u32 apic_id;                /* APIC ID */
    u32 acpi_id;                /* ACPI processor ID */
    bool present;               /* Thread is present */
    bool enabled;               /* Thread is enabled */
} cpu_thread_t;

/**
 * cpu_core_t - CPU core information
 */
typedef struct cpu_core {
    u32 core_id;                /* Core ID within package */
    u32 package_id;             /* Package/socket ID */
    u32 numa_node;              /* NUMA node ID */
    u32 num_threads;            /* Number of threads */
    cpu_thread_t threads[8];    /* Thread information (max 8 threads/core) */
    bool present;               /* Core is present */
} cpu_core_t;

/**
 * cpu_package_t - CPU package information
 */
typedef struct cpu_package {
    u32 package_id;             /* Package/socket ID */
    u32 num_cores;              /* Number of cores */
    u32 numa_node;              /* Primary NUMA node */
    cpu_core_t *cores;          /* Core array */
    spinlock_t lock;            /* Protection lock */
    bool present;               /* Package is present */
} cpu_package_t;

/**
 * numa_node_t - NUMA node information
 */
typedef struct numa_node {
    u32 node_id;                /* Node ID */
    u32 num_packages;           /* Number of packages */
    u32 num_cores;              /* Number of cores */
    u32 num_cpus;               /* Number of CPUs */
    u64 memory_size;            /* Memory size in bytes */
    u64 memory_free;            /* Free memory in bytes */
    u32 *cpu_map;               /* CPU map */
    u32 cpu_map_size;           /* CPU map size */
    spinlock_t lock;            /* Protection lock */
    bool present;               /* Node is present */
} numa_node_t;

/**
 * cpu_topology_manager_t - Topology manager global data
 */
typedef struct cpu_topology_manager {
    u32 num_packages;           /* Number of packages */
    u32 num_cores;              /* Total cores */
    u32 num_threads;            /* Total threads */
    u32 num_numa_nodes;         /* Number of NUMA nodes */

    cpu_package_t *packages;    /* Package array */
    cpu_core_t *cores;          /* Core array (flat) */
    numa_node_t *numa_nodes;    /* NUMA node array */

    u32 threads_per_core;       /* Threads per core */
    u32 cores_per_package;      /* Cores per package */

    spinlock_t lock;            /* Global lock */
    bool initialized;           /* Initialization flag */
} cpu_topology_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static cpu_topology_manager_t topology_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         X86_64 TOPOLOGY DETECTION                         */
/*===========================================================================*/

#if defined(ARCH_X86_64)

/**
 * x86_topology_get_apic_id - Get APIC ID for current CPU
 *
 * Returns: APIC ID
 */
static u32 x86_topology_get_apic_id(void)
{
    u32 eax, ebx, ecx, edx;

    __asm__ __volatile__(
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1), "c"(0)
    );

    return (ebx >> 24) & 0xFF;
}

/**
 * x86_topology_parse_x2apic - Parse x2APIC topology
 * @apic_id: APIC ID to parse
 * @package_id: Output package ID
 * @core_id: Output core ID
 * @thread_id: Output thread ID
 *
 * Returns: 0 on success, negative error code on failure
 */
static int x86_topology_parse_x2apic(u32 apic_id, u32 *package_id,
                                      u32 *core_id, u32 *thread_id)
{
    u32 regs[4];
    u32 shift_package, shift_core;

    /* Get topology leaf */
    cpuid_count(0xB, 0, regs);

    if (regs[3] == 0) {
        /* x2APIC not supported, use legacy method */
        *thread_id = apic_id & 0x1;
        *core_id = (apic_id >> 1) & 0x3F;
        *package_id = apic_id >> 7;
        return 0;
    }

    /* Level 0: SMT */
    shift_core = regs[0] & 0x1F;

    /* Level 1: Core */
    cpuid_count(0xB, 1, regs);
    shift_package = regs[0] & 0x1F;

    *thread_id = apic_id & ((1 << shift_core) - 1);
    *core_id = (apic_id >> shift_core) & ((1 << (shift_package - shift_core)) - 1);
    *package_id = apic_id >> shift_package;

    return 0;
}

/**
 * x86_topology_detect_cache - Detect cache topology
 * @cpu_id: CPU ID
 * @cache_info: Output cache information
 *
 * Returns: 0 on success, negative error code on failure
 */
static int x86_topology_detect_cache(u32 cpu_id, struct cache_info *cache_info)
{
    u32 regs[4];
    u32 i, type, level;

    memset(cache_info, 0, sizeof(struct cache_info));

    for (i = 0; i < 10; i++) {
        cpuid_count(4, i, regs);

        type = regs[0] & 0x1F;
        if (type == 0) {
            break;
        }

        level = (regs[0] >> 5) & 0x7;

        if (level > 3) {
            continue;
        }

        cache_info->caches[level].present = true;
        cache_info->caches[level].type = type;
        cache_info->caches[level].level = level;
        cache_info->caches[level].size = ((regs[1] >> 22) + 1) *
                                          ((regs[1] & 0x3FF) + 1) *
                                          ((regs[0] >> 14) & 0xFFF + 1) * 64;
        cache_info->caches[level].line_size = ((regs[1] >> 12) & 0x3FF) + 1;
        cache_info->caches[level].associativity = ((regs[1] >> 22) & 0x3FF) + 1;
        cache_info->caches[level].sets = ((regs[1] >> 22) + 1);
    }

    return 0;
}

#endif /* ARCH_X86_64 */

/*===========================================================================*/
/*                         ARM64 TOPOLOGY DETECTION                          */
/*===========================================================================*/

#if defined(ARCH_ARM64)

/**
 * arm_topology_get_mpidr - Get MPIDR for current CPU
 *
 * Returns: MPIDR value
 */
static u64 arm_topology_get_mpidr(void)
{
    u64 mpidr;
    __asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(mpidr));
    return mpidr;
}

/**
 * arm_topology_parse_mpidr - Parse MPIDR register
 * @mpidr: MPIDR value
 * @aff0: Output affinity level 0 (thread)
 * @aff1: Output affinity level 1 (core)
 * @aff2: Output affinity level 2 (cluster)
 * @aff3: Output affinity level 3 (package)
 */
static void arm_topology_parse_mpidr(u64 mpidr, u32 *aff0, u32 *aff1,
                                      u32 *aff2, u32 *aff3)
{
    *aff0 = (mpidr >> 0) & 0xFF;
    *aff1 = (mpidr >> 8) & 0xFF;
    *aff2 = (mpidr >> 16) & 0xFF;
    *aff3 = (mpidr >> 32) & 0xFF;
}

#endif /* ARCH_ARM64 */

/*===========================================================================*/
/*                         TOPOLOGY ENUMERATION                              */
/*===========================================================================*/

/**
 * cpu_topology_init - Initialize topology manager
 */
static void cpu_topology_init(void)
{
    pr_info("CPU: Initializing topology manager...\n");

    spin_lock_init_named(&topology_manager.lock, "cpu_topology");

    topology_manager.num_packages = 0;
    topology_manager.num_cores = 0;
    topology_manager.num_threads = 0;
    topology_manager.num_numa_nodes = 1; /* Default to single node */

    topology_manager.packages = NULL;
    topology_manager.cores = NULL;
    topology_manager.numa_nodes = NULL;

    topology_manager.threads_per_core = 1;
    topology_manager.cores_per_package = 1;

    topology_manager.initialized = false;
}

/**
 * cpu_topology_enumerate - Enumerate CPU topology
 *
 * Returns: 0 on success, negative error code on failure
 */
static int cpu_topology_enumerate(void)
{
    u32 i, cpu_id;
    int ret;

    pr_info("CPU: Enumerating CPU topology...\n");

    /* Detect threads per core and cores per package */
#if defined(ARCH_X86_64)
    cpu_features_t *features;

    features = cpu_get_features(0);
    if (features) {
        topology_manager.threads_per_core =
            features->logical_cores / features->physical_cores;
        topology_manager.cores_per_package = features->physical_cores;
        topology_manager.num_packages = 1; /* Assume single socket */
        topology_manager.num_cores = features->physical_cores;
        topology_manager.num_threads = features->logical_cores;
    }
#elif defined(ARCH_ARM64)
    /* ARM64: Read from device tree or ACPI */
    topology_manager.threads_per_core = 1;
    topology_manager.cores_per_package = cpu_get_num_cpus();
    topology_manager.num_packages = 1;
    topology_manager.num_cores = cpu_get_num_cpus();
    topology_manager.num_threads = cpu_get_num_cpus();
#endif

    /* Allocate topology structures */
    topology_manager.packages = kzalloc(
        topology_manager.num_packages * sizeof(cpu_package_t));
    if (!topology_manager.packages) {
        pr_err("CPU: Failed to allocate package array\n");
        return -ENOMEM;
    }

    topology_manager.cores = kzalloc(
        topology_manager.num_cores * sizeof(cpu_core_t));
    if (!topology_manager.cores) {
        pr_err("CPU: Failed to allocate core array\n");
        kfree(topology_manager.packages);
        return -ENOMEM;
    }

    /* Initialize packages */
    for (i = 0; i < topology_manager.num_packages; i++) {
        cpu_package_t *pkg = &topology_manager.packages[i];
        spin_lock_init_named(&pkg->lock, "cpu_package");
        pkg->package_id = i;
        pkg->num_cores = topology_manager.cores_per_package;
        pkg->numa_node = 0;
        pkg->present = true;
    }

    /* Initialize cores */
    for (i = 0; i < topology_manager.num_cores; i++) {
        cpu_core_t *core = &topology_manager.cores[i];
        u32 pkg_id = i / topology_manager.cores_per_package;

        core->core_id = i % topology_manager.cores_per_package;
        core->package_id = pkg_id;
        core->numa_node = 0;
        core->num_threads = topology_manager.threads_per_core;
        core->present = true;

        /* Initialize threads */
        for (cpu_id = 0; cpu_id < core->num_threads; cpu_id++) {
            cpu_thread_t *thread = &core->threads[cpu_id];
            thread->thread_id = cpu_id;
            thread->present = true;
            thread->enabled = true;
        }
    }

    pr_info("CPU: Found %u package(s), %u core(s), %u thread(s)\n",
            topology_manager.num_packages,
            topology_manager.num_cores,
            topology_manager.num_threads);

    return 0;
}

/**
 * cpu_topology_map_cpu - Map CPU ID to topology
 * @cpu_id: CPU ID
 * @package_id: Output package ID
 * @core_id: Output core ID
 * @thread_id: Output thread ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_topology_map_cpu(u32 cpu_id, u32 *package_id, u32 *core_id, u32 *thread_id)
{
    cpu_core_t *core;

    if (!topology_manager.initialized) {
        return -ENODEV;
    }

    if (cpu_id >= topology_manager.num_threads) {
        return -EINVAL;
    }

    /* Calculate core and thread IDs */
    *core_id = cpu_id / topology_manager.threads_per_core;
    *thread_id = cpu_id % topology_manager.threads_per_core;

    if (*core_id >= topology_manager.num_cores) {
        return -EINVAL;
    }

    core = &topology_manager.cores[*core_id];
    *package_id = core->package_id;

    return 0;
}

/**
 * cpu_topology_get_sibling - Get sibling CPU IDs
 * @cpu_id: CPU ID
 * @sibling_mask: Output sibling mask
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_topology_get_sibling(u32 cpu_id, u64 *sibling_mask)
{
    u32 pkg_id, core_id, thread_id;
    u32 i;
    int ret;

    if (!sibling_mask) {
        return -EINVAL;
    }

    ret = cpu_topology_map_cpu(cpu_id, &pkg_id, &core_id, &thread_id);
    if (ret < 0) {
        return ret;
    }

    *sibling_mask = 0;

    /* Find all threads in the same core */
    for (i = 0; i < topology_manager.num_threads; i++) {
        u32 p, c, t;

        ret = cpu_topology_map_cpu(i, &p, &c, &t);
        if (ret < 0) {
            continue;
        }

        if (p == pkg_id && c == core_id) {
            *sibling_mask |= (1ULL << i);
        }
    }

    return 0;
}

/**
 * cpu_topology_get_core_siblings - Get core sibling CPU IDs
 * @cpu_id: CPU ID
 * @sibling_mask: Output core sibling mask
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_topology_get_core_siblings(u32 cpu_id, u64 *sibling_mask)
{
    u32 pkg_id, core_id, thread_id;
    u32 i;
    int ret;

    if (!sibling_mask) {
        return -EINVAL;
    }

    ret = cpu_topology_map_cpu(cpu_id, &pkg_id, &core_id, &thread_id);
    if (ret < 0) {
        return ret;
    }

    *sibling_mask = 0;

    /* Find all threads in the same package */
    for (i = 0; i < topology_manager.num_threads; i++) {
        u32 p, c, t;

        ret = cpu_topology_map_cpu(i, &p, &c, &t);
        if (ret < 0) {
            continue;
        }

        if (p == pkg_id) {
            *sibling_mask |= (1ULL << i);
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         NUMA TOPOLOGY                                     */
/*===========================================================================*/

/**
 * cpu_topology_init_numa - Initialize NUMA topology
 *
 * Returns: 0 on success, negative error code on failure
 */
static int cpu_topology_init_numa(void)
{
    u32 i;

    pr_info("CPU: Initializing NUMA topology...\n");

    /* Default: single NUMA node */
    topology_manager.num_numa_nodes = 1;

    /* Allocate NUMA node array */
    topology_manager.numa_nodes = kzalloc(
        topology_manager.num_numa_nodes * sizeof(numa_node_t));
    if (!topology_manager.numa_nodes) {
        pr_err("CPU: Failed to allocate NUMA node array\n");
        return -ENOMEM;
    }

    /* Initialize NUMA node 0 */
    for (i = 0; i < topology_manager.num_numa_nodes; i++) {
        numa_node_t *node = &topology_manager.numa_nodes[i];

        spin_lock_init_named(&node->lock, "numa_node");
        node->node_id = i;
        node->num_packages = topology_manager.num_packages;
        node->num_cores = topology_manager.num_cores;
        node->num_cpus = topology_manager.num_threads;
        node->present = true;

        /* Create CPU map */
        node->cpu_map_size = topology_manager.num_threads;
        node->cpu_map = kzalloc(node->cpu_map_size * sizeof(u32));
        if (!node->cpu_map) {
            pr_err("CPU: Failed to allocate CPU map for node %u\n", i);
            return -ENOMEM;
        }

        /* Fill CPU map */
        for (u32 j = 0; j < node->cpu_map_size; j++) {
            node->cpu_map[j] = j;
        }
    }

    pr_info("CPU: Found %u NUMA node(s)\n", topology_manager.num_numa_nodes);

    return 0;
}

/**
 * cpu_topology_get_numa_node - Get NUMA node for CPU
 * @cpu_id: CPU ID
 *
 * Returns: NUMA node ID, or negative error code on failure
 */
int cpu_topology_get_numa_node(u32 cpu_id)
{
    u32 pkg_id, core_id, thread_id;
    int ret;

    if (!topology_manager.initialized) {
        return -ENODEV;
    }

    ret = cpu_topology_map_cpu(cpu_id, &pkg_id, &core_id, &thread_id);
    if (ret < 0) {
        return ret;
    }

    /* For now, all CPUs are on node 0 */
    return 0;
}

/**
 * cpu_topology_get_numa_cpus - Get CPUs in NUMA node
 * @node_id: NUMA node ID
 * @cpu_mask: Output CPU mask
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_topology_get_numa_cpus(u32 node_id, u64 *cpu_mask)
{
    numa_node_t *node;

    if (!cpu_mask) {
        return -EINVAL;
    }

    if (node_id >= topology_manager.num_numa_nodes) {
        return -EINVAL;
    }

    node = &topology_manager.numa_nodes[node_id];

    spin_lock(&node->lock);

    *cpu_mask = 0;
    for (u32 i = 0; i < node->cpu_map_size; i++) {
        *cpu_mask |= (1ULL << node->cpu_map[i]);
    }

    spin_unlock(&node->lock);

    return 0;
}

/*===========================================================================*/
/*                         CACHE TOPOLOGY                                    */
/*===========================================================================*/

/**
 * cpu_topology_get_cache_info - Get cache topology info
 * @cpu_id: CPU ID
 * @cache_info: Output cache information
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_topology_get_cache_info(u32 cpu_id, struct cache_info *cache_info)
{
    if (!cache_info) {
        return -EINVAL;
    }

#if defined(ARCH_X86_64)
    return x86_topology_detect_cache(cpu_id, cache_info);
#else
    /* Default cache info */
    memset(cache_info, 0, sizeof(struct cache_info));

    cpu_features_t *features = cpu_get_features(cpu_id);
    if (features) {
        cache_info->caches[0].present = true;
        cache_info->caches[0].level = 1;
        cache_info->caches[0].size = features->l1_dcache_size * 1024;

        cache_info->caches[1].present = true;
        cache_info->caches[1].level = 2;
        cache_info->caches[1].size = features->l2_cache_size * 1024;

        if (features->l3_cache_size > 0) {
            cache_info->caches[2].present = true;
            cache_info->caches[2].level = 3;
            cache_info->caches[2].size = features->l3_cache_size * 1024;
        }
    }

    return 0;
#endif
}

/**
 * cpu_topology_shares_cache - Check if CPUs share cache
 * @cpu1: First CPU ID
 * @cpu2: Second CPU ID
 * @level: Cache level (1=L1, 2=L2, 3=L3)
 *
 * Returns: true if CPUs share the specified cache level
 */
bool cpu_topology_shares_cache(u32 cpu1, u32 cpu2, u32 level)
{
    u32 pkg1, core1, thread1;
    u32 pkg2, core2, thread2;

    if (cpu_topology_map_cpu(cpu1, &pkg1, &core1, &thread1) < 0) {
        return false;
    }

    if (cpu_topology_map_cpu(cpu2, &pkg2, &core2, &thread2) < 0) {
        return false;
    }

    switch (level) {
        case 1: /* L1: Same thread */
            return (cpu1 == cpu2);

        case 2: /* L2: Same core */
            return (core1 == core2);

        case 3: /* L3: Same package */
            return (pkg1 == pkg2);

        default:
            return false;
    }
}

/*===========================================================================*/
/*                         TOPOLOGY INFORMATION ACCESS                       */
/*===========================================================================*/

/**
 * cpu_topology_get_package_count - Get number of packages
 *
 * Returns: Number of CPU packages
 */
u32 cpu_topology_get_package_count(void)
{
    return topology_manager.num_packages;
}

/**
 * cpu_topology_get_core_count - Get number of cores
 *
 * Returns: Total number of cores
 */
u32 cpu_topology_get_core_count(void)
{
    return topology_manager.num_cores;
}

/**
 * cpu_topology_get_thread_count - Get number of threads
 *
 * Returns: Total number of threads (logical CPUs)
 */
u32 cpu_topology_get_thread_count(void)
{
    return topology_manager.num_threads;
}

/**
 * cpu_topology_get_numa_node_count - Get number of NUMA nodes
 *
 * Returns: Number of NUMA nodes
 */
u32 cpu_topology_get_numa_node_count(void)
{
    return topology_manager.num_numa_nodes;
}

/**
 * cpu_topology_get_threads_per_core - Get threads per core
 *
 * Returns: Number of threads per core
 */
u32 cpu_topology_get_threads_per_core(void)
{
    return topology_manager.threads_per_core;
}

/**
 * cpu_topology_get_cores_per_package - Get cores per package
 *
 * Returns: Number of cores per package
 */
u32 cpu_topology_get_cores_per_package(void)
{
    return topology_manager.cores_per_package;
}

/**
 * cpu_topology_is_hyperthreading - Check if hyperthreading is enabled
 *
 * Returns: true if hyperthreading is enabled
 */
bool cpu_topology_is_hyperthreading(void)
{
    return (topology_manager.threads_per_core > 1);
}

/*===========================================================================*/
/*                         TOPOLOGY INITIALIZATION                           */
/*===========================================================================*/

/**
 * cpu_topology_early_init - Early topology initialization
 */
void cpu_topology_early_init(void)
{
    cpu_topology_init();
}

/**
 * cpu_topology_init - Main topology initialization
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_topology_initialize(void)
{
    int ret;

    pr_info("CPU: Initializing CPU topology...\n");

    /* Enumerate topology */
    ret = cpu_topology_enumerate();
    if (ret < 0) {
        pr_err("CPU: Failed to enumerate topology (error %d)\n", ret);
        return ret;
    }

    /* Initialize NUMA */
    ret = cpu_topology_init_numa();
    if (ret < 0) {
        pr_err("CPU: Failed to initialize NUMA (error %d)\n", ret);
        return ret;
    }

    topology_manager.initialized = true;

    pr_info("CPU: Topology initialization complete\n");

    return 0;
}

/**
 * cpu_topology_dump - Dump topology information for debugging
 */
void cpu_topology_dump(void)
{
    u32 i;

    pr_info("CPU Topology:\n");
    pr_info("  Packages: %u\n", topology_manager.num_packages);
    pr_info("  Cores: %u\n", topology_manager.num_cores);
    pr_info("  Threads: %u\n", topology_manager.num_threads);
    pr_info("  NUMA Nodes: %u\n", topology_manager.num_numa_nodes);
    pr_info("  Threads/Core: %u\n", topology_manager.threads_per_core);
    pr_info("  Cores/Package: %u\n", topology_manager.cores_per_package);
    pr_info("  Hyperthreading: %s\n",
            cpu_topology_is_hyperthreading() ? "Yes" : "No");

    for (i = 0; i < topology_manager.num_threads; i++) {
        u32 pkg, core, thread;

        if (cpu_topology_map_cpu(i, &pkg, &core, &thread) == 0) {
            pr_info("  CPU %u: Package %u, Core %u, Thread %u, NUMA %u\n",
                    i, pkg, core, thread,
                    cpu_topology_get_numa_node(i));
        }
    }
}
