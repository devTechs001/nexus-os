/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/cpu/cpu_features.c - CPU Feature Detection and Caching
 *
 * Implements CPU feature detection, caching, and query functionality
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         FEATURE DATA STRUCTURES                           */
/*===========================================================================*/

/**
 * cpu_feature_cache_t - CPU feature cache entry
 */
typedef struct cpu_feature_cache {
    u32 cpu_id;                       /* CPU ID */
    cpu_features_t features;          /* Cached features */
    bool valid;                       /* Cache validity flag */
    u64 timestamp;                    /* Cache timestamp */
    spinlock_t lock;                  /* Protection lock */
} cpu_feature_cache_t;

/**
 * cpu_feature_manager_t - Feature manager global data
 */
typedef struct cpu_feature_manager {
    cpu_feature_cache_t cache[MAX_CPUS];  /* Feature cache */
    spinlock_t lock;                      /* Global lock */
    bool initialized;                     /* Initialization flag */
    u32 common_features;                  /* Features common to all CPUs */
    u32 common_features_ext;              /* Extended features common to all */
} cpu_feature_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static cpu_feature_manager_t feature_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         CPUID WRAPPERS                                    */
/*===========================================================================*/

/**
 * cpuid - Execute CPUID instruction
 * @eax: Input EAX value (function number)
 * @ebx: Output EBX value (pointer)
 * @ecx: Input ECX value (sub-function number)
 * @edx: Output EDX value (pointer)
 *
 * Executes the CPUID instruction and stores results.
 */
static inline void cpuid(u32 eax, u32 *ebx, u32 ecx, u32 *edx)
{
#if defined(ARCH_X86_64)
    u32 b, d;
    __asm__ __volatile__(
        "cpuid\n\t"
        : "=b"(b), "=d"(d)
        : "a"(eax), "c"(ecx)
        : "memory"
    );
    if (ebx) *ebx = b;
    if (edx) *edx = d;
#elif defined(ARCH_ARM64)
    /* ARM64 uses different mechanism - see arm_cpu_features.c */
    if (ebx) *ebx = 0;
    if (edx) *edx = 0;
#endif
}

/**
 * cpuid_count - Execute CPUID with count
 * @eax: Input EAX value
 * @count: Count value (ECX)
 * @regs: Output registers array [EAX, EBX, ECX, EDX]
 */
static inline void cpuid_count(u32 eax, u32 count, u32 regs[4])
{
#if defined(ARCH_X86_64)
    __asm__ __volatile__(
        "cpuid\n\t"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(eax), "c"(count)
        : "memory"
    );
#endif
}

/*===========================================================================*/
/*                         X86_64 FEATURE DETECTION                          */
/*===========================================================================*/

#if defined(ARCH_X86_64)

/**
 * x86_detect_vendor - Detect CPU vendor string
 * @features: Feature structure to populate
 */
static void x86_detect_vendor(cpu_features_t *features)
{
    u32 regs[4];

    /* Get vendor string */
    cpuid_count(0, 0, regs);

    /* Store vendor string */
    *(u32 *)&features->vendor_string[0] = regs[1]; /* EBX */
    *(u32 *)&features->vendor_string[4] = regs[3]; /* EDX */
    *(u32 *)&features->vendor_string[8] = regs[2]; /* ECX */
    features->vendor_string[12] = '\0';

    pr_debug("CPU: Vendor: %s\n", features->vendor_string);
}

/**
 * x86_detect_brand - Detect CPU brand string
 * @features: Feature structure to populate
 */
static void x86_detect_brand(cpu_features_t *features)
{
    u32 regs[4];
    u32 i;

    /* Get brand string (3 CPUID calls) */
    for (i = 0; i < 3; i++) {
        cpuid_count(0x80000002 + i, 0, regs);
        *(u32 *)&features->brand_string[i * 16 + 0] = regs[0];
        *(u32 *)&features->brand_string[i * 16 + 4] = regs[1];
        *(u32 *)&features->brand_string[i * 16 + 8] = regs[2];
        *(u32 *)&features->brand_string[i * 16 + 12] = regs[3];
    }
    features->brand_string[47] = '\0';

    pr_debug("CPU: Brand: %s\n", features->brand_string);
}

/**
 * x86_detect_basic_info - Detect basic CPU information
 * @features: Feature structure to populate
 */
static void x86_detect_basic_info(cpu_features_t *features)
{
    u32 regs[4];
    u32 family, model, stepping;

    /* Get family/model/stepping */
    cpuid_count(1, 0, regs);

    stepping = (regs[0]) & 0xF;
    model = ((regs[0]) >> 4) & 0xF;
    family = ((regs[0]) >> 8) & 0xF;

    /* Extended family/model for newer CPUs */
    if (family == 0xF || family == 0x6) {
        family += ((regs[0]) >> 20) & 0xFF;
        model += (((regs[0]) >> 16) & 0xF) << 4;
    }

    features->family = family;
    features->model = model;
    features->stepping = stepping;

    pr_debug("CPU: Family=%u, Model=%u, Stepping=%u\n",
             family, model, stepping);
}

/**
 * x86_detect_features - Detect CPU feature flags
 * @features: Feature structure to populate
 */
static void x86_detect_features(cpu_features_t *features)
{
    u32 regs[4];

    /* Standard feature flags (CPUID.01H:EDX) */
    cpuid_count(1, 0, regs);
    features->features = regs[3]; /* EDX */
    features->features_ext = regs[2]; /* ECX */

    /* Extended feature flags (CPUID.07H:EBX) */
    cpuid_count(7, 0, regs);
    features->features = features->features | ((u64)regs[1] << 32);

    pr_debug("CPU: Features=0x%08X, Ext=0x%08X\n",
             (u32)features->features, (u32)features->features_ext);
}

/**
 * x86_detect_cache - Detect CPU cache information
 * @features: Feature structure to populate
 */
static void x86_detect_cache(cpu_features_t *features)
{
    u32 regs[4];
    u32 i, type, level, size;

    /* Use CPUID leaf 4 for deterministic cache parameters */
    for (i = 0; i < 10; i++) {
        cpuid_count(4, i, regs);

        type = regs[0] & 0x1F;
        if (type == 0) {
            break; /* No more caches */
        }

        level = (regs[0] >> 5) & 0x7;
        size = ((regs[1] >> 22) + 1) * ((regs[1] & 0x3FF) + 1) *
               ((regs[0] >> 14) & 0xFFF + 1) * 64;
        size /= 1024; /* Convert to KB */

        switch (level) {
            case 1:
                if (type == 1) { /* Data cache */
                    features->l1_dcache_size = size;
                } else if (type == 2) { /* Instruction cache */
                    features->l1_icache_size = size;
                }
                break;
            case 2:
                features->l2_cache_size = size;
                break;
            case 3:
                features->l3_cache_size = size;
                break;
        }

        pr_debug("CPU: L%u Cache: %u KB (type=%u)\n", level, size, type);
    }

    /* Cache line size from CPUID.01H:EBX[15:8] */
    cpuid_count(1, 0, regs);
    features->cache_line_size = ((regs[1] >> 8) & 0xFF) * 8;

    pr_debug("CPU: Cache line size: %u bytes\n", features->cache_line_size);
}

/**
 * x86_detect_topology - Detect CPU topology
 * @features: Feature structure to populate
 */
static void x86_detect_topology(cpu_features_t *features)
{
    u32 regs[4];
    u32 max_logical, apic_id_width;

    /* Get max logical processors */
    cpuid_count(1, 0, regs);
    max_logical = (regs[1] >> 16) & 0xFF;

    /* Get APIC ID width for topology */
    cpuid_count(0xB, 0, regs);
    apic_id_width = regs[0] & 0x1F;

    /* Calculate physical cores */
    if (max_logical > 0 && apic_id_width > 0) {
        features->logical_cores = max_logical;
        features->physical_cores = max_logical >> apic_id_width;
    } else {
        features->logical_cores = 1;
        features->physical_cores = 1;
    }

    pr_debug("CPU: Physical cores=%u, Logical cores=%u\n",
             features->physical_cores, features->logical_cores);
}

/**
 * x86_detect_virtualization - Detect virtualization features
 * @features: Feature structure to populate
 */
static void x86_detect_virtualization(cpu_features_t *features)
{
    u32 regs[4];
    u32 vendor_ecx;

    /* Check vendor for virtualization type */
    if (strcmp(features->vendor_string, "GenuineIntel") == 0) {
        /* Intel VT-x */
        cpuid_count(1, 0, regs);
        if (regs[2] & (1 << 5)) { /* VMX bit */
            features->has_vmx = true;

            /* Check for EPT */
            cpuid_count(7, 0, regs);
            if (regs[1] & (1 << 7)) { /* EPT bit */
                features->has_ept = true;
            }
        }
    } else if (strcmp(features->vendor_string, "AuthenticAMD") == 0) {
        /* AMD SVM */
        cpuid_count(0x80000001, 0, regs);
        if (regs[2] & (1 << 2)) { /* SVM bit */
            features->has_svm = true;

            /* Check for NPT */
            cpuid_count(0x8000000A, 0, regs);
            if (regs[3] & (1 << 0)) { /* NPT bit */
                features->has_npt = true;
            }
        }
    }

    pr_debug("CPU: VMX=%u, SVM=%u, EPT=%u, NPT=%u\n",
             features->has_vmx, features->has_svm,
             features->has_ept, features->has_npt);
}

/**
 * x86_detect_security - Detect security features
 * @features: Feature structure to populate
 */
static void x86_detect_security(cpu_features_t *features)
{
    u32 regs[4];

    /* Extended features (CPUID.07H:EBX) */
    cpuid_count(7, 0, regs);

    features->has_smep = !!(regs[1] & (1 << 7));
    features->has_smap = !!(regs[1] & (1 << 20));
    features->has_pku = !!(regs[1] & (1 << 3));
    features->has_sgx = !!(regs[1] & (1 << 2));

    pr_debug("CPU: SMEP=%u, SMAP=%u, PKU=%u, SGX=%u\n",
             features->has_smep, features->has_smap,
             features->has_pku, features->has_sgx);
}

/**
 * x86_detect_frequency - Detect CPU frequency
 * @features: Feature structure to populate
 */
static void x86_detect_frequency(cpu_features_t *features)
{
    u32 regs[4];

    /* Try to get frequency from CPUID.16H */
    cpuid_count(0x16, 0, regs);
    if (regs[0] > 0) {
        features->base_frequency_mhz = regs[0];
        features->max_frequency_mhz = regs[1];
    } else {
        /* Fallback: estimate from TSC */
        features->base_frequency_mhz = 0;
        features->max_frequency_mhz = 0;
    }

    pr_debug("CPU: Base=%u MHz, Max=%u MHz\n",
             features->base_frequency_mhz, features->max_frequency_mhz);
}

#endif /* ARCH_X86_64 */

/*===========================================================================*/
/*                         ARM64 FEATURE DETECTION                           */
/*===========================================================================*/

#if defined(ARCH_ARM64)

/**
 * arm_detect_vendor - Detect CPU vendor (ARM64)
 * @features: Feature structure to populate
 */
static void arm_detect_vendor(cpu_features_t *features)
{
    /* ARM64 uses MIDR_EL1 for identification */
    strcpy(features->vendor_string, "ARM");
    strcpy(features->brand_string, "ARM64 Processor");
}

/**
 * arm_detect_features - Detect CPU features (ARM64)
 * @features: Feature structure to populate
 */
static void arm_detect_features(cpu_features_t *features)
{
    u64 id_aa64pfr0, id_aa64isar0;

    /* Read feature registers */
    __asm__ __volatile__("mrs %0, id_aa64pfr0_el1" : "=r"(id_aa64pfr0));
    __asm__ __volatile__("mrs %0, id_aa64isar0_el1" : "=r"(id_aa64isar0));

    /* Parse feature fields */
    if ((id_aa64pfr0 & 0xF) >= 1) {
        features->features |= CPU_FEATURE_PAE;
    }

    /* Check for FP/SIMD */
    features->features |= CPU_FEATURE_FPU;
    features->features |= CPU_FEATURE_NEON;

    pr_debug("CPU: ARM64 Features detected\n");
}

#endif /* ARCH_ARM64 */

/*===========================================================================*/
/*                         FEATURE DETECTION API                             */
/*===========================================================================*/

/**
 * cpu_features_init - Initialize feature manager
 */
static void cpu_features_init(void)
{
    u32 i;

    pr_info("CPU: Initializing feature manager...\n");

    spin_lock_init_named(&feature_manager.lock, "cpu_features");

    for (i = 0; i < MAX_CPUS; i++) {
        cpu_feature_cache_t *cache = &feature_manager.cache[i];
        spin_lock_init_named(&cache->lock, "cpu_feature_cache");
        cache->valid = false;
        cache->cpu_id = i;
    }

    feature_manager.common_features = 0;
    feature_manager.common_features_ext = 0;
    feature_manager.initialized = true;
}

/**
 * cpu_features_detect - Detect features for a CPU
 * @cpu_id: CPU ID
 * @features: Feature structure to populate
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_features_detect(u32 cpu_id, cpu_features_t *features)
{
    if (!features) {
        return -EINVAL;
    }

    /* Zero the structure */
    memset(features, 0, sizeof(cpu_features_t));

#if defined(ARCH_X86_64)
    x86_detect_vendor(features);
    x86_detect_brand(features);
    x86_detect_basic_info(features);
    x86_detect_features(features);
    x86_detect_cache(features);
    x86_detect_topology(features);
    x86_detect_virtualization(features);
    x86_detect_security(features);
    x86_detect_frequency(features);
#elif defined(ARCH_ARM64)
    arm_detect_vendor(features);
    arm_detect_features(features);
#endif

    return 0;
}

/**
 * cpu_features_cache - Cache CPU features
 * @cpu_id: CPU ID
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_features_cache(u32 cpu_id)
{
    cpu_feature_cache_t *cache;
    int ret;

    if (cpu_id >= MAX_CPUS) {
        return -EINVAL;
    }

    cache = &feature_manager.cache[cpu_id];

    spin_lock(&cache->lock);

    if (cache->valid) {
        spin_unlock(&cache->lock);
        return 0; /* Already cached */
    }

    spin_unlock(&cache->lock);

    /* Detect features */
    ret = cpu_features_detect(cpu_id, &cache->features);
    if (ret < 0) {
        return ret;
    }

    spin_lock(&cache->lock);
    cache->valid = true;
    cache->timestamp = get_ticks();
    spin_unlock(&cache->lock);

    pr_debug("CPU: Features cached for CPU %u\n", cpu_id);

    return 0;
}

/**
 * cpu_features_get_cached - Get cached CPU features
 * @cpu_id: CPU ID
 *
 * Returns: Pointer to cached features, or NULL on failure
 */
cpu_features_t *cpu_features_get_cached(u32 cpu_id)
{
    cpu_feature_cache_t *cache;

    if (cpu_id >= MAX_CPUS) {
        return NULL;
    }

    cache = &feature_manager.cache[cpu_id];

    if (!cache->valid) {
        return NULL;
    }

    return &cache->features;
}

/**
 * cpu_features_has - Check if CPU has a feature
 * @cpu_id: CPU ID
 * @feature: Feature flag to check
 *
 * Returns: true if feature is present, false otherwise
 */
bool cpu_features_has(u32 cpu_id, u64 feature)
{
    cpu_features_t *features;

    features = cpu_features_get_cached(cpu_id);
    if (!features) {
        return false;
    }

    return !!(features->features & feature);
}

/**
 * cpu_features_has_all - Check if CPU has all features
 * @cpu_id: CPU ID
 * @features: Feature flags to check
 *
 * Returns: true if all features are present, false otherwise
 */
bool cpu_features_has_all(u32 cpu_id, u64 features)
{
    cpu_features_t *feat;

    feat = cpu_features_get_cached(cpu_id);
    if (!feat) {
        return false;
    }

    return (feat->features & features) == features;
}

/**
 * cpu_features_has_any - Check if CPU has any of the features
 * @cpu_id: CPU ID
 * @features: Feature flags to check
 *
 * Returns: true if any feature is present, false otherwise
 */
bool cpu_features_has_any(u32 cpu_id, u64 features)
{
    cpu_features_t *feat;

    feat = cpu_features_get_cached(cpu_id);
    if (!feat) {
        return false;
    }

    return !!(feat->features & features);
}

/**
 * cpu_features_get_common - Get features common to all CPUs
 * @features: Output structure for common features
 *
 * Returns: 0 on success, negative error code on failure
 */
int cpu_features_get_common(cpu_features_t *features)
{
    u32 i;
    u64 common, common_ext;
    cpu_features_t *feat;

    if (!features) {
        return -EINVAL;
    }

    if (!feature_manager.initialized) {
        return -ENODEV;
    }

    /* Start with all features set */
    common = ~0ULL;
    common_ext = ~0ULL;

    /* AND with each CPU's features */
    for (i = 0; i < MAX_CPUS; i++) {
        feat = cpu_features_get_cached(i);
        if (!feat) {
            continue;
        }

        common &= feat->features;
        common_ext &= feat->features_ext;
    }

    features->features = common;
    features->features_ext = common_ext;

    return 0;
}

/**
 * cpu_features_dump - Dump CPU features for debugging
 * @cpu_id: CPU ID
 */
void cpu_features_dump(u32 cpu_id)
{
    cpu_features_t *features;

    features = cpu_features_get_cached(cpu_id);
    if (!features) {
        pr_err("CPU: No features cached for CPU %u\n", cpu_id);
        return;
    }

    pr_info("CPU %u Features:\n", cpu_id);
    pr_info("  Vendor: %s\n", features->vendor_string);
    pr_info("  Brand: %s\n", features->brand_string);
    pr_info("  Family: %u, Model: %u, Stepping: %u\n",
            features->family, features->model, features->stepping);
    pr_info("  L1 D-Cache: %u KB\n", features->l1_dcache_size);
    pr_info("  L1 I-Cache: %u KB\n", features->l1_icache_size);
    pr_info("  L2 Cache: %u KB\n", features->l2_cache_size);
    pr_info("  L3 Cache: %u KB\n", features->l3_cache_size);
    pr_info("  Cache Line: %u bytes\n", features->cache_line_size);
    pr_info("  Physical Cores: %u\n", features->physical_cores);
    pr_info("  Logical Cores: %u\n", features->logical_cores);
    pr_info("  VMX: %u, SVM: %u\n",
            features->has_vmx, features->has_svm);
    pr_info("  SMEP: %u, SMAP: %u, PKU: %u, SGX: %u\n",
            features->has_smep, features->has_smap,
            features->has_pku, features->has_sgx);
}

/*===========================================================================*/
/*                         FEATURE CHECK HELPERS                             */
/*===========================================================================*/

/**
 * cpu_has_fpu - Check if CPU has FPU
 *
 * Returns: true if FPU is present
 */
bool cpu_has_fpu(void)
{
    return cpu_features_has(cpu_get_id(), CPU_FEATURE_FPU);
}

/**
 * cpu_has_mmx - Check if CPU has MMX
 *
 * Returns: true if MMX is present
 */
bool cpu_has_mmx(void)
{
    return cpu_features_has(cpu_get_id(), CPU_FEATURE_MMX);
}

/**
 * cpu_has_sse - Check if CPU has SSE
 * @version: SSE version (1-4)
 *
 * Returns: true if SSE version is present
 */
bool cpu_has_sse(u32 version)
{
    u64 feature;

    switch (version) {
        case 1:
            feature = CPU_FEATURE_SSE;
            break;
        case 2:
            feature = CPU_FEATURE_SSE2;
            break;
        case 3:
            feature = CPU_FEATURE_SSE3;
            break;
        case 4:
            feature = CPU_FEATURE_SSSE3;
            break;
        default:
            return false;
    }

    return cpu_features_has(cpu_get_id(), feature);
}

/**
 * cpu_has_avx - Check if CPU has AVX
 *
 * Returns: true if AVX is present
 */
bool cpu_has_avx(void)
{
    return cpu_features_has(cpu_get_id(), CPU_FEATURE_AVX);
}

/**
 * cpu_has_vmx - Check if CPU has Intel VT-x
 *
 * Returns: true if VMX is present
 */
bool cpu_has_vmx(void)
{
    return cpu_features_has(cpu_get_id(), CPU_FEATURE_VMX);
}

/**
 * cpu_has_svm - Check if CPU has AMD-V
 *
 * Returns: true if SVM is present
 */
bool cpu_has_svm(void)
{
    return cpu_features_has(cpu_get_id(), CPU_FEATURE_SVM);
}

/*===========================================================================*/
/*                         MODULE INITIALIZATION                             */
/*===========================================================================*/

/**
 * cpu_features_early_init - Early feature initialization
 */
void cpu_features_early_init(void)
{
    cpu_features_init();

    /* Cache boot CPU features */
    cpu_features_cache(0);
}

/**
 * cpu_features_late_init - Late feature initialization
 */
void cpu_features_late_init(void)
{
    u32 i;

    /* Cache features for all present CPUs */
    for (i = 1; i < MAX_CPUS; i++) {
        cpu_features_cache(i);
    }

    /* Calculate common features */
    cpu_features_get_common(NULL);

    pr_info("CPU: Feature initialization complete\n");
}
