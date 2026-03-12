/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/timer/arm_timer.c - ARM Timer Subsystem
 *
 * Implements ARM Generic Timer and architectural timer support for ARM64
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ARM TIMER CONSTANTS                               */
/*===========================================================================*/

/* System Register Encodings */
#define CNTFRQ_EL0              S3_3_C14_C0_0
#define CNTPCT_EL0              S3_3_C14_C0_1
#define CNTVCT_EL0              S3_3_C14_C0_2
#define CNTP_CTL_EL0            S3_3_C14_C2_1
#define CNTP_CVAL_EL0           S3_3_C14_C2_2
#define CNTP_TVAL_EL0           S3_3_C14_C2_3
#define CNTV_CTL_EL0            S3_3_C14_C3_1
#define CNTV_CVAL_EL0           S3_3_C14_C3_2
#define CNTV_TVAL_EL0           S3_3_C14_C3_3

/* Timer Control Register Bits */
#define CNT_CTL_ENABLE          (1 << 0)
#define CNT_CTL_IMASK           (1 << 1)
#define CNT_CTL_ISTATUS         (1 << 2)

/* Timer Types */
#define ARM_TIMER_PHYSICAL      0
#define ARM_TIMER_VIRTUAL       1
#define ARM_TIMER_HYP           2

/* Secure States */
#define ARM_TIMER_SECURE        0
#define ARM_TIMER_NON_SECURE    1

/*===========================================================================*/
/*                         ARM TIMER DATA STRUCTURES                         */
/*===========================================================================*/

/**
 * arm_timer_t - ARM timer structure
 */
typedef struct arm_timer {
    u32 type;                     /* Timer type (physical/virtual) */
    u32 irq;                      /* IRQ number */
    u64 frequency;                /* Timer frequency */
    u64 period_ns;                /* Current period */
    bool enabled;                 /* Timer enabled */
    bool periodic;                /* Periodic mode */
    bool secure;                  /* Secure state */
} arm_timer_t;

/**
 * arch_timer_data_t - Architectural timer data
 */
typedef struct arch_timer_data {
    u64 frequency;                /* Timer frequency */
    u64 counter_mask;             /* Counter mask */
    u32 flags;                    /* Timer flags */
    bool always_on;               /* Always-on timer */
    bool initialized;             /* Initialized flag */
} arch_timer_data_t;

/**
 * timer_manager_t - ARM timer manager global data
 */
typedef struct arm_timer_manager {
    arm_timer_t physical_timer;   /* Physical timer */
    arm_timer_t virtual_timer;    /* Virtual timer */
    arch_timer_data_t arch_timer; /* Architectural timer */

    u64 total_interrupts;         /* Total timer interrupts */
    u64 total_oneshots;           /* Total one-shot events */
    u64 total_periodic;           /* Total periodic events */

    spinlock_t lock;              /* Global lock */
    bool initialized;             /* Manager initialized */
} arm_timer_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static arm_timer_manager_t timer_manager __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         TIMER REGISTER ACCESS                             */
/*===========================================================================*/

/**
 * timer_read_cntfrq - Read timer frequency
 *
 * Returns: Frequency in Hz
 */
static u64 timer_read_cntfrq(void)
{
    u32 freq;
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

/**
 * timer_read_cntpct - Read physical counter
 *
 * Returns: Current physical counter value
 */
static u64 timer_read_cntpct(void)
{
    u64 cnt;
    __asm__ __volatile__("mrs %0, cntpct_el0" : "=r"(cnt));
    return cnt;
}

/**
 * timer_read_cntvct - Read virtual counter
 *
 * Returns: Current virtual counter value
 */
static u64 timer_read_cntvct(void)
{
    u64 cnt;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(cnt));
    return cnt;
}

/**
 * timer_read_cntp_ctl - Read physical timer control
 *
 * Returns: Control register value
 */
static u32 timer_read_cntp_ctl(void)
{
    u32 ctl;
    __asm__ __volatile__("mrs %0, cntp_ctl_el0" : "=r"(ctl));
    return ctl;
}

/**
 * timer_write_cntp_ctl - Write physical timer control
 * @val: Value to write
 */
static void timer_write_cntp_ctl(u32 val)
{
    __asm__ __volatile__("msr cntp_ctl_el0, %0" : : "r"(val));
}

/**
 * timer_read_cntp_cval - Read physical timer comparator
 *
 * Returns: Comparator value
 */
static u64 timer_read_cntp_cval(void)
{
    u64 cval;
    __asm__ __volatile__("mrs %0, cntp_cval_el0" : "=r"(cval));
    return cval;
}

/**
 * timer_write_cntp_cval - Write physical timer comparator
 * @val: Value to write
 */
static void timer_write_cntp_cval(u64 val)
{
    __asm__ __volatile__("msr cntp_cval_el0, %0" : : "r"(val));
}

/**
 * timer_read_cntp_tval - Read physical timer value
 *
 * Returns: Timer value (countdown)
 */
static s64 timer_read_cntp_tval(void)
{
    s64 tval;
    __asm__ __volatile__("mrs %0, cntp_tval_el0" : "=r"(tval));
    return tval;
}

/**
 * timer_write_cntp_tval - Write physical timer value
 * @val: Value to write
 */
static void timer_write_cntp_tval(s64 val)
{
    __asm__ __volatile__("msr cntp_tval_el0, %0" : : "r"(val));
}

/**
 * timer_read_cntv_ctl - Read virtual timer control
 *
 * Returns: Control register value
 */
static u32 timer_read_cntv_ctl(void)
{
    u32 ctl;
    __asm__ __volatile__("mrs %0, cntv_ctl_el0" : "=r"(ctl));
    return ctl;
}

/**
 * timer_write_cntv_ctl - Write virtual timer control
 * @val: Value to write
 */
static void timer_write_cntv_ctl(u32 val)
{
    __asm__ __volatile__("msr cntv_ctl_el0, %0" : : "r"(val));
}

/**
 * timer_write_cntv_cval - Write virtual timer comparator
 * @val: Value to write
 */
static void timer_write_cntv_cval(u64 val)
{
    __asm__ __volatile__("msr cntv_cval_el0, %0" : : "r"(val));
}

/**
 * timer_write_cntv_tval - Write virtual timer value
 * @val: Value to write
 */
static void timer_write_cntv_tval(s64 val)
{
    __asm__ __volatile__("msr cntv_tval_el0, %0" : : "r"(val));
}

/*===========================================================================*/
/*                         TIMER DETECTION                                   */
/*===========================================================================*/

/**
 * arm_timer_detect - Detect ARM timer hardware
 *
 * Returns: 0 on success, negative error code on failure
 */
static int arm_timer_detect(void)
{
    u64 freq;

    pr_info("ARM Timer: Detecting ARM generic timer...\n");

    /* Read timer frequency */
    freq = timer_read_cntfrq();

    if (freq == 0) {
        pr_err("ARM Timer: Invalid frequency detected\n");
        return -ENODEV;
    }

    timer_manager.arch_timer.frequency = freq;
    timer_manager.arch_timer.counter_mask = 0xFFFFFFFFFFFFFFFFULL;
    timer_manager.arch_timer.always_on = true;
    timer_manager.arch_timer.initialized = true;

    pr_info("ARM Timer: Frequency: %llu Hz\n", freq);

    return 0;
}

/*===========================================================================*/
/*                         TIMER INITIALIZATION                              */
/*===========================================================================*/

/**
 * arm_timer_early_init - Early ARM timer initialization
 */
void arm_timer_early_init(void)
{
    pr_info("ARM Timer: Early initialization...\n");

    spin_lock_init_named(&timer_manager.lock, "arm_timer");

    memset(&timer_manager.physical_timer, 0, sizeof(arm_timer_t));
    memset(&timer_manager.virtual_timer, 0, sizeof(arm_timer_t));
    memset(&timer_manager.arch_timer, 0, sizeof(arch_timer_data_t));

    timer_manager.total_interrupts = 0;
    timer_manager.total_oneshots = 0;
    timer_manager.total_periodic = 0;

    timer_manager.initialized = false;

    /* Detect timer hardware */
    arm_timer_detect();
}

/**
 * arm_timer_init - Main ARM timer initialization
 *
 * Returns: 0 on success, negative error code on failure
 */
int arm_timer_init(void)
{
    pr_info("ARM Timer: Initializing timer subsystem...\n");

    /* Initialize physical timer */
    timer_manager.physical_timer.type = ARM_TIMER_PHYSICAL;
    timer_manager.physical_timer.irq = 30; /* PPI 30 */
    timer_manager.physical_timer.frequency = timer_manager.arch_timer.frequency;
    timer_manager.physical_timer.enabled = false;
    timer_manager.physical_timer.periodic = false;
    timer_manager.physical_timer.secure = false;

    /* Initialize virtual timer */
    timer_manager.virtual_timer.type = ARM_TIMER_VIRTUAL;
    timer_manager.virtual_timer.irq = 27; /* PPI 27 */
    timer_manager.virtual_timer.frequency = timer_manager.arch_timer.frequency;
    timer_manager.virtual_timer.enabled = false;
    timer_manager.virtual_timer.periodic = false;
    timer_manager.virtual_timer.secure = false;

    /* Disable timers initially */
    timer_write_cntp_ctl(0);
    timer_write_cntv_ctl(0);

    timer_manager.initialized = true;

    pr_info("ARM Timer: Initialization complete\n");

    return 0;
}

/*===========================================================================*/
/*                         PHYSICAL TIMER MANAGEMENT                         */
/*===========================================================================*/

/**
 * arm_timer_physical_enable - Enable physical timer
 */
void arm_timer_physical_enable(void)
{
    u32 ctl;

    spin_lock(&timer_manager.lock);

    ctl = timer_read_cntp_ctl();
    ctl |= CNT_CTL_ENABLE;
    ctl &= ~CNT_CTL_IMASK; /* Unmask interrupt */
    timer_write_cntp_ctl(ctl);

    timer_manager.physical_timer.enabled = true;

    spin_unlock(&timer_manager.lock);
}

/**
 * arm_timer_physical_disable - Disable physical timer
 */
void arm_timer_physical_disable(void)
{
    u32 ctl;

    spin_lock(&timer_manager.lock);

    ctl = timer_read_cntp_ctl();
    ctl &= ~CNT_CTL_ENABLE;
    timer_write_cntp_ctl(ctl);

    timer_manager.physical_timer.enabled = false;

    spin_unlock(&timer_manager.lock);
}

/**
 * arm_timer_physical_set_oneshot - Set physical timer one-shot
 * @ns: Timeout in nanoseconds
 */
void arm_timer_physical_set_oneshot(u64 ns)
{
    u64 ticks;
    u64 current;

    if (!timer_manager.initialized) {
        return;
    }

    /* Convert nanoseconds to ticks */
    ticks = (ns * timer_manager.arch_timer.frequency) / 1000000000ULL;

    /* Set comparator */
    current = timer_read_cntpct();
    timer_write_cntp_cval(current + ticks);

    /* Enable timer */
    timer_manager.physical_timer.periodic = false;
    arm_timer_physical_enable();

    timer_manager.total_oneshots++;
}

/**
 * arm_timer_physical_set_periodic - Set physical timer periodic
 * @ns: Period in nanoseconds
 */
void arm_timer_physical_set_periodic(u64 ns)
{
    u64 ticks;
    u64 current;

    if (!timer_manager.initialized) {
        return;
    }

    /* Convert nanoseconds to ticks */
    ticks = (ns * timer_manager.arch_timer.frequency) / 1000000000ULL;

    /* Set comparator */
    current = timer_read_cntpct();
    timer_write_cntp_cval(current + ticks);

    /* Enable timer in periodic mode */
    timer_manager.physical_timer.periodic = true;
    arm_timer_physical_enable();

    timer_manager.total_periodic++;
}

/**
 * arm_timer_physical_cancel - Cancel physical timer
 */
void arm_timer_physical_cancel(void)
{
    arm_timer_physical_disable();

    /* Clear comparator */
    timer_write_cntp_cval(0);
}

/*===========================================================================*/
/*                         VIRTUAL TIMER MANAGEMENT                          */
/*===========================================================================*/

/**
 * arm_timer_virtual_enable - Enable virtual timer
 */
void arm_timer_virtual_enable(void)
{
    u32 ctl;

    spin_lock(&timer_manager.lock);

    ctl = timer_read_cntv_ctl();
    ctl |= CNT_CTL_ENABLE;
    ctl &= ~CNT_CTL_IMASK; /* Unmask interrupt */
    timer_write_cntv_ctl(ctl);

    timer_manager.virtual_timer.enabled = true;

    spin_unlock(&timer_manager.lock);
}

/**
 * arm_timer_virtual_disable - Disable virtual timer
 */
void arm_timer_virtual_disable(void)
{
    u32 ctl;

    spin_lock(&timer_manager.lock);

    ctl = timer_read_cntv_ctl();
    ctl &= ~CNT_CTL_ENABLE;
    timer_write_cntv_ctl(ctl);

    timer_manager.virtual_timer.enabled = false;

    spin_unlock(&timer_manager.lock);
}

/**
 * arm_timer_virtual_set_oneshot - Set virtual timer one-shot
 * @ns: Timeout in nanoseconds
 */
void arm_timer_virtual_set_oneshot(u64 ns)
{
    u64 ticks;
    u64 current;

    if (!timer_manager.initialized) {
        return;
    }

    /* Convert nanoseconds to ticks */
    ticks = (ns * timer_manager.arch_timer.frequency) / 1000000000ULL;

    /* Set comparator */
    current = timer_read_cntvct();
    timer_write_cntv_cval(current + ticks);

    /* Enable timer */
    timer_manager.virtual_timer.periodic = false;
    arm_timer_virtual_enable();

    timer_manager.total_oneshots++;
}

/**
 * arm_timer_virtual_set_periodic - Set virtual timer periodic
 * @ns: Period in nanoseconds
 */
void arm_timer_virtual_set_periodic(u64 ns)
{
    u64 ticks;
    u64 current;

    if (!timer_manager.initialized) {
        return;
    }

    /* Convert nanoseconds to ticks */
    ticks = (ns * timer_manager.arch_timer.frequency) / 1000000000ULL;

    /* Set comparator */
    current = timer_read_cntvct();
    timer_write_cntv_cval(current + ticks);

    /* Enable timer in periodic mode */
    timer_manager.virtual_timer.periodic = true;
    arm_timer_virtual_enable();

    timer_manager.total_periodic++;
}

/**
 * arm_timer_virtual_cancel - Cancel virtual timer
 */
void arm_timer_virtual_cancel(void)
{
    arm_timer_virtual_disable();

    /* Clear comparator */
    timer_write_cntv_cval(0);
}

/*===========================================================================*/
/*                         TIMER COUNTER ACCESS                              */
/*===========================================================================*/

/**
 * arm_timer_read_counter - Read current counter value
 *
 * Returns: Current counter value
 */
u64 arm_timer_read_counter(void)
{
    return timer_read_cntvct();
}

/**
 * arm_timer_read_counter_ns - Read counter as nanoseconds
 *
 * Returns: Current time in nanoseconds
 */
u64 arm_timer_read_counter_ns(void)
{
    u64 ticks = timer_read_cntvct();
    u64 freq = timer_manager.arch_timer.frequency;

    if (freq == 0) {
        return 0;
    }

    return (ticks * 1000000000ULL) / freq;
}

/**
 * arm_timer_get_frequency - Get timer frequency
 *
 * Returns: Frequency in Hz
 */
u64 arm_timer_get_frequency(void)
{
    return timer_manager.arch_timer.frequency;
}

/*===========================================================================*/
/*                         TIMER AS CLOCKSOURCE                              */
/*===========================================================================*/

/**
 * arm_timer_clocksource_read - Read ARM timer as clocksource
 *
 * Returns: Current counter value
 */
static u64 arm_timer_clocksource_read(void)
{
    return timer_read_cntvct();
}

/**
 * arm_timer_register_clocksource - Register ARM timer as clocksource
 *
 * Returns: 0 on success, negative error code on failure
 */
int arm_timer_register_clocksource(void)
{
    if (!timer_manager.initialized) {
        return -ENODEV;
    }

    pr_info("ARM Timer: Registering as clocksource (%llu Hz)\n",
            timer_manager.arch_timer.frequency);

    /* Would register with clocksource subsystem */

    return 0;
}

/*===========================================================================*/
/*                         TIMER AS CLOCK EVENT                              */
/*===========================================================================*/

/**
 * arm_timer_set_next_event - Set next timer event
 * @delta_ns: Delta in nanoseconds
 *
 * Returns: 0 on success, negative error code on failure
 */
int arm_timer_set_next_event(u64 delta_ns)
{
    if (!timer_manager.initialized) {
        return -ENODEV;
    }

    arm_timer_physical_set_oneshot(delta_ns);

    return 0;
}

/**
 * arm_timer_set_mode - Set timer mode
 * @mode: Timer mode
 *
 * Returns: 0 on success, negative error code on failure
 */
int arm_timer_set_mode(u32 mode)
{
    if (!timer_manager.initialized) {
        return -ENODEV;
    }

    if (mode == TIMER_TYPE_PERIODIC) {
        /* Periodic mode - would use hypervisor timer if available */
        return -ENOSYS;
    }

    /* One-shot mode */
    return 0;
}

/*===========================================================================*/
/*                         TIMER INTERRUPT HANDLER                           */
/*===========================================================================*/

/**
 * arm_timer_interrupt - ARM timer interrupt handler
 * @timer_type: Timer type (physical/virtual)
 *
 * Returns: IRQ_HANDLED if handled, IRQ_NONE otherwise
 */
irqreturn_t arm_timer_interrupt(u32 timer_type)
{
    u32 ctl;
    u32 status;

    spin_lock(&timer_manager.lock);

    timer_manager.total_interrupts++;

    if (timer_type == ARM_TIMER_PHYSICAL) {
        /* Read and clear physical timer status */
        ctl = timer_read_cntp_ctl();
        status = ctl & CNT_CTL_ISTATUS;

        if (status) {
            /* Timer fired - acknowledge by writing new comparator */
            if (timer_manager.physical_timer.periodic) {
                /* Periodic: add period to current comparator */
                u64 cval = timer_read_cntp_cval();
                u64 ticks = (timer_manager.physical_timer.period_ns *
                            timer_manager.arch_timer.frequency) / 1000000000ULL;
                timer_write_cntp_cval(cval + ticks);
            }
        }
    } else {
        /* Read and clear virtual timer status */
        ctl = timer_read_cntv_ctl();
        status = ctl & CNT_CTL_ISTATUS;

        if (status) {
            /* Timer fired */
            if (timer_manager.virtual_timer.periodic) {
                /* Periodic: add period to current comparator */
                u64 cval = timer_read_cntp_cval();
                u64 ticks = (timer_manager.virtual_timer.period_ns *
                            timer_manager.arch_timer.frequency) / 1000000000ULL;
                timer_write_cntv_cval(cval + ticks);
            }
        }
    }

    spin_unlock(&timer_manager.lock);

    /* Call generic handler */
    generic_irq_handler(timer_type == ARM_TIMER_PHYSICAL ? 30 : 27);

    return IRQ_HANDLED;
}

/*===========================================================================*/
/*                         DELAY IMPLEMENTATION                              */
/*===========================================================================*/

/**
 * arm_delay_ns - Delay for specified nanoseconds using ARM timer
 * @ns: Delay in nanoseconds
 */
void arm_delay_ns(u64 ns)
{
    u64 start, now, elapsed;
    u64 freq = timer_manager.arch_timer.frequency;

    if (freq == 0) {
        return;
    }

    start = timer_read_cntvct();

    while (1) {
        now = timer_read_cntvct();

        /* Handle counter wrap */
        if (now >= start) {
            elapsed = now - start;
        } else {
            elapsed = (0xFFFFFFFFFFFFFFFFULL - start) + now;
        }

        if ((elapsed * 1000000000ULL / freq) >= ns) {
            break;
        }

        __asm__ __volatile__("isb" ::: "memory");
    }
}

/**
 * arm_delay_us - Delay for specified microseconds
 * @us: Delay in microseconds
 */
void arm_delay_us(u64 us)
{
    arm_delay_ns(us * 1000);
}

/**
 * arm_delay_ms - Delay for specified milliseconds
 * @ms: Delay in milliseconds
 */
void arm_delay_ms(u64 ms)
{
    u64 start, now;

    start = get_time_ms();

    while (1) {
        now = get_time_ms();
        if ((now - start) >= ms) {
            break;
        }
        __asm__ __volatile__("wfi" ::: "memory");
    }
}

/*===========================================================================*/
/*                         TIMER INFORMATION                                 */
/*===========================================================================*/

/**
 * arm_timer_dump_info - Dump ARM timer information
 */
void arm_timer_dump_info(void)
{
    if (!timer_manager.initialized) {
        pr_info("ARM Timer: Not initialized\n");
        return;
    }

    pr_info("ARM Timer Information:\n");
    pr_info("  Frequency: %llu Hz\n", timer_manager.arch_timer.frequency);
    pr_info("  Counter Mask: 0x%016X\n", (u32)timer_manager.arch_timer.counter_mask);
    pr_info("  Always On: %s\n", timer_manager.arch_timer.always_on ? "Yes" : "No");
    pr_info("  Total Interrupts: %llu\n", timer_manager.total_interrupts);
    pr_info("  Total One-shots: %llu\n", timer_manager.total_oneshots);
    pr_info("  Total Periodic: %llu\n", timer_manager.total_periodic);
    pr_info("\n");

    pr_info("Physical Timer:\n");
    pr_info("  IRQ: %u\n", timer_manager.physical_timer.irq);
    pr_info("  Enabled: %s\n", timer_manager.physical_timer.enabled ? "Yes" : "No");
    pr_info("  Periodic: %s\n", timer_manager.physical_timer.periodic ? "Yes" : "No");
    pr_info("  Control: 0x%08X\n", timer_read_cntp_ctl());
    pr_info("  Comparator: 0x%016X\n", (u32)timer_read_cntp_cval());
    pr_info("\n");

    pr_info("Virtual Timer:\n");
    pr_info("  IRQ: %u\n", timer_manager.virtual_timer.irq);
    pr_info("  Enabled: %s\n", timer_manager.virtual_timer.enabled ? "Yes" : "No");
    pr_info("  Periodic: %s\n", timer_manager.virtual_timer.periodic ? "Yes" : "No");
    pr_info("  Control: 0x%08X\n", timer_read_cntv_ctl());
}

/*===========================================================================*/
/*                         TIMER SHUTDOWN                                    */
/*===========================================================================*/

/**
 * arm_timer_shutdown - Shutdown ARM timer subsystem
 */
void arm_timer_shutdown(void)
{
    pr_info("ARM Timer: Shutting down...\n");

    /* Disable both timers */
    arm_timer_physical_disable();
    arm_timer_virtual_disable();

    timer_manager.initialized = false;

    pr_info("ARM Timer: Shutdown complete\n");
}
