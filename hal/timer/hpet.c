/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/timer/hpet.c - HPET Timer for x86_64
 *
 * Implements High Precision Event Timer (HPET) support for x86_64
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         HPET CONSTANTS                                    */
/*===========================================================================*/

/* HPET Base Address */
#define HPET_BASE_ADDRESS       0xFED00000ULL

/* HPET Register Offsets */
#define HPET_ID                 0x000
#define HPET_PERIOD             0x008
#define HPET_CONF               0x010
#define HPET_STATUS             0x020
#define HPET_COUNTER            0x0F0
#define HPET_T0_CONF            0x100
#define HPET_T0_COMP            0x108
#define HPET_T0_ROUTE           0x110
#define HPET_T1_CONF            0x120
#define HPET_T1_COMP            0x128
#define HPET_T1_ROUTE           0x130
#define HPET_T2_CONF            0x140
#define HPET_T2_COMP            0x148
#define HPET_T2_ROUTE           0x150

/* HPET ID Register Bits */
#define HPET_ID_REV             0x000000FF
#define HPET_ID_NUM_TIMERS      0x00001F00
#define HPET_ID_64BIT           0x00002000
#define HPET_ID_LEGACY          0x00008000
#define HPET_ID_VENDOR          0xFFFF0000

/* HPET Configuration Register Bits */
#define HPET_CONF_ENABLE        (1 << 0)
#define HPET_CONF_LEGACY        (1 << 1)

/* HPET Timer Configuration Bits */
#define HPET_TCONF_INT_TYPE     (1 << 1)
#define HPET_TCONF_INT_ROUTE    0x00003E00
#define HPET_TCONF_FSB          (1 << 14)
#define HPET_TCONF_PERIODIC     (1 << 15)
#define HPET_TCONF_64BIT        (1 << 16)
#define HPET_TCONF_SETVAL       (1 << 17)
#define HPET_TCONF_32BIT        (1 << 18)
#define HPET_TCONF_INT_ROUTE_CAP 0x003F0000

/* HPET Timer Status Bits */
#define HPET_TSTAT_TVAL         (1 << 0)
#define HPET_TSTAT_NS           (1 << 2)

/*===========================================================================*/
/*                         HPET DATA STRUCTURES                              */
/*===========================================================================*/

/**
 * hpet_timer_t - HPET timer structure
 */
typedef struct hpet_timer {
    u32 index;                    /* Timer index */
    u32 irq;                      /* IRQ number */
    u64 period;                   /* Period in femtoseconds */
    bool enabled;                 /* Timer enabled */
    bool periodic;                /* Periodic mode */
    bool legacy;                  /* Legacy replacement */
} hpet_timer_t;

/**
 * hpet_data_t - HPET controller data
 */
typedef struct hpet_data {
    volatile u64 *base;           /* MMIO base address */
    u32 vendor_id;                /* Vendor ID */
    u32 revision;                 /* Revision number */
    u32 num_timers;               /* Number of timers */
    u64 period;                   /* Counter period in femtoseconds */
    u64 frequency;                /* Counter frequency in Hz */
    bool enabled;                 /* HPET enabled */
    bool legacy_replacement;      /* Legacy replacement mode */
    bool initialized;             /* HPET initialized */
} hpet_data_t;

/**
 * hpet_manager_t - HPET manager global data
 */
typedef struct hpet_manager {
    hpet_data_t hpet;             /* HPET data */
    hpet_timer_t timers[8];       /* Timer structures */
    u32 num_timers;               /* Number of usable timers */

    u64 total_interrupts;         /* Total HPET interrupts */
    u64 total_comparisons;        /* Total comparator matches */

    spinlock_t lock;              /* Global lock */
    bool initialized;             /* Manager initialized */
} hpet_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static hpet_manager_t hpet_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         HPET REGISTER ACCESS                              */
/*===========================================================================*/

/**
 * hpet_read - Read HPET register
 * @offset: Register offset
 *
 * Returns: Register value
 */
static inline u64 hpet_read(u32 offset)
{
    return hpet_manager.hpet.base[offset >> 3];
}

/**
 * hpet_write - Write HPET register
 * @offset: Register offset
 * @value: Value to write
 */
static inline void hpet_write(u32 offset, u64 value)
{
    hpet_manager.hpet.base[offset >> 3] = value;
}

/**
 * hpet_read32 - Read 32-bit HPET register
 * @offset: Register offset
 *
 * Returns: Register value
 */
static inline u32 hpet_read32(u32 offset)
{
    return (u32)hpet_read(offset);
}

/**
 * hpet_write32 - Write 32-bit HPET register
 * @offset: Register offset
 * @value: Value to write
 */
static inline void hpet_write32(u32 offset, u32 value)
{
    volatile u32 *addr = (volatile u32 *)((u64)hpet_manager.hpet.base + offset);
    *addr = value;
}

/*===========================================================================*/
/*                         HPET DETECTION                                    */
/*===========================================================================*/

/**
 * hpet_detect - Detect HPET hardware
 *
 * Returns: 0 on success, negative error code on failure
 */
static int hpet_detect(void)
{
    u64 id_reg;
    u32 vendor, rev, num_timers;

    pr_info("HPET: Detecting HPET...\n");

    /* Map HPET base address */
    hpet_manager.hpet.base = (volatile u64 *)HPET_BASE_ADDRESS;

    /* Read ID register */
    id_reg = hpet_read(HPET_ID);

    /* Parse ID register */
    rev = id_reg & HPET_ID_REV;
    num_timers = (id_reg & HPET_ID_NUM_TIMERS) >> 8;
    hpet_manager.hpet.legacy_replacement = !!(id_reg & HPET_ID_LEGACY);
    vendor = (id_reg & HPET_ID_VENDOR) >> 16;

    hpet_manager.hpet.revision = rev;
    hpet_manager.hpet.num_timers = num_timers + 1;
    hpet_manager.hpet.vendor_id = vendor;

    /* Read period */
    hpet_manager.hpet.period = hpet_read(HPET_PERIOD);

    /* Calculate frequency */
    if (hpet_manager.hpet.period > 0) {
        hpet_manager.hpet.frequency = 1000000000000000ULL / hpet_manager.hpet.period;
    } else {
        hpet_manager.hpet.frequency = 0;
    }

    pr_info("HPET: Revision %u.%u, %u timers, %llu Hz, Vendor 0x%04X\n",
            (rev >> 4) & 0xF, rev & 0xF,
            hpet_manager.hpet.num_timers,
            hpet_manager.hpet.frequency,
            vendor);

    return 0;
}

/*===========================================================================*/
/*                         HPET INITIALIZATION                               */
/*===========================================================================*/

/**
 * hpet_enable - Enable HPET
 */
static void hpet_enable(void)
{
    u64 conf;

    pr_info("HPET: Enabling HPET...\n");

    /* Read current configuration */
    conf = hpet_read(HPET_CONF);

    /* Enable HPET */
    conf |= HPET_CONF_ENABLE;
    hpet_write(HPET_CONF, conf);

    hpet_manager.hpet.enabled = true;

    pr_info("HPET: HPET enabled\n");
}

/**
 * hpet_disable - Disable HPET
 */
static void hpet_disable(void)
{
    u64 conf;

    pr_info("HPET: Disabling HPET...\n");

    /* Read current configuration */
    conf = hpet_read(HPET_CONF);

    /* Disable HPET */
    conf &= ~HPET_CONF_ENABLE;
    hpet_write(HPET_CONF, conf);

    hpet_manager.hpet.enabled = false;

    pr_info("HPET: HPET disabled\n");
}

/**
 * hpet_init - Initialize HPET subsystem
 *
 * Returns: 0 on success, negative error code on failure
 */
int hpet_init(void)
{
    u32 i;
    int ret;

    pr_info("HPET: Initializing HPET subsystem...\n");

    spin_lock_init_named(&hpet_manager.lock, "hpet_manager");

    memset(hpet_manager.timers, 0, sizeof(hpet_manager.timers));
    hpet_manager.num_timers = 0;
    hpet_manager.total_interrupts = 0;
    hpet_manager.total_comparisons = 0;
    hpet_manager.initialized = false;

    /* Detect HPET */
    ret = hpet_detect();
    if (ret < 0) {
        pr_err("HPET: Failed to detect HPET\n");
        return ret;
    }

    /* Initialize timer structures */
    for (i = 0; i < hpet_manager.hpet.num_timers && i < 8; i++) {
        hpet_timer_t *timer = &hpet_manager.timers[i];

        timer->index = i;
        timer->irq = 0;
        timer->period = hpet_manager.hpet.period;
        timer->enabled = false;
        timer->periodic = false;
        timer->legacy = (i < 2); /* Timers 0 and 1 can be legacy */

        hpet_manager.num_timers++;
    }

    /* Enable HPET */
    hpet_enable();

    hpet_manager.initialized = true;

    pr_info("HPET: Initialization complete (%u timers)\n", hpet_manager.num_timers);

    return 0;
}

/*===========================================================================*/
/*                         HPET TIMER MANAGEMENT                             */
/*===========================================================================*/

/**
 * hpet_timer_setup - Setup HPET timer
 * @timer_id: Timer ID (0-7)
 * @irq: IRQ number
 * @periodic: Periodic mode flag
 *
 * Returns: 0 on success, negative error code on failure
 */
int hpet_timer_setup(u32 timer_id, u32 irq, bool periodic)
{
    hpet_timer_t *timer;
    u32 conf_offset;
    u64 tconf;

    if (!hpet_manager.initialized) {
        return -ENODEV;
    }

    if (timer_id >= hpet_manager.num_timers) {
        return -EINVAL;
    }

    timer = &hpet_manager.timers[timer_id];

    spin_lock(&hpet_manager.lock);

    /* Disable timer during setup */
    conf_offset = HPET_T0_CONF + (timer_id * 0x20);
    tconf = hpet_read(conf_offset);
    tconf &= ~HPET_TCONF_INT_TYPE; /* Edge triggered */
    tconf &= ~HPET_TCONF_PERIODIC;
    hpet_write(conf_offset, tconf);

    /* Set IRQ route */
    hpet_write(conf_offset + 0x10, (1 << irq));

    /* Configure timer */
    timer->irq = irq;
    timer->periodic = periodic;

    if (periodic) {
        tconf |= HPET_TCONF_PERIODIC;
    }

    tconf |= HPET_TCONF_INT_TYPE; /* Level triggered */
    tconf |= HPET_TCONF_64BIT;
    hpet_write(conf_offset, tconf);

    timer->enabled = true;

    spin_unlock(&hpet_manager.lock);

    pr_debug("HPET: Timer %u setup: IRQ=%u, periodic=%u\n",
             timer_id, irq, periodic);

    return 0;
}

/**
 * hpet_timer_enable - Enable HPET timer
 * @timer_id: Timer ID
 */
void hpet_timer_enable(u32 timer_id)
{
    hpet_timer_t *timer;
    u32 conf_offset;
    u64 tconf;

    if (!hpet_manager.initialized || timer_id >= hpet_manager.num_timers) {
        return;
    }

    timer = &hpet_manager.timers[timer_id];

    spin_lock(&hpet_manager.lock);

    conf_offset = HPET_T0_CONF + (timer_id * 0x20);
    tconf = hpet_read(conf_offset);
    tconf |= HPET_TCONF_INT_TYPE;
    hpet_write(conf_offset, tconf);

    timer->enabled = true;

    spin_unlock(&hpet_manager.lock);
}

/**
 * hpet_timer_disable - Disable HPET timer
 * @timer_id: Timer ID
 */
void hpet_timer_disable(u32 timer_id)
{
    hpet_timer_t *timer;
    u32 conf_offset;
    u64 tconf;

    if (!hpet_manager.initialized || timer_id >= hpet_manager.num_timers) {
        return;
    }

    timer = &hpet_manager.timers[timer_id];

    spin_lock(&hpet_manager.lock);

    conf_offset = HPET_T0_CONF + (timer_id * 0x20);
    tconf = hpet_read(conf_offset);
    tconf &= ~HPET_TCONF_INT_TYPE;
    hpet_write(conf_offset, tconf);

    timer->enabled = false;

    spin_unlock(&hpet_manager.lock);
}

/**
 * hpet_timer_set_comparator - Set timer comparator value
 * @timer_id: Timer ID
 * @value: Comparator value
 */
void hpet_timer_set_comparator(u32 timer_id, u64 value)
{
    u32 comp_offset;

    if (!hpet_manager.initialized || timer_id >= hpet_manager.num_timers) {
        return;
    }

    comp_offset = HPET_T0_COMP + (timer_id * 0x20);
    hpet_write(comp_offset, value);

    hpet_manager.total_comparisons++;
}

/**
 * hpet_timer_set_period - Set timer period (for periodic mode)
 * @timer_id: Timer ID
 * @period_ns: Period in nanoseconds
 *
 * Returns: 0 on success, negative error code on failure
 */
int hpet_timer_set_period(u32 timer_id, u64 period_ns)
{
    hpet_timer_t *timer;
    u64 femtoseconds, comparator_value;

    if (!hpet_manager.initialized || timer_id >= hpet_manager.num_timers) {
        return -ENODEV;
    }

    timer = &hpet_manager.timers[timer_id];

    if (!timer->periodic) {
        return -EINVAL;
    }

    /* Convert nanoseconds to femtoseconds */
    femtoseconds = period_ns * 1000000;

    /* Calculate comparator value */
    comparator_value = femtoseconds / timer->period;

    spin_lock(&hpet_manager.lock);

    /* Set the period via comparator */
    hpet_timer_set_comparator(timer_id, comparator_value);

    spin_unlock(&hpet_manager.lock);

    return 0;
}

/*===========================================================================*/
/*                         HPET COUNTER ACCESS                               */
/*===========================================================================*/

/**
 * hpet_read_counter - Read HPET main counter
 *
 * Returns: Current counter value
 */
u64 hpet_read_counter(void)
{
    if (!hpet_manager.hpet.enabled) {
        return 0;
    }

    return hpet_read(HPET_COUNTER);
}

/**
 * hpet_read_counter_ns - Read HPET counter as nanoseconds
 *
 * Returns: Current time in nanoseconds
 */
u64 hpet_read_counter_ns(void)
{
    u64 ticks = hpet_read_counter();

    /* Convert ticks to nanoseconds */
    return (ticks * hpet_manager.hpet.period) / 1000000;
}

/**
 * hpet_get_frequency - Get HPET frequency
 *
 * Returns: Frequency in Hz
 */
u64 hpet_get_frequency(void)
{
    return hpet_manager.hpet.frequency;
}

/*===========================================================================*/
/*                         HPET AS CLOCKSOURCE                               */
/*===========================================================================*/

/**
 * hpet_clocksource_read - Read HPET as clocksource
 *
 * Returns: Current counter value
 */
static u64 hpet_clocksource_read(void)
{
    return hpet_read_counter();
}

/**
 * hpet_register_clocksource - Register HPET as clocksource
 *
 * Returns: 0 on success, negative error code on failure
 */
int hpet_register_clocksource(void)
{
    if (!hpet_manager.initialized) {
        return -ENODEV;
    }

    pr_info("HPET: Registering as clocksource (%llu Hz)\n",
            hpet_manager.hpet.frequency);

    /* Would register with clocksource subsystem */

    return 0;
}

/*===========================================================================*/
/*                         HPET AS CLOCK EVENT                               */
/*===========================================================================*/

/**
 * hpet_set_next_event - Set next HPET event
 * @delta_ns: Delta in nanoseconds
 *
 * Returns: 0 on success, negative error code on failure
 */
int hpet_set_next_event(u64 delta_ns)
{
    u64 current, target;

    if (!hpet_manager.initialized) {
        return -ENODEV;
    }

    current = hpet_read_counter();
    target = current + (delta_ns * 1000000 / hpet_manager.hpet.period);

    hpet_timer_set_comparator(0, target);

    return 0;
}

/**
 * hpet_set_mode - Set HPET mode
 * @mode: Timer mode
 *
 * Returns: 0 on success, negative error code on failure
 */
int hpet_set_mode(u32 mode)
{
    if (!hpet_manager.initialized) {
        return -ENODEV;
    }

    /* Configure timer 0 based on mode */
    hpet_timer_setup(0, 0, (mode == TIMER_TYPE_PERIODIC));

    return 0;
}

/*===========================================================================*/
/*                         HPET INTERRUPT HANDLER                            */
/*===========================================================================*/

/**
 * hpet_interrupt - HPET interrupt handler
 * @timer_id: Timer ID that triggered interrupt
 *
 * Returns: IRQ_HANDLED if handled, IRQ_NONE otherwise
 */
irqreturn_t hpet_interrupt(u32 timer_id)
{
    hpet_timer_t *timer;

    if (!hpet_manager.initialized || timer_id >= hpet_manager.num_timers) {
        return IRQ_NONE;
    }

    timer = &hpet_manager.timers[timer_id];

    spin_lock(&hpet_manager.lock);

    hpet_manager.total_interrupts++;

    /* Clear interrupt status */
    /* (HPET doesn't have explicit interrupt clear) */

    spin_unlock(&hpet_manager.lock);

    /* Call registered handler if any */
    generic_irq_handler(timer->irq);

    return IRQ_HANDLED;
}

/*===========================================================================*/
/*                         HPET INFORMATION                                  */
/*===========================================================================*/

/**
 * hpet_dump_info - Dump HPET information for debugging
 */
void hpet_dump_info(void)
{
    u32 i;

    if (!hpet_manager.initialized) {
        pr_info("HPET: Not initialized\n");
        return;
    }

    pr_info("HPET Information:\n");
    pr_info("  Vendor ID: 0x%04X\n", hpet_manager.hpet.vendor_id);
    pr_info("  Revision: %u.%u\n",
            (hpet_manager.hpet.revision >> 4) & 0xF,
            hpet_manager.hpet.revision & 0xF);
    pr_info("  Timers: %u\n", hpet_manager.hpet.num_timers);
    pr_info("  Period: %llu femtoseconds\n", hpet_manager.hpet.period);
    pr_info("  Frequency: %llu Hz\n", hpet_manager.hpet.frequency);
    pr_info("  Legacy Replacement: %s\n",
            hpet_manager.hpet.legacy_replacement ? "Yes" : "No");
    pr_info("  Enabled: %s\n", hpet_manager.hpet.enabled ? "Yes" : "No");
    pr_info("  Total Interrupts: %llu\n", hpet_manager.total_interrupts);
    pr_info("  Total Comparisons: %llu\n", hpet_manager.total_comparisons);
    pr_info("\n");

    pr_info("Timer Configuration:\n");
    for (i = 0; i < hpet_manager.num_timers; i++) {
        hpet_timer_t *timer = &hpet_manager.timers[i];
        u32 conf_offset = HPET_T0_CONF + (i * 0x20);
        u64 tconf = hpet_read(conf_offset);

        pr_info("  Timer %u: IRQ=%u, Enabled=%s, Periodic=%s, Legacy=%s\n",
                i, timer->irq,
                timer->enabled ? "Yes" : "No",
                timer->periodic ? "Yes" : "No",
                timer->legacy ? "Yes" : "No");
        pr_info("    Config: 0x%016X\n", (u32)tconf);
    }
}

/*===========================================================================*/
/*                         HPET SHUTDOWN                                     */
/*===========================================================================*/

/**
 * hpet_shutdown - Shutdown HPET subsystem
 */
void hpet_shutdown(void)
{
    u32 i;

    pr_info("HPET: Shutting down...\n");

    /* Disable all timers */
    for (i = 0; i < hpet_manager.num_timers; i++) {
        hpet_timer_disable(i);
    }

    /* Disable HPET */
    hpet_disable();

    hpet_manager.initialized = false;

    pr_info("HPET: Shutdown complete\n");
}
