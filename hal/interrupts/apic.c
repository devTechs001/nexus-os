/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/interrupts/apic.c - APIC Controller for x86_64
 *
 * Implements Advanced Programmable Interrupt Controller (APIC) support
 * for x86_64 architecture including local APIC and I/O APIC
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         APIC CONSTANTS                                    */
/*===========================================================================*/

/* APIC Base Address */
#define APIC_DEFAULT_BASE       0xFEE00000ULL
#define APIC_BASE_MSR           0x1B

/* APIC Register Offsets */
#define APIC_ID                 0x0020
#define APIC_VERSION            0x0030
#define APIC_TPR                0x0080
#define APIC_APR                0x0090
#define APIC_PPR                0x00A0
#define APIC_EOI                0x00B0
#define APIC_RRD                0x00C0
#define APIC_LDR                0x00D0
#define APIC_DFR                0x00E0
#define APIC_SVR                0x00F0
#define APIC_ISR                0x0100
#define APIC_TMR                0x0180
#define APIC_IRR                0x0200
#define APIC_ESR                0x0280
#define APIC_ICR_LOW            0x0300
#define APIC_ICR_HIGH           0x0310
#define APIC_LVT_TIMER          0x0320
#define APIC_LVT_THERMAL        0x0330
#define APIC_LVT_PERFMON        0x0340
#define APIC_LVT_LINT0          0x0350
#define APIC_LVT_LINT1          0x0360
#define APIC_LVT_ERROR          0x0370
#define APIC_INIT_COUNT         0x0380
#define APIC_CUR_COUNT          0x0390
#define APIC_DIV_CONF           0x03E0

/* APIC SVR (Spurious Vector Register) */
#define APIC_SVR_ENABLE         (1 << 8)

/* APIC Timer Modes */
#define APIC_TIMER_MODE_ONESHOT     0x00000
#define APIC_TIMER_MODE_PERIODIC    0x02000
#define APIC_TIMER_MODE_TSC_DEADLINE 0x04000

/* APIC Delivery Modes */
#define APIC_DELIVERY_FIXED     0x000
#define APIC_DELIVERY_LOWEST    0x100
#define APIC_DELIVERY_SMI       0x200
#define APIC_DELIVERY_NMI       0x400
#define APIC_DELIVERY_INIT      0x500
#define APIC_DELIVERY_STARTUP   0x600
#define APIC_DELIVERY_EXTINT    0x700

/* APIC Destination Modes */
#define APIC_DEST_PHYSICAL      0x000
#define APIC_DEST_LOGICAL       0x800

/* APIC Trigger Modes */
#define APIC_TRIGGER_EDGE       0x0000
#define APIC_TRIGGER_LEVEL      0x8000

/* APIC Level */
#define APIC_LEVEL_DEASSERT     0x00000
#define APIC_LEVEL_ASSERT       0x04000

/* IPI Vectors */
#define IPI_RESCHEDULE          0xF0
#define IPI_CALL_FUNCTION       0xF1
#define IPI_WAKEUP              0xF2

/*===========================================================================*/
/*                         APIC DATA STRUCTURES                              */
/*===========================================================================*/

/**
 * apic_reg_t - APIC register access
 */
typedef volatile u32 apic_reg_t;

/**
 * apic_data_t - APIC controller data
 */
typedef struct apic_data {
    volatile apic_reg_t *base;      /* APIC base address */
    u32 apic_id;                    /* Local APIC ID */
    u32 apic_version;               /* APIC version */
    u32 max_lvt;                    /* Maximum LVT entries */
    bool x2apic_enabled;            /* x2APIC mode enabled */
    bool initialized;               /* APIC initialized */
} apic_data_t;

/**
 * io_apic_entry_t - I/O APIC redirection entry
 */
typedef struct io_apic_entry {
    u8 vector;                      /* Interrupt vector */
    u8 delivery_mode;               /* Delivery mode */
    u8 destination;                 /* Destination APIC ID */
    u8 trigger_mode;                /* Edge/Level trigger */
    u8 polarity;                    /* Active high/low */
    u8 mask;                        /* Masked */
} io_apic_entry_t;

/**
 * io_apic_data_t - I/O APIC controller data
 */
typedef struct io_apic_data {
    u32 apic_id;                    /* I/O APIC ID */
    volatile u32 *base;             /* I/O APIC base address */
    u32 version;                    /* I/O APIC version */
    u32 num_entries;                /* Number of redirection entries */
    io_apic_entry_t entries[24];    /* Redirection entries */
    bool present;                   /* I/O APIC present */
} io_apic_data_t;

/**
 * apic_manager_t - APIC manager global data
 */
typedef struct apic_manager {
    apic_data_t local_apic;         /* Local APIC */
    io_apic_data_t io_apics[8];     /* I/O APICs (max 8) */
    u32 num_io_apics;               /* Number of I/O APICs */

    u32 timer_vector;               /* Timer interrupt vector */
    u32 error_vector;               /* Error interrupt vector */
    u32 spurious_vector;            /* Spurious interrupt vector */

    u64 timer_frequency;            /* APIC timer frequency */
    u64 ticks_per_ms;               /* Ticks per millisecond */

    spinlock_t lock;                /* Global lock */
    bool initialized;               /* Manager initialized */
} apic_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static apic_manager_t apic_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         APIC REGISTER ACCESS                              */
/*===========================================================================*/

/**
 * apic_read - Read APIC register
 * @reg: Register offset
 *
 * Returns: Register value
 */
static inline u32 apic_read(u32 reg)
{
    if (apic_manager.local_apic.x2apic_enabled) {
        /* x2APIC mode - use MSR */
        u64 value;
        /* MSR access would go here */
        return (u32)value;
    } else {
        /* xAPIC mode - memory mapped */
        return apic_manager.local_apic.base[reg >> 4];
    }
}

/**
 * apic_write - Write APIC register
 * @reg: Register offset
 * @value: Value to write
 */
static inline void apic_write(u32 reg, u32 value)
{
    if (apic_manager.local_apic.x2apic_enabled) {
        /* x2APIC mode - use MSR */
        /* MSR access would go here */
    } else {
        /* xAPIC mode - memory mapped */
        apic_manager.local_apic.base[reg >> 4] = value;

        /* Wait for write to complete */
        while (apic_read(APIC_ICR_LOW) & (1 << 12))
            ;
    }
}

/**
 * apic_wait_idle - Wait for APIC to become idle
 */
static inline void apic_wait_idle(void)
{
    int i;

    for (i = 0; i < 1000; i++) {
        if (!(apic_read(APIC_ICR_LOW) & (1 << 12))) {
            return;
        }
        __asm__ __volatile__("pause" ::: "memory");
    }

    pr_err("APIC: Timeout waiting for APIC idle\n");
}

/*===========================================================================*/
/*                         I/O APIC REGISTER ACCESS                          */
/*===========================================================================*/

/**
 * io_apic_read - Read I/O APIC register
 * @apic: I/O APIC index
 * @reg: Register index
 *
 * Returns: Register value
 */
static inline u32 io_apic_read(u32 apic, u32 reg)
{
    io_apic_data_t *io_apic = &apic_manager.io_apics[apic];

    if (!io_apic->present) {
        return 0;
    }

    io_apic->base[0] = reg;
    return io_apic->base[4];
}

/**
 * io_apic_write - Write I/O APIC register
 * @apic: I/O APIC index
 * @reg: Register index
 * @value: Value to write
 */
static inline void io_apic_write(u32 apic, u32 reg, u32 value)
{
    io_apic_data_t *io_apic = &apic_manager.io_apics[apic];

    if (!io_apic->present) {
        return;
    }

    io_apic->base[0] = reg;
    io_apic->base[4] = value;
}

/*===========================================================================*/
/*                         APIC INITIALIZATION                               */
/*===========================================================================*/

/**
 * apic_detect - Detect and initialize local APIC
 *
 * Returns: 0 on success, negative error code on failure
 */
static int apic_detect(void)
{
    u64 apic_base;
    u32 eax, ebx, ecx, edx;

    pr_info("APIC: Detecting local APIC...\n");

    /* Check if APIC is supported */
    __asm__ __volatile__(
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1), "c"(0)
    );

    if (!(edx & (1 << 9))) {
        pr_err("APIC: APIC not supported by CPU\n");
        return -ENODEV;
    }

    /* Read APIC base MSR */
    /* apic_base = rdmsr(APIC_BASE_MSR); */
    apic_base = APIC_DEFAULT_BASE;

    /* Check if APIC is enabled */
    if (!(apic_base & (1 << 11))) {
        pr_info("APIC: APIC disabled in MSR, enabling...\n");
        /* wrmsr(APIC_BASE_MSR, apic_base | (1 << 11)); */
    }

    /* Map APIC base address */
    apic_manager.local_apic.base = (volatile apic_reg_t *)apic_base;

    /* Read APIC ID */
    apic_manager.local_apic.apic_id = apic_read(APIC_ID) >> 24;

    /* Read APIC version */
    apic_manager.local_apic.apic_version = apic_read(APIC_VERSION);
    apic_manager.local_apic.max_lvt = (apic_manager.local_apic.apic_version >> 16) & 0xFF;

    pr_info("APIC: Local APIC ID: %u, Version: 0x%08X, Max LVT: %u\n",
            apic_manager.local_apic.apic_id,
            apic_manager.local_apic.apic_version,
            apic_manager.local_apic.max_lvt);

    return 0;
}

/**
 * apic_enable - Enable local APIC
 */
static void apic_enable(void)
{
    u32 svr;

    pr_info("APIC: Enabling local APIC...\n");

    /* Set spurious interrupt vector and enable APIC */
    svr = apic_read(APIC_SVR);
    svr &= ~0xFF;
    svr |= apic_manager.spurious_vector;
    svr |= APIC_SVR_ENABLE;
    apic_write(APIC_SVR, svr);

    /* Clear error status */
    apic_write(APIC_ESR, 0);
    apic_read(APIC_ESR);

    /* Set LVT error handler */
    apic_write(APIC_LVT_ERROR, apic_manager.error_vector);

    apic_manager.local_apic.initialized = true;

    pr_info("APIC: Local APIC enabled\n");
}

/**
 * io_apic_detect - Detect I/O APICs
 *
 * Returns: Number of I/O APICs detected
 */
static int io_apic_detect(void)
{
    u32 i;

    pr_info("APIC: Detecting I/O APICs...\n");

    /* Default: one I/O APIC at standard address */
    /* In real implementation, would parse ACPI MADT table */

    i = 0;
    apic_manager.io_apics[i].apic_id = i;
    apic_manager.io_apics[i].base = (volatile u32 *)0xFEC00000;
    apic_manager.io_apics[i].present = true;

    /* Read I/O APIC version */
    apic_manager.io_apics[i].version = io_apic_read(i, 0x01);
    apic_manager.io_apics[i].num_entries = ((apic_manager.io_apics[i].version >> 16) & 0xFF) + 1;

    pr_info("APIC: I/O APIC %u: Version 0x%08X, %u entries\n",
            i,
            apic_manager.io_apics[i].version,
            apic_manager.io_apics[i].num_entries);

    apic_manager.num_io_apics = 1;

    return apic_manager.num_io_apics;
}

/**
 * apic_init - Initialize APIC subsystem
 */
void apic_init(void)
{
    int ret;

    pr_info("APIC: Initializing APIC subsystem...\n");

    spin_lock_init_named(&apic_manager.lock, "apic_manager");

    /* Set default vectors */
    apic_manager.timer_vector = 0xF8;
    apic_manager.error_vector = 0xFE;
    apic_manager.spurious_vector = 0xFF;

    apic_manager.timer_frequency = 0;
    apic_manager.ticks_per_ms = 0;

    apic_manager.initialized = false;

    /* Detect and enable local APIC */
    ret = apic_detect();
    if (ret < 0) {
        pr_err("APIC: Failed to detect local APIC\n");
        return;
    }

    /* Detect I/O APICs */
    io_apic_detect();

    /* Enable local APIC */
    apic_enable();

    /* Calibrate APIC timer */
    apic_calibrate_timer();

    apic_manager.initialized = true;

    pr_info("APIC: Initialization complete\n");
}

/*===========================================================================*/
/*                         APIC TIMER                                        */
/*===========================================================================*/

/**
 * apic_calibrate_timer - Calibrate APIC timer frequency
 */
void apic_calibrate_timer(void)
{
    u32 start, end, count;
    u64 ms_start, ms_end;

    pr_info("APIC: Calibrating timer...\n");

    /* Set one-shot mode */
    apic_write(APIC_LVT_TIMER, apic_manager.timer_vector);
    apic_write(APIC_DIV_CONF, 0xB); /* Divide by 1 */

    /* Set initial count */
    apic_write(APIC_INIT_COUNT, 0xFFFFFFFF);

    /* Wait for 10ms */
    ms_start = get_time_ms();
    while ((get_time_ms() - ms_start) < 10) {
        __asm__ __volatile__("pause" ::: "memory");
    }

    /* Read remaining count */
    end = apic_read(APIC_CUR_COUNT);
    count = 0xFFFFFFFF - end;

    /* Calculate frequency */
    apic_manager.timer_frequency = (u64)count * 100; /* ticks per second */
    apic_manager.ticks_per_ms = apic_manager.timer_frequency / 1000;

    pr_info("APIC: Timer frequency: %llu Hz (%llu ticks/ms)\n",
            apic_manager.timer_frequency,
            apic_manager.ticks_per_ms);
}

/**
 * apic_timer_setup - Setup APIC timer
 * @mode: Timer mode (oneshot/periodic)
 * @ticks: Number of ticks
 */
void apic_timer_setup(u32 mode, u32 ticks)
{
    u32 lvt;

    lvt = apic_manager.timer_vector;

    if (mode == APIC_TIMER_MODE_PERIODIC) {
        lvt |= APIC_TIMER_MODE_PERIODIC;
    }

    apic_write(APIC_LVT_TIMER, lvt);
    apic_write(APIC_INIT_COUNT, ticks);
}

/**
 * apic_timer_set_periodic - Set APIC timer to periodic mode
 * @ms: Period in milliseconds
 */
void apic_timer_set_periodic(u32 ms)
{
    u32 ticks = ms * apic_manager.ticks_per_ms;
    apic_timer_setup(APIC_TIMER_MODE_PERIODIC, ticks);
}

/**
 * apic_timer_set_oneshot - Set APIC timer to one-shot mode
 * @ms: Delay in milliseconds
 */
void apic_timer_set_oneshot(u32 ms)
{
    u32 ticks = ms * apic_manager.ticks_per_ms;
    apic_timer_setup(APIC_TIMER_MODE_ONESHOT, ticks);
}

/**
 * apic_timer_cancel - Cancel APIC timer
 */
void apic_timer_cancel(void)
{
    apic_write(APIC_LVT_TIMER, apic_manager.timer_vector | (1 << 16)); /* Masked */
    apic_write(APIC_INIT_COUNT, 0);
}

/*===========================================================================*/
/*                         I/O APIC IRQ ROUTING                              */
/*===========================================================================*/

/**
 * io_apic_setup_irq - Setup IRQ routing in I/O APIC
 * @irq: IRQ number
 * @vector: Interrupt vector
 * @dest_apic_id: Destination APIC ID
 * @trigger: Trigger mode (edge/level)
 * @polarity: Polarity (high/low)
 *
 * Returns: 0 on success, negative error code on failure
 */
int io_apic_setup_irq(u32 irq, u32 vector, u32 dest_apic_id,
                      u32 trigger, u32 polarity)
{
    io_apic_data_t *io_apic;
    u32 entry_low, entry_high;
    u32 reg;

    if (apic_manager.num_io_apics == 0) {
        return -ENODEV;
    }

    io_apic = &apic_manager.io_apics[0];

    if (irq >= io_apic->num_entries) {
        return -EINVAL;
    }

    spin_lock(&apic_manager.lock);

    /* Calculate register offsets */
    reg = 0x10 + irq * 2;

    /* Build entry */
    entry_low = vector;
    entry_low |= (APIC_DELIVERY_FIXED << 8);
    entry_low |= (trigger << 15);
    entry_low |= (polarity << 13);
    entry_low |= (1 << 16); /* Masked initially */

    entry_high = dest_apic_id << 24;

    /* Write entry */
    io_apic_write(0, reg, entry_low);
    io_apic_write(0, reg + 1, entry_high);

    /* Store in cache */
    io_apic->entries[irq].vector = vector;
    io_apic->entries[irq].delivery_mode = APIC_DELIVERY_FIXED;
    io_apic->entries[irq].destination = dest_apic_id;
    io_apic->entries[irq].trigger_mode = trigger;
    io_apic->entries[irq].polarity = polarity;
    io_apic->entries[irq].mask = 1;

    spin_unlock(&apic_manager.lock);

    pr_debug("APIC: I/O APIC IRQ %u -> vector %u\n", irq, vector);

    return 0;
}

/**
 * io_apic_enable_irq - Enable IRQ in I/O APIC
 * @irq: IRQ number
 */
void io_apic_enable_irq(u32 irq)
{
    io_apic_data_t *io_apic;
    u32 entry_low, reg;

    if (apic_manager.num_io_apics == 0) {
        return;
    }

    io_apic = &apic_manager.io_apics[0];

    if (irq >= io_apic->num_entries) {
        return;
    }

    spin_lock(&apic_manager.lock);

    reg = 0x10 + irq * 2;
    entry_low = io_apic_read(0, reg);
    entry_low &= ~(1 << 16); /* Unmask */
    io_apic_write(0, reg, entry_low);

    io_apic->entries[irq].mask = 0;

    spin_unlock(&apic_manager.lock);
}

/**
 * io_apic_disable_irq - Disable IRQ in I/O APIC
 * @irq: IRQ number
 */
void io_apic_disable_irq(u32 irq)
{
    io_apic_data_t *io_apic;
    u32 entry_low, reg;

    if (apic_manager.num_io_apics == 0) {
        return;
    }

    io_apic = &apic_manager.io_apics[0];

    if (irq >= io_apic->num_entries) {
        return;
    }

    spin_lock(&apic_manager.lock);

    reg = 0x10 + irq * 2;
    entry_low = io_apic_read(0, reg);
    entry_low |= (1 << 16); /* Mask */
    io_apic_write(0, reg, entry_low);

    io_apic->entries[irq].mask = 1;

    spin_unlock(&apic_manager.lock);
}

/*===========================================================================*/
/*                         IPI (INTER-PROCESSOR INTERRUPT)                   */
/*===========================================================================*/

/**
 * apic_send_ipi - Send IPI to a CPU
 * @apic_id: Target APIC ID
 * @vector: IPI vector
 */
void apic_send_ipi(u32 apic_id, u32 vector)
{
    u32 icr_low, icr_high;

    spin_lock(&apic_manager.lock);

    apic_wait_idle();

    /* Set destination */
    icr_high = (apic_id & 0xFF) << 24;
    apic_write(APIC_ICR_HIGH, icr_high);

    /* Build and send IPI */
    icr_low = vector;
    icr_low |= APIC_DEST_PHYSICAL;
    icr_low |= APIC_DELIVERY_FIXED;
    icr_low |= APIC_LEVEL_ASSERT;
    icr_low |= APIC_TRIGGER_EDGE;

    apic_write(APIC_ICR_LOW, icr_low);

    apic_wait_idle();

    spin_unlock(&apic_manager.lock);
}

/**
 * apic_send_ipi_self - Send IPI to self
 * @vector: IPI vector
 */
void apic_send_ipi_self(u32 vector)
{
    u32 icr_low;

    icr_low = vector;
    icr_low |= APIC_DELIVERY_FIXED;
    icr_low |= APIC_LEVEL_ASSERT;
    icr_low |= APIC_TRIGGER_EDGE;

    apic_write(APIC_ICR_LOW, icr_low);
}

/**
 * apic_send_ipi_all - Send IPI to all CPUs (including self)
 * @vector: IPI vector
 */
void apic_send_ipi_all(u32 vector)
{
    u32 icr_low;

    spin_lock(&apic_manager.lock);

    apic_wait_idle();

    icr_low = vector;
    icr_low |= APIC_DEST_LOGICAL;
    icr_low |= APIC_DELIVERY_FIXED;
    icr_low |= APIC_LEVEL_ASSERT;
    icr_low |= APIC_TRIGGER_EDGE;
    icr_low |= (0xFF << 24); /* All CPUs */

    apic_write(APIC_ICR_LOW, icr_low);

    apic_wait_idle();

    spin_unlock(&apic_manager.lock);
}

/**
 * apic_send_ipi_all_but_self - Send IPI to all CPUs except self
 * @vector: IPI vector
 */
void apic_send_ipi_all_but_self(u32 vector)
{
    u32 icr_low;

    spin_lock(&apic_manager.lock);

    apic_wait_idle();

    icr_low = vector;
    icr_low |= APIC_DEST_LOGICAL;
    icr_low |= APIC_DELIVERY_FIXED;
    icr_low |= APIC_LEVEL_ASSERT;
    icr_low |= APIC_TRIGGER_EDGE;

    apic_write(APIC_ICR_LOW, icr_low);

    apic_wait_idle();

    spin_unlock(&apic_manager.lock);
}

/*===========================================================================*/
/*                         APIC INFORMATION                                  */
/*===========================================================================*/

/**
 * apic_get_id - Get local APIC ID
 *
 * Returns: APIC ID
 */
u32 apic_get_id(void)
{
    return apic_manager.local_apic.apic_id;
}

/**
 * apic_get_version - Get local APIC version
 *
 * Returns: APIC version
 */
u32 apic_get_version(void)
{
    return apic_manager.local_apic.apic_version;
}

/**
 * apic_get_timer_frequency - Get APIC timer frequency
 *
 * Returns: Timer frequency in Hz
 */
u64 apic_get_timer_frequency(void)
{
    return apic_manager.timer_frequency;
}

/**
 * apic_dump_info - Dump APIC information for debugging
 */
void apic_dump_info(void)
{
    u32 i;

    pr_info("APIC Information:\n");
    pr_info("  Local APIC ID: %u\n", apic_manager.local_apic.apic_id);
    pr_info("  Local APIC Version: 0x%08X\n", apic_manager.local_apic.apic_version);
    pr_info("  Max LVT Entries: %u\n", apic_manager.local_apic.max_lvt);
    pr_info("  x2APIC: %s\n", apic_manager.local_apic.x2apic_enabled ? "Yes" : "No");
    pr_info("  Timer Frequency: %llu Hz\n", apic_manager.timer_frequency);
    pr_info("  Timer Vector: 0x%02X\n", apic_manager.timer_vector);
    pr_info("  Error Vector: 0x%02X\n", apic_manager.error_vector);
    pr_info("  Spurious Vector: 0x%02X\n", apic_manager.spurious_vector);
    pr_info("  I/O APICs: %u\n", apic_manager.num_io_apics);

    for (i = 0; i < apic_manager.num_io_apics; i++) {
        io_apic_data_t *io_apic = &apic_manager.io_apics[i];
        pr_info("  I/O APIC %u: ID=%u, Version=0x%08X, Entries=%u\n",
                i, io_apic->apic_id, io_apic->version, io_apic->num_entries);
    }
}

/*===========================================================================*/
/*                         APIC SHUTDOWN                                     */
/*===========================================================================*/

/**
 * apic_shutdown - Shutdown APIC subsystem
 */
void apic_shutdown(void)
{
    pr_info("APIC: Shutting down...\n");

    /* Disable local APIC */
    apic_write(APIC_SVR, apic_read(APIC_SVR) & ~APIC_SVR_ENABLE);

    /* Mask all LVTs */
    apic_write(APIC_LVT_TIMER, apic_read(APIC_LVT_TIMER) | (1 << 16));
    apic_write(APIC_LVT_ERROR, apic_read(APIC_LVT_ERROR) | (1 << 16));

    apic_manager.initialized = false;

    pr_info("APIC: Shutdown complete\n");
}
