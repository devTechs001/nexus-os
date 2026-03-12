/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/hal.h
 */

#ifndef _NEXUS_HAL_H
#define _NEXUS_HAL_H

#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         HAL CONFIGURATION                                 */
/*===========================================================================*/

#define HAL_VERSION_MAJOR       1
#define HAL_VERSION_MINOR       0
#define HAL_VERSION_PATCH       0

/*===========================================================================*/
/*                         CPU FEATURES                                      */
/*===========================================================================*/

/* CPU Feature Flags */
#define CPU_FEATURE_FPU         (1ULL << 0)
#define CPU_FEATURE_VME         (1ULL << 1)
#define CPU_FEATURE_DE          (1ULL << 2)
#define CPU_FEATURE_PSE         (1ULL << 3)
#define CPU_FEATURE_TSC         (1ULL << 4)
#define CPU_FEATURE_MSR         (1ULL << 5)
#define CPU_FEATURE_PAE         (1ULL << 6)
#define CPU_FEATURE_MCE         (1ULL << 7)
#define CPU_FEATURE_CX8         (1ULL << 8)
#define CPU_FEATURE_APIC        (1ULL << 9)
#define CPU_FEATURE_SEP         (1ULL << 10)
#define CPU_FEATURE_MTRR        (1ULL << 11)
#define CPU_FEATURE_PGE         (1ULL << 12)
#define CPU_FEATURE_MCA         (1ULL << 13)
#define CPU_FEATURE_CMOV        (1ULL << 14)
#define CPU_FEATURE_PAT         (1ULL << 15)
#define CPU_FEATURE_PSE36       (1ULL << 16)
#define CPU_FEATURE_PN          (1ULL << 17)
#define CPU_FEATURE_CLFLUSH     (1ULL << 18)
#define CPU_FEATURE_DS          (1ULL << 19)
#define CPU_FEATURE_ACPI        (1ULL << 20)
#define CPU_FEATURE_MMX         (1ULL << 21)
#define CPU_FEATURE_FXSR        (1ULL << 22)
#define CPU_FEATURE_SSE         (1ULL << 23)
#define CPU_FEATURE_SSE2        (1ULL << 24)
#define CPU_FEATURE_SS          (1ULL << 25)
#define CPU_FEATURE_HTT         (1ULL << 26)
#define CPU_FEATURE_TM          (1ULL << 27)
#define CPU_FEATURE_IA64        (1ULL << 28)
#define CPU_FEATURE_PBE         (1ULL << 29)

/* Extended Features */
#define CPU_FEATURE_SSE3        (1ULL << 32)
#define CPU_FEATURE_PCLMULQDQ   (1ULL << 33)
#define CPU_FEATURE_DTES64      (1ULL << 34)
#define CPU_FEATURE_MONITOR     (1ULL << 35)
#define CPU_FEATURE_DS_CPL      (1ULL << 36)
#define CPU_FEATURE_VMX         (1ULL << 37)
#define CPU_FEATURE_SMX         (1ULL << 38)
#define CPU_FEATURE_EST         (1ULL << 39)
#define CPU_FEATURE_TM2         (1ULL << 40)
#define CPU_FEATURE_SSSE3       (1ULL << 41)
#define CPU_FEATURE_CNXT_ID     (1ULL << 42)
#define CPU_FEATURE_SDBG        (1ULL << 43)
#define CPU_FEATURE_FMA         (1ULL << 44)
#define CPU_FEATURE_CX16        (1ULL << 45)
#define CPU_FEATURE_XTPR        (1ULL << 46)
#define CPU_FEATURE_PDCM        (1ULL << 47)
#define CPU_FEATURE_PCID        (1ULL << 48)
#define CPU_FEATURE_DCA         (1ULL << 49)
#define CPU_FEATURE_SSE4_1      (1ULL << 50)
#define CPU_FEATURE_SSE4_2      (1ULL << 51)
#define CPU_FEATURE_X2APIC      (1ULL << 52)
#define CPU_FEATURE_MOVBE       (1ULL << 53)
#define CPU_FEATURE_POPCNT      (1ULL << 54)
#define CPU_FEATURE_TSC_DEADLINE (1ULL << 55)
#define CPU_FEATURE_AES         (1ULL << 56)
#define CPU_FEATURE_XSAVE       (1ULL << 57)
#define CPU_FEATURE_OSXSAVE     (1ULL << 58)
#define CPU_FEATURE_AVX         (1ULL << 59)
#define CPU_FEATURE_F16C        (1ULL << 60)
#define CPU_FEATURE_RDRAND      (1ULL << 61)

/* Virtualization Features */
#define CPU_FEATURE_VMX_VMXON   (1ULL << 62)
#define CPU_FEATURE_VMX_EPT     (1ULL << 63)
#define CPU_FEATURE_SVM         (1ULL << 64)
#define CPU_FEATURE_SVM_NPT     (1ULL << 65)
#define CPU_FEATURE_SVM_LBRV    (1ULL << 66)
#define CPU_FEATURE_SVM_SVML    (1ULL << 67)
#define CPU_FEATURE_SVM_NRIPS   (1ULL << 68)
#define CPU_FEATURE_SVM_TSC_RATE (1ULL << 69)

/* Security Features */
#define CPU_FEATURE_SMEP        (1ULL << 70)
#define CPU_FEATURE_SMAP        (1ULL << 71)
#define CPU_FEATURE_PKU         (1ULL << 72)
#define CPU_FEATURE_OSPKE       (1ULL << 73)
#define CPU_FEATURE_SGX         (1ULL << 74)
#define CPU_FEATURE_MPX         (1ULL << 75)
#define CPU_FEATURE_SHA         (1ULL << 76)

/**
 * cpu_features_t - CPU feature information
 */
typedef struct cpu_features {
    /* Vendor Info */
    char vendor_string[16];
    char brand_string[64];
    
    /* Basic Info */
    u32 family;
    u32 model;
    u32 stepping;
    
    /* Feature Flags */
    u64 features;
    u64 features_ext;
    u64 features_vmx;
    u64 features_svm;
    
    /* Cache Info */
    u32 l1_dcache_size;
    u32 l1_icache_size;
    u32 l2_cache_size;
    u32 l3_cache_size;
    u32 cache_line_size;
    
    /* Topology */
    u32 physical_cores;
    u32 logical_cores;
    u32 numa_nodes;
    
    /* Virtualization */
    bool has_vmx;           /* Intel VT-x */
    bool has_svm;           /* AMD-V */
    bool has_ept;           /* Extended Page Tables */
    bool has_npt;           /* Nested Page Tables */
    
    /* Security */
    bool has_smep;
    bool has_smap;
    bool has_pku;
    bool has_sgx;
    
    /* Performance */
    u32 max_frequency_mhz;
    u32 base_frequency_mhz;
} cpu_features_t;

/*===========================================================================*/
/*                         CPU ABSTRACTION                                   */
/*===========================================================================*/

/**
 * cpu_info_t - CPU information structure
 */
typedef struct cpu_info {
    u32 cpu_id;
    u32 apic_id;
    u32 acpi_id;
    
    cpu_features_t features;
    
    /* State */
    volatile bool online;
    volatile bool present;
    volatile bool enabled;
    
    /* Frequency */
    u64 tsc_frequency;
    u64 apic_frequency;
    
    /* Power */
    u32 pstate_count;
    u32 current_pstate;
    
    /* Temperature */
    s32 temperature;
    s32 tj_max;
} cpu_info_t;

/* CPU Functions */
void cpu_init(void);
void cpu_early_init(void);
void cpu_late_init(void);

int cpu_detect(u32 cpu_id);
int cpu_enable(u32 cpu_id);
int cpu_disable(u32 cpu_id);
int cpu_online(u32 cpu_id);

cpu_info_t *cpu_get_info(u32 cpu_id);
cpu_features_t *cpu_get_features(u32 cpu_id);

u32 cpu_get_id(void);
u32 cpu_get_apic_id(void);
u32 cpu_get_num_cpus(void);

void cpu_halt(void);
void cpu_shutdown(void);
void cpu_reset(void);

void cpu_idle(void);
void cpu_sleep(u64 ns);

/* CPU Frequency */
int cpu_set_frequency(u32 cpu_id, u32 freq);
u32 cpu_get_frequency(u32 cpu_id);
int cpu_set_pstate(u32 cpu_id, u32 pstate);

/* CPU Temperature */
s32 cpu_get_temperature(u32 cpu_id);
s32 cpu_get_tjmax(u32 cpu_id);

/*===========================================================================*/
/*                         MEMORY ABSTRACTION                                */
/*===========================================================================*/

/**
 * memory_region_t - Memory region information
 */
typedef struct memory_region {
    phys_addr_t base;
    phys_addr_t size;
    u32 type;
    u32 attributes;
    char name[32];
} memory_region_t;

/* Memory Types */
#define MEM_TYPE_AVAILABLE      0
#define MEM_TYPE_RESERVED       1
#define MEM_TYPE_ACPI           2
#define MEM_TYPE_NVS            3
#define MEM_TYPE_UNUSABLE       4
#define MEM_TYPE_PERSISTENT     5

/* Memory Attributes */
#define MEM_ATTR_WRITEBACK      0x00000001
#define MEM_ATTR_WRITECOMBINE   0x00000002
#define MEM_ATTR_UNCACHEABLE    0x00000004
#define MEM_ATTR_WRITEPROTECT   0x00000008
#define MEM_ATTR_DMA            0x00000010
#define MEM_ATTR_DMA32          0x00000020

/* Memory Functions */
void memory_init(void);
void memory_early_init(void);

int memory_add_region(phys_addr_t base, phys_addr_t size, u32 type);
int memory_remove_region(phys_addr_t base);

memory_region_t *memory_get_region(u32 index);
u32 memory_get_region_count(void);

phys_addr_t memory_get_total(void);
phys_addr_t memory_get_available(void);
phys_addr_t memory_get_reserved(void);

int memory_reserve(phys_addr_t base, phys_addr_t size);
int memory_free(phys_addr_t base, phys_addr_t size);

/* NUMA Support */
int numa_get_node(void *addr);
int numa_set_node(void *addr, int node);
int numa_get_num_nodes(void);
u64 numa_get_node_memory(int node);

/*===========================================================================*/
/*                         INTERRUPT ABSTRACTION                             */
/*===========================================================================*/

/* Interrupt Types */
#define IRQ_TYPE_EDGE           0x00000001
#define IRQ_TYPE_LEVEL          0x00000002
#define IRQ_TYPE_HIGH           0x00000004
#define IRQ_TYPE_LOW            0x00000008

/* Interrupt Flags */
#define IRQF_SHARED             0x00000010
#define IRQF_DISABLED           0x00000020
#define IRQF_ONESHOT            0x00000040
#define IRQF_NO_THREAD          0x00000080
#define IRQF_PER_CPU            0x00000100
#define IRQF_NO_SUSPEND         0x00000200

/* Interrupt Handler */
typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

/**
 * irq_desc_t - Interrupt descriptor
 */
typedef struct irq_desc {
    u32 irq;
    u32 flags;
    u32 type;
    
    irq_handler_t handler;
    void *dev_id;
    
    u32 cpu;
    u64 cpumask;
    
    atomic_t count;
    atomic_t unhandled;
    
    spinlock_t lock;
    
    char name[32];
} irq_desc_t;

/* Interrupt Functions */
void interrupt_init(void);
void interrupt_early_init(void);
void interrupt_late_init(void);

int request_irq(u32 irq, irq_handler_t handler, u32 flags, const char *name, void *dev_id);
void free_irq(u32 irq, void *dev_id);

void enable_irq(u32 irq);
void disable_irq(u32 irq);
void disable_irq_nosync(u32 irq);

irq_desc_t *irq_to_desc(u32 irq);

void local_irq_enable(void);
void local_irq_disable(void);
bool local_irq_save(void);
void local_irq_restore(bool flags);

/* IPI Support */
void arch_send_ipi(u32 cpu, u32 vector);
void arch_send_ipi_all(u32 vector);
void arch_send_ipi_mask(u64 mask, u32 vector);

/* Interrupt Controllers */
void apic_init(void);
void gic_init(void);
void pic_init(void);

/*===========================================================================*/
/*                         TIMER ABSTRACTION                                 */
/*===========================================================================*/

/* Timer Types */
#define TIMER_TYPE_ONESHOT      0
#define TIMER_TYPE_PERIODIC     1
#define TIMER_TYPE_RELATIVE     2
#define TIMER_TYPE_ABSOLUTE     3

/**
 * timer_handler_t - Timer callback
 */
typedef void (*timer_handler_t)(void *data);

/**
 * hrtimer_t - High-resolution timer
 */
typedef struct hrtimer {
    struct rb_node node;
    
    u64 expires;
    u64 period;
    u32 type;
    
    timer_handler_t handler;
    void *data;
    
    bool active;
    bool pending;
    
    u64 total_time;
    u32 fire_count;
} hrtimer_t;

/* Timer Functions */
void timer_init(void);
void timer_early_init(void);

u64 get_ticks(void);
u64 get_time_ns(void);
u64 get_time_us(void);
u64 get_time_ms(void);

void delay_ns(u64 ns);
void delay_us(u64 us);
void delay_ms(u64 ms);

/* High-Resolution Timer */
void hrtimer_init(void);

void hrtimer_init_timer(hrtimer_t *timer, timer_handler_t handler, void *data);
int hrtimer_start(hrtimer_t *timer, u64 ns, u32 type);
int hrtimer_cancel(hrtimer_t *timer);
bool hrtimer_active(hrtimer_t *timer);

void hrtimer_set_periodic(hrtimer_t *timer, u64 period_ns);

/* Clocksource */
u64 clocksource_read(void);
u64 clocksource_get_freq(void);
const char *clocksource_get_name(void);

/* Clock Event Device */
int clockevent_set_next_event(u64 delta_ns);
void clockevent_set_mode(u32 mode);

/*===========================================================================*/
/*                         POWER MANAGEMENT                                  */
/*===========================================================================*/

/* Power States */
#define POWER_STATE_WORKING     0
#define POWER_STATE_SLEEPING    1
#define POWER_STATE_STANDBY     2
#define POWER_STATE_SUSPEND     3
#define POWER_STATE_HIBERNATE   4
#define POWER_STATE_SHUTDOWN    5

/* Sleep States */
#define SLEEP_STATE_S0          0   /* Working */
#define SLEEP_STATE_S1          1   /* CPU stopped, cache maintained */
#define SLEEP_STATE_S2          2   /* CPU powered off, cache maintained */
#define SLEEP_STATE_S3          3   /* Suspend to RAM */
#define SLEEP_STATE_S4          4   /* Hibernation */
#define SLEEP_STATE_S5          5   /* Soft off */

/* Device Power States */
#define D_STATE_D0              0   /* Fully on */
#define D_STATE_D1              1   /* Intermediate */
#define D_STATE_D2              2   /* Intermediate */
#define D_STATE_D3              3   /* Off */

/**
 * power_state_t - Power state information
 */
typedef struct power_state {
    u32 state;
    u32 sub_state;
    u64 residency_ns;
} power_state_t;

/* Power Management Functions */
void power_init(void);
void power_early_init(void);
void power_late_init(void);

int power_set_state(u32 state);
int power_get_state(power_state_t *state);

int power_suspend(u32 state);
int power_resume(void);
int power_hibernate(void);

/* CPU Power Management */
int cpu_idle_set_governor(const char *governor);
const char *cpu_idle_get_governor(void);

int cpu_freq_set_governor(const char *governor);
const char *cpu_freq_get_governor(void);

/* Device Power Management */
int device_set_power_state(const char *device, u32 state);
int device_get_power_state(const char *device, u32 *state);

/* ACPI Support */
void acpi_init(void);
int acpi_get_sleep_state(const char *name);
const char *acpi_get_sleep_state_name(u32 state);

/* Device Tree Support */
void device_tree_init(void);
void *device_tree_get_property(const char *node, const char *property);

/*===========================================================================*/
/*                         HAL INITIALIZATION                                */
/*===========================================================================*/

void hal_init(void);
void hal_early_init(void);
void hal_late_init(void);
void hal_shutdown(void);

#endif /* _NEXUS_HAL_H */
