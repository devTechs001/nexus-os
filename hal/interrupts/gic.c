/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/interrupts/gic.c - GIC Controller for ARM64
 *
 * Implements Generic Interrupt Controller (GIC) support for ARM64
 * architecture including GICv2 and GICv3
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         GIC CONSTANTS                                     */
/*===========================================================================*/

/* GIC Distributor Registers */
#define GICD_CTLR               0x0000
#define GICD_TYPER              0x0004
#define GICD_IIDR               0x0008
#define GICD_IGROUPR            0x0080
#define GICD_ISENABLER          0x0100
#define GICD_ICENABLER          0x0180
#define GICD_ISPENDR            0x0200
#define GICD_ICPENDR            0x0280
#define GICD_ISACTIVER          0x0300
#define GICD_ICACTIVER          0x0380
#define GICD_IPRIORITYR         0x0400
#define GICD_ITARGETSR          0x0800
#define GICD_ICFGR              0x0C00
#define GICD_SGIR               0x0F00
#define GICD_CPENDSGIR          0x0F10
#define GICD_SPENDSGIR          0x0F20

/* GIC Distributor Control */
#define GICD_CTLR_ENABLE        (1 << 0)
#define GICD_CTLR_ARE_NS        (1 << 5)

/* GIC Redistributor Registers (GICv3) */
#define GICR_CTLR               0x0000
#define GICR_IIDR               0x0004
#define GICR_TYPER              0x0008
#define GICR_STATUSR            0x0010
#define GICR_WAKER              0x0014
#define GICR_SETLPIR            0x0040
#define GICR_CLRLPIR            0x0048
#define GICR_PROPBASER          0x0070
#define GICR_PENDBASER          0x0078
#define GICR_INVLPIR            0x00A0
#define GICR_INVALLR            0x00B0
#define GICR_SYNCR              0x00C0
#define GICR_MOVLPIR            0x0100
#define GICR_MOVALLR            0x0110

/* GIC Redistributor Wake */
#define GICR_WAKER_ProcessorSleep   (1 << 1)
#define GICR_WAKER_ChildrenASleep   (1 << 2)

/* GIC CPU Interface Registers */
#define GICC_CTLR               0x0000
#define GICC_PMR                0x0004
#define GICC_BPR                0x0008
#define GICC_IAR                0x000C
#define GICC_EOIR               0x0010
#define GICC_RPR                0x0014
#define GICC_HPPIR              0x0018
#define GICC_ABPR               0x001C
#define GICC_AIAR               0x0020
#define GICC_AEOIR              0x0024
#define GICC_AHPPIR             0x0028
#define GICC_IIDR               0x00FC

/* GIC CPU Interface Control */
#define GICC_CTLR_ENABLE        (1 << 0)
#define GICC_CTLR_FIQ_ENABLE    (1 << 3)

/* GICv3 System Registers */
#define ICC_SRE_EL1             S3_0_C12_C12_5
#define ICC_PMR_EL1             S3_0_C4_C6_0
#define ICC_IAR1_EL1            S3_0_C12_C12_0
#define ICC_EOIR1_EL1           S3_0_C12_C12_1
#define ICC_HPPIR1_EL1          S3_0_C12_C12_8
#define ICC_BPR1_EL1            S3_0_C12_C12_3
#define ICC_CTLR_EL1            S3_0_C12_C12_4
#define ICC_SGI1R_EL1           S3_0_C12_C11_5
#define ICC_ASGI1R_EL1          S3_0_C12_C11_6
#define ICC_SGI0R_EL1           S3_0_C12_C11_7

/* GICv3 SRE Enable */
#define ICC_SRE_ENB             (1 << 0)
#define ICC_SRE_DIB             (1 << 1)
#define ICC_SRE_DFB             (1 << 2)

/* Interrupt Types */
#define GIC_INT_TYPE_PPI        1
#define GIC_INT_TYPE_SPI        0

/* Trigger Types */
#define GIC_TRIGGER_EDGE        0
#define GIC_TRIGGER_LEVEL       1

/* Priority Levels */
#define GIC_PRIORITY_LOWEST     0xFF
#define GIC_PRIORITY_LOW        0xC0
#define GIC_PRIORITY_NORMAL     0x80
#define GIC_PRIORITY_HIGH       0x40
#define GIC_PRIORITY_HIGHEST    0x00

/* IPI Vectors */
#define GIC_IPI_RESCHEDULE      0
#define GIC_IPI_CALL_FUNCTION   1
#define GIC_IPI_WAKEUP          2

/*===========================================================================*/
/*                         GIC DATA STRUCTURES                               */
/*===========================================================================*/

/**
 * gic_version_t - GIC version enumeration
 */
typedef enum {
    GIC_VERSION_UNKNOWN = 0,
    GIC_VERSION_V2,
    GIC_VERSION_V3,
    GIC_VERSION_V4,
} gic_version_t;

/**
 * gic_data_t - GIC controller data
 */
typedef struct gic_data {
    volatile u32 *dist_base;        /* Distributor base */
    volatile u32 *cpu_base;         /* CPU interface base */
    volatile u64 *redist_base;      /* Redistributor base (GICv3) */
    u32 dist_size;                  /* Distributor size */
    u32 num_irqs;                   /* Number of supported IRQs */
    u32 num_cpus;                   /* Number of CPU interfaces */
    gic_version_t version;          /* GIC version */
    u32 gic_id;                     /* GIC ID */
    bool initialized;               /* GIC initialized */
} gic_data_t;

/**
 * gic_irq_config_t - GIC IRQ configuration
 */
typedef struct gic_irq_config {
    u32 irq;                        /* IRQ number */
    u32 priority;                   /* Priority (0-255) */
    u32 target_cpu;                 /* Target CPU */
    u32 trigger;                    /* Trigger type */
    u32 group;                      /* Interrupt group */
    bool enabled;                   /* IRQ enabled */
} gic_irq_config_t;

/**
 * gic_manager_t - GIC manager global data
 */
typedef struct gic_manager {
    gic_data_t gic;                 /* GIC data */
    gic_irq_config_t configs[1024]; /* IRQ configurations */

    u32 spurious_count;             /* Spurious interrupt count */
    u64 total_interrupts;           /* Total interrupt count */

    spinlock_t lock;                /* Global lock */
    bool initialized;               /* Manager initialized */
} gic_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static gic_manager_t gic_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         GIC REGISTER ACCESS                               */
/*===========================================================================*/

/**
 * gic_dist_read - Read GIC distributor register
 * @offset: Register offset
 *
 * Returns: Register value
 */
static inline u32 gic_dist_read(u32 offset)
{
    return gic_manager.gic.dist_base[offset >> 2];
}

/**
 * gic_dist_write - Write GIC distributor register
 * @offset: Register offset
 * @value: Value to write
 */
static inline void gic_dist_write(u32 offset, u32 value)
{
    gic_manager.gic.dist_base[offset >> 2] = value;
}

/**
 * gic_cpu_read - Read GIC CPU interface register
 * @offset: Register offset
 *
 * Returns: Register value
 */
static inline u32 gic_cpu_read(u32 offset)
{
    return gic_manager.gic.cpu_base[offset >> 2];
}

/**
 * gic_cpu_write - Write GIC CPU interface register
 * @offset: Register offset
 * @value: Value to write
 */
static inline void gic_cpu_write(u32 offset, u32 value)
{
    gic_manager.gic.cpu_base[offset >> 2] = value;
}

/*===========================================================================*/
/*                         GIC DETECTION                                     */
/*===========================================================================*/

/**
 * gic_detect_version - Detect GIC version
 *
 * Returns: GIC version
 */
static gic_version_t gic_detect_version(void)
{
    u32 pidr0, pidr1;
    u32 part_number;

    /* Read peripheral ID registers */
    pidr0 = gic_dist_read(GICD_IIDR);
    pidr1 = gic_dist_read(GICD_IIDR + 4);

    part_number = (pidr0 >> 4) & 0xFFF;

    /* Determine version from part number */
    if (part_number == 0x020 || part_number == 0x040) {
        return GIC_VERSION_V2;
    } else if (part_number == 0x043 || part_number == 0x044) {
        return GIC_VERSION_V3;
    } else if (part_number == 0x045 || part_number == 0x046) {
        return GIC_VERSION_V4;
    }

    /* Default to V2 for unknown */
    pr_warn("GIC: Unknown GIC version (part=0x%03X), assuming GICv2\n", part_number);
    return GIC_VERSION_V2;
}

/**
 * gic_detect - Detect and initialize GIC
 *
 * Returns: 0 on success, negative error code on failure
 */
static int gic_detect(void)
{
    u32 typer;

    pr_info("GIC: Detecting GIC...\n");

    /* Set default base addresses */
    /* In real implementation, would get from device tree or ACPI */
    gic_manager.gic.dist_base = (volatile u32 *)0x08000000;
    gic_manager.gic.cpu_base = (volatile u32 *)0x08010000;
    gic_manager.gic.redist_base = NULL;

    /* Detect version */
    gic_manager.gic.version = gic_detect_version();

    /* Read GIC type register */
    typer = gic_dist_read(GICD_TYPER);

    /* Calculate number of IRQs */
    gic_manager.gic.num_irqs = 32 * ((typer & 0x1F) + 1);

    /* Calculate number of CPU interfaces */
    gic_manager.gic.num_cpus = ((typer >> 5) & 0x7) + 1;

    /* Read GIC ID */
    gic_manager.gic.gic_id = gic_dist_read(GICD_IIDR);

    pr_info("GIC: Version %u, IRQs: %u, CPUs: %u, ID: 0x%08X\n",
            gic_manager.gic.version,
            gic_manager.gic.num_irqs,
            gic_manager.gic.num_cpus,
            gic_manager.gic.gic_id);

    return 0;
}

/*===========================================================================*/
/*                         GIC INITIALIZATION                                */
/*===========================================================================*/

/**
 * gic_dist_init - Initialize GIC distributor
 */
static void gic_dist_init(void)
{
    u32 i;

    pr_info("GIC: Initializing distributor...\n");

    /* Disable distributor */
    gic_dist_write(GICD_CTLR, 0);

    /* Set all interrupts to Group 0 (secure) */
    for (i = 0; i < gic_manager.gic.num_irqs; i += 32) {
        gic_dist_write(GICD_IGROUPR + (i >> 3), 0xFFFFFFFF);
    }

    /* Disable all interrupts */
    for (i = 0; i < gic_manager.gic.num_irqs; i += 32) {
        gic_dist_write(GICD_ICENABLER + (i >> 3), 0xFFFFFFFF);
    }

    /* Clear all pending interrupts */
    for (i = 0; i < gic_manager.gic.num_irqs; i += 32) {
        gic_dist_write(GICD_ICPENDR + (i >> 3), 0xFFFFFFFF);
    }

    /* Set all priorities to lowest */
    for (i = 0; i < gic_manager.gic.num_irqs; i += 4) {
        gic_dist_write(GICD_IPRIORITYR + (i >> 2), 0xFFFFFFFF);
    }

    /* Set all targets to CPU 0 */
    if (gic_manager.gic.version == GIC_VERSION_V2) {
        for (i = 0; i < gic_manager.gic.num_irqs; i += 4) {
            gic_dist_write(GICD_ITARGETSR + (i >> 2), 0x01010101);
        }
    }

    /* Set all to level-triggered, active high */
    for (i = 32; i < gic_manager.gic.num_irqs; i += 16) {
        gic_dist_write(GICD_ICFGR + (i >> 3), 0x00000000);
    }

    /* Enable distributor */
    gic_dist_write(GICD_CTLR, GICD_CTLR_ENABLE);

    pr_info("GIC: Distributor initialized\n");
}

/**
 * gic_cpu_init - Initialize GIC CPU interface
 */
static void gic_cpu_init(void)
{
    pr_info("GIC: Initializing CPU interface...\n");

    if (gic_manager.gic.version >= GIC_VERSION_V3) {
        /* GICv3: Use system registers */
        u64 sre;

        /* Enable system register interface */
        __asm__ __volatile__(
            "mrs %0, s3_0_c12_c12_5\n\t"
            : "=r"(sre)
        );
        sre |= ICC_SRE_ENB;
        __asm__ __volatile__(
            "msr s3_0_c12_c12_5, %0\n\t"
            "isb\n\t"
            :
            : "r"(sre)
        );

        /* Set priority mask */
        __asm__ __volatile__("msr s3_0_c4_c6_0, %0" : : "r"(GIC_PRIORITY_LOWEST));

        /* Set binary point */
        __asm__ __volatile__("msr s3_0_c12_c12_3, %0" : : "r"(7));

        /* Enable CPU interface */
        __asm__ __volatile__("msr s3_0_c12_c12_4, %0" : : "r"(1));
    } else {
        /* GICv2: Use memory-mapped registers */

        /* Set priority mask */
        gic_cpu_write(GICC_PMR, GIC_PRIORITY_LOWEST);

        /* Set binary point */
        gic_cpu_write(GICC_BPR, 7);

        /* Enable CPU interface */
        gic_cpu_write(GICC_CTLR, GICC_CTLR_ENABLE);
    }

    pr_info("GIC: CPU interface initialized\n");
}

/**
 * gic_init - Initialize GIC subsystem
 */
void gic_init(void)
{
    int ret;

    pr_info("GIC: Initializing GIC subsystem...\n");

    spin_lock_init_named(&gic_manager.lock, "gic_manager");

    memset(gic_manager.configs, 0, sizeof(gic_manager.configs));
    gic_manager.spurious_count = 0;
    gic_manager.total_interrupts = 0;
    gic_manager.initialized = false;

    /* Detect GIC */
    ret = gic_detect();
    if (ret < 0) {
        pr_err("GIC: Failed to detect GIC\n");
        return;
    }

    /* Initialize distributor */
    gic_dist_init();

    /* Initialize CPU interface */
    gic_cpu_init();

    gic_manager.initialized = true;

    pr_info("GIC: Initialization complete\n");
}

/*===========================================================================*/
/*                         GIC IRQ MANAGEMENT                                */
/*===========================================================================*/

/**
 * gic_setup_irq - Setup GIC IRQ
 * @irq: IRQ number
 * @priority: Priority (0-255)
 * @target_cpu: Target CPU
 * @trigger: Trigger type (edge/level)
 *
 * Returns: 0 on success, negative error code on failure
 */
int gic_setup_irq(u32 irq, u32 priority, u32 target_cpu, u32 trigger)
{
    gic_irq_config_t *config;
    u32 reg, mask, shift;

    if (!gic_manager.initialized) {
        return -ENODEV;
    }

    if (irq >= gic_manager.gic.num_irqs) {
        return -EINVAL;
    }

    spin_lock(&gic_manager.lock);

    config = &gic_manager.configs[irq];
    config->irq = irq;
    config->priority = priority;
    config->target_cpu = target_cpu;
    config->trigger = trigger;
    config->enabled = false;

    /* Set priority */
    reg = GICD_IPRIORITYR + (irq & ~3);
    shift = (irq & 3) * 8;
    mask = 0xFF << shift;
    gic_dist_write(reg, (gic_dist_read(reg) & ~mask) | (priority << shift));

    /* Set target CPU (GICv2 only) */
    if (gic_manager.gic.version == GIC_VERSION_V2 && irq >= 32) {
        reg = GICD_ITARGETSR + (irq & ~3);
        shift = (irq & 3) * 8;
        mask = 0xFF << shift;
        gic_dist_write(reg, (gic_dist_read(reg) & ~mask) | ((1 << target_cpu) << shift));
    }

    /* Set trigger type */
    if (irq >= 32) {
        reg = GICD_ICFGR + (irq / 16) * 4;
        shift = (irq % 16) * 2;
        mask = 0x3 << shift;
        gic_dist_write(reg, (gic_dist_read(reg) & ~mask) | (trigger << shift));
    }

    spin_unlock(&gic_manager.lock);

    pr_debug("GIC: Setup IRQ %u: priority=%u, cpu=%u, trigger=%u\n",
             irq, priority, target_cpu, trigger);

    return 0;
}

/**
 * gic_enable_irq - Enable GIC IRQ
 * @irq: IRQ number
 */
void gic_enable_irq(u32 irq)
{
    gic_irq_config_t *config;
    u32 reg;

    if (!gic_manager.initialized || irq >= gic_manager.gic.num_irqs) {
        return;
    }

    spin_lock(&gic_manager.lock);

    config = &gic_manager.configs[irq];
    config->enabled = true;

    reg = GICD_ISENABLER + (irq / 32) * 4;
    gic_dist_write(reg, 1 << (irq % 32));

    spin_unlock(&gic_manager.lock);
}

/**
 * gic_disable_irq - Disable GIC IRQ
 * @irq: IRQ number
 */
void gic_disable_irq(u32 irq)
{
    gic_irq_config_t *config;
    u32 reg;

    if (!gic_manager.initialized || irq >= gic_manager.gic.num_irqs) {
        return;
    }

    spin_lock(&gic_manager.lock);

    config = &gic_manager.configs[irq];
    config->enabled = false;

    reg = GICD_ICENABLER + (irq / 32) * 4;
    gic_dist_write(reg, 1 << (irq % 32));

    spin_unlock(&gic_manager.lock);
}

/**
 * gic_set_priority - Set GIC IRQ priority
 * @irq: IRQ number
 * @priority: Priority (0-255)
 */
void gic_set_priority(u32 irq, u32 priority)
{
    gic_irq_config_t *config;
    u32 reg, mask, shift;

    if (!gic_manager.initialized || irq >= gic_manager.gic.num_irqs) {
        return;
    }

    spin_lock(&gic_manager.lock);

    config = &gic_manager.configs[irq];
    config->priority = priority;

    reg = GICD_IPRIORITYR + (irq & ~3);
    shift = (irq & 3) * 8;
    mask = 0xFF << shift;
    gic_dist_write(reg, (gic_dist_read(reg) & ~mask) | (priority << shift));

    spin_unlock(&gic_manager.lock);
}

/**
 * gic_set_affinity - Set GIC IRQ affinity
 * @irq: IRQ number
 * @cpu: Target CPU
 *
 * Returns: 0 on success, negative error code on failure
 */
int gic_set_affinity(u32 irq, u32 cpu)
{
    gic_irq_config_t *config;
    u32 reg, mask, shift;

    if (!gic_manager.initialized || irq >= gic_manager.gic.num_irqs) {
        return -ENODEV;
    }

    if (cpu >= gic_manager.gic.num_cpus) {
        return -EINVAL;
    }

    spin_lock(&gic_manager.lock);

    config = &gic_manager.configs[irq];
    config->target_cpu = cpu;

    /* GICv2 only */
    if (gic_manager.gic.version == GIC_VERSION_V2 && irq >= 32) {
        reg = GICD_ITARGETSR + (irq & ~3);
        shift = (irq & 3) * 8;
        mask = 0xFF << shift;
        gic_dist_write(reg, (gic_dist_read(reg) & ~mask) | ((1 << cpu) << shift));
    }

    spin_unlock(&gic_manager.lock);

    return 0;
}

/*===========================================================================*/
/*                         GIC INTERRUPT HANDLING                            */
/*===========================================================================*/

/**
 * gic_get_irq - Get current active IRQ
 *
 * Returns: IRQ number, or 0x3FF for spurious
 */
u32 gic_get_irq(void)
{
    u32 iar;

    if (gic_manager.gic.version >= GIC_VERSION_V3) {
        __asm__ __volatile__("mrs %0, s3_0_c12_c12_0" : "=r"(iar));
    } else {
        iar = gic_cpu_read(GICC_IAR);
    }

    iar &= 0x3FF;

    if (iar == 0x3FF) {
        gic_manager.spurious_count++;
    }

    return iar;
}

/**
 * gic_ack_irq - Acknowledge IRQ
 * @irq: IRQ number
 */
void gic_ack_irq(u32 irq)
{
    if (gic_manager.gic.version >= GIC_VERSION_V3) {
        __asm__ __volatile__("msr s3_0_c12_c12_1, %0" : : "r"(irq));
    } else {
        gic_cpu_write(GICC_EOIR, irq);
    }
}

/**
 * gic_handle_irq - Handle GIC interrupt
 *
 * Returns: IRQ number handled, or -1 for spurious
 */
int gic_handle_irq(void)
{
    u32 irq;

    irq = gic_get_irq();

    if (irq == 0x3FF) {
        return -1; /* Spurious */
    }

    gic_manager.total_interrupts++;

    /* Call generic handler */
    generic_irq_handler(irq);

    /* Acknowledge */
    gic_ack_irq(irq);

    return irq;
}

/*===========================================================================*/
/*                         GIC IPI (INTER-PROCESSOR INTERRUPT)               */
/*===========================================================================*/

/**
 * gic_send_sgi - Send Software Generated Interrupt
 * @target_cpu: Target CPU
 * @sgi_id: SGI ID (0-15)
 */
void gic_send_sgi(u32 target_cpu, u32 sgi_id)
{
    u32 sgir;

    if (sgi_id > 15) {
        return;
    }

    if (gic_manager.gic.version >= GIC_VERSION_V3) {
        /* GICv3: Use ICC_SGI1R_EL1 */
        u64 sgi1r;

        sgi1r = sgi_id & 0xF;
        sgi1r |= ((u64)target_cpu & 0xFF) << 16;
        sgi1r |= (1ULL << 40); /* Physical mode */

        __asm__ __volatile__("msr s3_0_c12_c11_5, %0" : : "r"(sgi1r));
        __asm__ __volatile__("isb" ::: "memory");
    } else {
        /* GICv2: Use GICD_SGIR */
        sgir = (sgi_id & 0xF);
        sgir |= ((target_cpu & 0xFF) << 16);
        sgir |= (1 << 24); /* Physical mode */

        gic_dist_write(GICD_SGIR, sgir);
    }
}

/**
 * gic_send_sgi_self - Send SGI to self
 * @sgi_id: SGI ID
 */
void gic_send_sgi_self(u32 sgi_id)
{
    u32 sgir;

    if (gic_manager.gic.version >= GIC_VERSION_V3) {
        u64 sgi1r;

        sgi1r = sgi_id & 0xF;
        sgi1r |= (1ULL << 40);
        sgi1r |= (1ULL << 31); /* Self mode */

        __asm__ __volatile__("msr s3_0_c12_c11_7, %0" : : "r"(sgi1r));
    } else {
        sgir = (sgi_id & 0xF);
        sgir |= (2 << 24); /* Self mode */

        gic_dist_write(GICD_SGIR, sgir);
    }
}

/**
 * gic_send_sgi_all - Send SGI to all CPUs
 * @sgi_id: SGI ID
 */
void gic_send_sgi_all(u32 sgi_id)
{
    u32 sgir;

    if (gic_manager.gic.version >= GIC_VERSION_V3) {
        u64 sgi1r;

        sgi1r = sgi_id & 0xF;
        sgi1r |= (1ULL << 40);
        sgi1r |= (1ULL << 31); /* All mode */

        __asm__ __volatile__("msr s3_0_c12_c11_6, %0" : : "r"(sgi1r));
    } else {
        sgir = (sgi_id & 0xF);
        sgir |= (0 << 24); /* All mode */
        sgir |= (1 << 25); /* Use target list */
        sgir |= 0xFF; /* All CPUs */

        gic_dist_write(GICD_SGIR, sgir);
    }
}

/**
 * arm_send_ipi - Send IPI on ARM64
 * @cpu: Target CPU
 * @vector: IPI vector
 */
void arm_send_ipi(u32 cpu, u32 vector)
{
    gic_send_sgi(cpu, vector & 0xF);
}

/**
 * arm_send_ipi_all - Send IPI to all CPUs on ARM64
 * @vector: IPI vector
 */
void arm_send_ipi_all(u32 vector)
{
    gic_send_sgi_all(vector & 0xF);
}

/*===========================================================================*/
/*                         GIC INFORMATION                                   */
/*===========================================================================*/

/**
 * gic_get_version - Get GIC version
 *
 * Returns: GIC version
 */
gic_version_t gic_get_version(void)
{
    return gic_manager.gic.version;
}

/**
 * gic_get_num_irqs - Get number of IRQs
 *
 * Returns: Number of IRQs
 */
u32 gic_get_num_irqs(void)
{
    return gic_manager.gic.num_irqs;
}

/**
 * gic_dump_info - Dump GIC information for debugging
 */
void gic_dump_info(void)
{
    u32 i;

    pr_info("GIC Information:\n");
    pr_info("  Version: %u\n", gic_manager.gic.version);
    pr_info("  IRQs: %u\n", gic_manager.gic.num_irqs);
    pr_info("  CPUs: %u\n", gic_manager.gic.num_cpus);
    pr_info("  GIC ID: 0x%08X\n", gic_manager.gic.gic_id);
    pr_info("  Total Interrupts: %llu\n", gic_manager.total_interrupts);
    pr_info("  Spurious Interrupts: %u\n", gic_manager.spurious_count);
    pr_info("\n");

    pr_info("IRQ Configurations:\n");
    for (i = 0; i < gic_manager.gic.num_irqs && i < 256; i++) {
        gic_irq_config_t *config = &gic_manager.configs[i];

        if (config->enabled) {
            pr_info("  IRQ %3u: priority=%u, cpu=%u, trigger=%u\n",
                    i, config->priority, config->target_cpu, config->trigger);
        }
    }
}

/*===========================================================================*/
/*                         GIC SHUTDOWN                                      */
/*===========================================================================*/

/**
 * gic_shutdown - Shutdown GIC subsystem
 */
void gic_shutdown(void)
{
    u32 i;

    pr_info("GIC: Shutting down...\n");

    /* Disable all interrupts */
    for (i = 0; i < gic_manager.gic.num_irqs; i += 32) {
        gic_dist_write(GICD_ICENABLER + (i >> 3), 0xFFFFFFFF);
    }

    /* Disable distributor */
    gic_dist_write(GICD_CTLR, 0);

    /* Disable CPU interface */
    if (gic_manager.gic.version >= GIC_VERSION_V3) {
        __asm__ __volatile__("msr s3_0_c12_c12_4, %0" : : "r"(0));
    } else {
        gic_cpu_write(GICC_CTLR, 0);
    }

    gic_manager.initialized = false;

    pr_info("GIC: Shutdown complete\n");
}
