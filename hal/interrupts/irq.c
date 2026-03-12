/*
 * NEXUS OS - Hardware Abstraction Layer
 * hal/interrupts/irq.c - IRQ Management
 *
 * Implements IRQ request, free, enable, disable, and handler management
 */

#include "../hal.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         IRQ DATA STRUCTURES                               */
/*===========================================================================*/

/* IRQ return values */
#define IRQ_NONE        0
#define IRQ_HANDLED     1
#define IRQ_WAKE_THREAD 2

typedef u32 irqreturn_t;

/**
 * irq_action_t - IRQ action (handler registration)
 */
typedef struct irq_action {
    struct list_head list;          /* List entry */
    irq_handler_t handler;          /* Interrupt handler */
    void *dev_id;                   /* Device ID for shared IRQs */
    u32 flags;                      /* IRQ flags */
    u32 type;                       /* Trigger type */
    char name[32];                  /* Device name */
    bool disabled;                  /* Handler disabled */
} irq_action_t;

/**
 * irq_desc_extended_t - Extended IRQ descriptor
 */
typedef struct irq_desc_extended {
    irq_desc_t desc;                /* Base descriptor */
    struct list_head actions;       /* Action list (for shared IRQs) */
    u32 action_count;               /* Number of actions */
    u32 depth;                      /* Disable depth */
    u32 wake_depth;                 /* Wake enable depth */
    u64 last_irq_time;              /* Last IRQ timestamp */
    u64 total_handler_time;         /* Total handler time */
    u64 max_handler_time;           /* Maximum handler time */
    u32 affinity;                   /* CPU affinity */
    struct irq_chip *chip;          /* IRQ chip */
    void *chip_data;                /* Chip-specific data */
    bool suspended;                 /* IRQ suspended */
    bool threaded;                  /* Threaded IRQ */
    spinlock_t lock;                /* Descriptor lock */
} irq_desc_extended_t;

/**
 * irq_chip_t - IRQ chip operations
 */
typedef struct irq_chip {
    char name[32];                  /* Chip name */

    /* Operations */
    int (*irq_enable)(u32 irq);
    int (*irq_disable)(u32 irq);
    int (*irq_ack)(u32 irq);
    int (*irq_mask)(u32 irq);
    int (*irq_unmask)(u32 irq);
    int (*irq_set_type)(u32 irq, u32 type);
    int (*irq_set_affinity)(u32 irq, u32 cpu);
    int (*irq_eoi)(u32 irq);

    /* Power management */
    int (*irq_suspend)(u32 irq);
    int (*irq_resume)(u32 irq);
    int (*irq_set_wake)(u32 irq, bool enable);
} irq_chip_t;

/**
 * irq_manager_t - IRQ manager global data
 */
typedef struct irq_manager {
    irq_desc_extended_t *irqs;      /* IRQ descriptor array */
    u32 num_irqs;                   /* Number of IRQs */
    u32 num_registered;             /* Number of registered IRQs */

    u64 total_interrupts;           /* Total interrupt count */
    u64 total_shared;               /* Shared interrupt count */
    u64 total_spurious;             /* Spurious interrupt count */

    spinlock_t lock;                /* Global lock */
    bool initialized;               /* Initialization flag */
} irq_manager_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

#define NR_IRQS     256             /* Default IRQ count */

static irq_manager_t irq_manager __aligned(CACHE_LINE_SIZE);
static irq_desc_extended_t irq_descs[NR_IRQS] __aligned(CACHE_LINE_SIZE);

/*===========================================================================*/
/*                         INTERNAL FUNCTIONS                                */
/*===========================================================================*/

/**
 * irq_validate - Validate IRQ number
 * @irq: IRQ number
 *
 * Returns: 0 on success, -EINVAL on failure
 */
static inline int irq_validate(u32 irq)
{
    if (!irq_manager.initialized) {
        return -ENODEV;
    }

    if (irq >= irq_manager.num_irqs) {
        pr_err("IRQ: Invalid IRQ %u (max %u)\n", irq, irq_manager.num_irqs - 1);
        return -EINVAL;
    }

    return 0;
}

/**
 * irq_desc_get - Get IRQ descriptor
 * @irq: IRQ number
 *
 * Returns: Pointer to descriptor, or NULL on failure
 */
static irq_desc_extended_t *irq_desc_get(u32 irq)
{
    if (irq_validate(irq) < 0) {
        return NULL;
    }

    return &irq_descs[irq];
}

/**
 * irq_action_create - Create IRQ action
 * @handler: Interrupt handler
 * @dev_id: Device ID
 * @flags: IRQ flags
 * @name: Device name
 *
 * Returns: Pointer to new action, or NULL on failure
 */
static irq_action_t *irq_action_create(irq_handler_t handler, void *dev_id,
                                        u32 flags, const char *name)
{
    irq_action_t *action;

    action = kzalloc(sizeof(irq_action_t));
    if (!action) {
        return NULL;
    }

    action->handler = handler;
    action->dev_id = dev_id;
    action->flags = flags;
    action->disabled = false;

    if (name) {
        strncpy(action->name, name, sizeof(action->name) - 1);
    } else {
        snprintf(action->name, sizeof(action->name), "irq_%u", irq);
    }

    return action;
}

/**
 * irq_action_destroy - Destroy IRQ action
 * @action: Action to destroy
 */
static void irq_action_destroy(irq_action_t *action)
{
    if (action) {
        kfree(action);
    }
}

/*===========================================================================*/
/*                         IRQ INITIALIZATION                                */
/*===========================================================================*/

/**
 * interrupt_early_init - Early interrupt initialization
 */
void interrupt_early_init(void)
{
    u32 i;

    pr_info("IRQ: Early interrupt initialization...\n");

    spin_lock_init_named(&irq_manager.lock, "irq_manager");

    irq_manager.irqs = irq_descs;
    irq_manager.num_irqs = NR_IRQS;
    irq_manager.num_registered = 0;

    irq_manager.total_interrupts = 0;
    irq_manager.total_shared = 0;
    irq_manager.total_spurious = 0;

    irq_manager.initialized = false;

    /* Initialize all IRQ descriptors */
    for (i = 0; i < NR_IRQS; i++) {
        irq_desc_extended_t *desc = &irq_descs[i];

        spin_lock_init_named(&desc->lock, "irq_desc");
        INIT_LIST_HEAD(&desc->actions);

        desc->desc.irq = i;
        desc->desc.flags = 0;
        desc->desc.type = IRQ_TYPE_LEVEL;
        desc->desc.handler = NULL;
        desc->desc.dev_id = NULL;
        desc->desc.cpu = 0;
        desc->desc.cpumask = 0;

        atomic_set(&desc->desc.count, 0);
        atomic_set(&desc->desc.unhandled, 0);

        desc->action_count = 0;
        desc->depth = 0;
        desc->wake_depth = 0;
        desc->last_irq_time = 0;
        desc->total_handler_time = 0;
        desc->max_handler_time = 0;
        desc->affinity = 0;
        desc->chip = NULL;
        desc->chip_data = NULL;
        desc->suspended = false;
        desc->threaded = false;
    }

    /* Initialize architecture-specific interrupt controller */
#if defined(ARCH_X86_64)
    apic_init();
#elif defined(ARCH_ARM64)
    gic_init();
#endif
}

/**
 * interrupt_init - Main interrupt initialization
 */
void interrupt_init(void)
{
    pr_info("IRQ: Initializing interrupt subsystem...\n");

    irq_manager.initialized = true;

    pr_info("IRQ: Initialized %u IRQs\n", irq_manager.num_irqs);
}

/**
 * interrupt_late_init - Late interrupt initialization
 */
void interrupt_late_init(void)
{
    pr_info("IRQ: Late interrupt initialization complete\n");
}

/*===========================================================================*/
/*                         IRQ REQUEST/FREE                                  */
/*===========================================================================*/

/**
 * request_irq - Request an IRQ
 * @irq: IRQ number
 * @handler: Interrupt handler
 * @flags: IRQ flags
 * @name: Device name
 * @dev_id: Device ID (for shared IRQs)
 *
 * Returns: 0 on success, negative error code on failure
 */
int request_irq(u32 irq, irq_handler_t handler, u32 flags,
                const char *name, void *dev_id)
{
    irq_desc_extended_t *desc;
    irq_action_t *action;
    int ret = 0;

    if (irq_validate(irq) < 0) {
        return -EINVAL;
    }

    if (!handler) {
        pr_err("IRQ: NULL handler for IRQ %u\n", irq);
        return -EINVAL;
    }

    desc = irq_desc_get(irq);
    if (!desc) {
        return -EINVAL;
    }

    spin_lock(&desc->lock);

    /* Check if IRQ is already registered (non-shared) */
    if (desc->action_count > 0 && !(flags & IRQF_SHARED)) {
        pr_err("IRQ: IRQ %u already registered (not shared)\n", irq);
        ret = -EBUSY;
        goto out_unlock;
    }

    /* Check if existing handlers allow sharing */
    if (desc->action_count > 0) {
        irq_action_t *existing;

        list_for_each_entry(existing, &desc->actions, list) {
            if (!(existing->flags & IRQF_SHARED)) {
                pr_err("IRQ: IRQ %u not shared by existing handler\n", irq);
                ret = -EBUSY;
                goto out_unlock;
            }
        }
    }

    /* Create action */
    action = irq_action_create(handler, dev_id, flags, name);
    if (!action) {
        ret = -ENOMEM;
        goto out_unlock;
    }

    /* Add to action list */
    list_add_tail(&action->list, &desc->actions);
    desc->action_count++;

    /* Update descriptor */
    if (desc->action_count == 1) {
        desc->desc.handler = handler;
        desc->desc.dev_id = dev_id;
        desc->desc.flags = flags;
        strncpy(desc->desc.name, name, sizeof(desc->desc.name) - 1);
    }

    /* Enable the IRQ */
    if (desc->chip && desc->chip->irq_unmask) {
        desc->chip->irq_unmask(irq);
    }

    irq_manager.num_registered++;

    pr_debug("IRQ: Registered IRQ %u for %s\n", irq, name);

out_unlock:
    spin_unlock(&desc->lock);

    return ret;
}

/**
 * free_irq - Free an IRQ
 * @irq: IRQ number
 * @dev_id: Device ID (to identify handler for shared IRQs)
 */
void free_irq(u32 irq, void *dev_id)
{
    irq_desc_extended_t *desc;
    irq_action_t *action, *tmp;
    bool found = false;

    if (irq_validate(irq) < 0) {
        return;
    }

    desc = irq_desc_get(irq);
    if (!desc) {
        return;
    }

    spin_lock(&desc->lock);

    /* Find and remove the action */
    list_for_each_entry_safe(action, tmp, &desc->actions, list) {
        if (action->dev_id == dev_id) {
            list_del(&action->list);
            irq_action_destroy(action);
            desc->action_count--;
            found = true;
            break;
        }
    }

    if (!found) {
        pr_err("IRQ: No handler found for IRQ %u, dev_id %p\n", irq, dev_id);
        spin_unlock(&desc->lock);
        return;
    }

    /* Update descriptor */
    if (desc->action_count == 0) {
        desc->desc.handler = NULL;
        desc->desc.dev_id = NULL;
        desc->desc.flags = 0;

        /* Mask the IRQ */
        if (desc->chip && desc->chip->irq_mask) {
            desc->chip->irq_mask(irq);
        }

        irq_manager.num_registered--;
    } else {
        /* Update to first remaining handler */
        action = list_first_entry(&desc->actions, irq_action_t, list);
        desc->desc.handler = action->handler;
        desc->desc.dev_id = action->dev_id;
    }

    pr_debug("IRQ: Freed IRQ %u\n", irq);

    spin_unlock(&desc->lock);
}

/*===========================================================================*/
/*                         IRQ ENABLE/DISABLE                                */
/*===========================================================================*/

/**
 * enable_irq - Enable an IRQ
 * @irq: IRQ number
 */
void enable_irq(u32 irq)
{
    irq_desc_extended_t *desc;

    if (irq_validate(irq) < 0) {
        return;
    }

    desc = irq_desc_get(irq);
    if (!desc) {
        return;
    }

    spin_lock(&desc->lock);

    if (desc->depth > 0) {
        desc->depth--;
        if (desc->depth == 0) {
            if (desc->chip && desc->chip->irq_unmask) {
                desc->chip->irq_unmask(irq);
            }
        }
    }

    spin_unlock(&desc->lock);
}

/**
 * disable_irq - Disable an IRQ (waits for handler to complete)
 * @irq: IRQ number
 */
void disable_irq(u32 irq)
{
    irq_desc_extended_t *desc;

    if (irq_validate(irq) < 0) {
        return;
    }

    desc = irq_desc_get(irq);
    if (!desc) {
        return;
    }

    spin_lock(&desc->lock);

    if (desc->depth == 0) {
        if (desc->chip && desc->chip->irq_mask) {
            desc->chip->irq_mask(irq);
        }
    }
    desc->depth++;

    spin_unlock(&desc->lock);

    /* Wait for any running handlers to complete */
    /* (Implementation depends on threading model) */
}

/**
 * disable_irq_nosync - Disable an IRQ (doesn't wait for handler)
 * @irq: IRQ number
 */
void disable_irq_nosync(u32 irq)
{
    irq_desc_extended_t *desc;

    if (irq_validate(irq) < 0) {
        return;
    }

    desc = irq_desc_get(irq);
    if (!desc) {
        return;
    }

    spin_lock(&desc->lock);

    if (desc->depth == 0) {
        if (desc->chip && desc->chip->irq_mask) {
            desc->chip->irq_mask(irq);
        }
    }
    desc->depth++;

    spin_unlock(&desc->lock);
}

/**
 * irq_to_desc - Get IRQ descriptor
 * @irq: IRQ number
 *
 * Returns: Pointer to irq_desc_t, or NULL on failure
 */
irq_desc_t *irq_to_desc(u32 irq)
{
    irq_desc_extended_t *desc;

    desc = irq_desc_get(irq);
    if (!desc) {
        return NULL;
    }

    return &desc->desc;
}

/*===========================================================================*/
/*                         LOCAL IRQ CONTROL                                 */
/*===========================================================================*/

/**
 * local_irq_enable - Enable local interrupts
 */
void local_irq_enable(void)
{
#if defined(ARCH_X86_64)
    __asm__ __volatile__("sti" ::: "memory");
#elif defined(ARCH_ARM64)
    __asm__ __volatile__("msr daifclr, #2" ::: "memory");
#endif
}

/**
 * local_irq_disable - Disable local interrupts
 */
void local_irq_disable(void)
{
#if defined(ARCH_X86_64)
    __asm__ __volatile__("cli" ::: "memory");
#elif defined(ARCH_ARM64)
    __asm__ __volatile__("msr daifset, #2" ::: "memory");
#endif
}

/**
 * local_irq_save - Save and disable local interrupts
 *
 * Returns: Previous interrupt state
 */
bool local_irq_save(void)
{
    bool flags;

#if defined(ARCH_X86_64)
    u64 rflags;
    __asm__ __volatile__("pushfq; pop %0" : "=rm"(rflags));
    flags = !!(rflags & 0x200);
    local_irq_disable();
#elif defined(ARCH_ARM64)
    u64 daif;
    __asm__ __volatile__("mrs %0, daif" : "=r"(daif));
    flags = !(daif & 0x80);
    local_irq_disable();
#endif

    return flags;
}

/**
 * local_irq_restore - Restore local interrupt state
 * @flags: Previous interrupt state
 */
void local_irq_restore(bool flags)
{
    if (flags) {
        local_irq_enable();
    }
}

/*===========================================================================*/
/*                         IPI (INTER-PROCESSOR INTERRUPT)                   */
/*===========================================================================*/

/**
 * arch_send_ipi - Send IPI to a specific CPU
 * @cpu: Target CPU ID
 * @vector: IPI vector
 */
void arch_send_ipi(u32 cpu, u32 vector)
{
#if defined(ARCH_X86_64)
    x86_send_ipi(cpu, vector);
#elif defined(ARCH_ARM64)
    arm_send_ipi(cpu, vector);
#endif
}

/**
 * arch_send_ipi_all - Send IPI to all CPUs
 * @vector: IPI vector
 */
void arch_send_ipi_all(u32 vector)
{
#if defined(ARCH_X86_64)
    x86_send_ipi_all(vector);
#elif defined(ARCH_ARM64)
    arm_send_ipi_all(vector);
#endif
}

/**
 * arch_send_ipi_mask - Send IPI to CPUs in mask
 * @mask: CPU mask
 * @vector: IPI vector
 */
void arch_send_ipi_mask(u64 mask, u32 vector)
{
    u32 cpu;

    for (cpu = 0; cpu < 64; cpu++) {
        if (mask & (1ULL << cpu)) {
            arch_send_ipi(cpu, vector);
        }
    }
}

/*===========================================================================*/
/*                         IRQ HANDLER                                       */
/*===========================================================================*/

/**
 * generic_irq_handler - Generic IRQ handler
 * @irq: IRQ number
 *
 * Returns: IRQ_HANDLED if handled, IRQ_NONE otherwise
 */
irqreturn_t generic_irq_handler(u32 irq)
{
    irq_desc_extended_t *desc;
    irq_action_t *action;
    irqreturn_t ret = IRQ_NONE;
    u64 start_time, end_time;

    desc = irq_desc_get(irq);
    if (!desc) {
        irq_manager.total_spurious++;
        return IRQ_NONE;
    }

    spin_lock(&desc->lock);

    /* Update statistics */
    atomic_inc(&desc->desc.count);
    irq_manager.total_interrupts++;
    desc->last_irq_time = get_time_ns();

    /* Call all handlers (for shared IRQs) */
    list_for_each_entry(action, &desc->actions, list) {
        if (action->disabled) {
            continue;
        }

        start_time = get_time_ns();

        if (action->handler) {
            irqreturn_t action_ret = action->handler(irq, action->dev_id);
            if (action_ret == IRQ_HANDLED) {
                ret = IRQ_HANDLED;
            }
        }

        end_time = get_time_ns();

        /* Update timing statistics */
        desc->total_handler_time += (end_time - start_time);
        if ((end_time - start_time) > desc->max_handler_time) {
            desc->max_handler_time = (end_time - start_time);
        }
    }

    if (ret == IRQ_NONE) {
        atomic_inc(&desc->desc.unhandled);
        pr_debug("IRQ: Unhandled IRQ %u\n", irq);
    }

    /* End of interrupt */
    if (desc->chip && desc->chip->irq_eoi) {
        desc->chip->irq_eoi(irq);
    }

    spin_unlock(&desc->lock);

    return ret;
}

/*===========================================================================*/
/*                         IRQ STATISTICS                                    */
/*===========================================================================*/

/**
 * irq_get_stats - Get IRQ statistics
 * @irq: IRQ number
 * @stats: Output statistics structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int irq_get_stats(u32 irq, struct irq_stats *stats)
{
    irq_desc_extended_t *desc;

    if (irq_validate(irq) < 0) {
        return -EINVAL;
    }

    if (!stats) {
        return -EINVAL;
    }

    desc = irq_desc_get(irq);
    if (!desc) {
        return -EINVAL;
    }

    spin_lock(&desc->lock);

    stats->count = atomic_read(&desc->desc.count);
    stats->unhandled = atomic_read(&desc->desc.unhandled);
    stats->last_irq_time = desc->last_irq_time;
    stats->total_handler_time = desc->total_handler_time;
    stats->max_handler_time = desc->max_handler_time;
    stats->action_count = desc->action_count;
    stats->affinity = desc->affinity;

    strncpy(stats->name, desc->desc.name, sizeof(stats->name) - 1);

    spin_unlock(&desc->lock);

    return 0;
}

/**
 * irq_dump_stats - Dump IRQ statistics for debugging
 */
void irq_dump_stats(void)
{
    u32 i;

    pr_info("IRQ Statistics:\n");
    pr_info("  Total Interrupts: %llu\n", irq_manager.total_interrupts);
    pr_info("  Shared Interrupts: %llu\n", irq_manager.total_shared);
    pr_info("  Spurious Interrupts: %llu\n", irq_manager.total_spurious);
    pr_info("  Registered IRQs: %u / %u\n",
            irq_manager.num_registered, irq_manager.num_irqs);
    pr_info("\n");

    pr_info("IRQ Handlers:\n");

    for (i = 0; i < irq_manager.num_irqs; i++) {
        irq_desc_extended_t *desc = &irq_descs[i];

        if (desc->action_count > 0) {
            pr_info("  IRQ %3u: %8u handlers: %s\n",
                    i, atomic_read(&desc->desc.count), desc->desc.name);
        }
    }
}

/*===========================================================================*/
/*                         ARCHITECTURE-SPECIFIC                             */
/*===========================================================================*/

#if defined(ARCH_X86_64)
/**
 * x86_send_ipi - Send IPI on x86_64
 * @cpu: Target CPU
 * @vector: IPI vector
 */
void x86_send_ipi(u32 cpu, u32 vector)
{
    /* Placeholder - would use APIC IPI */
    pr_debug("IRQ: x86_64 IPI to CPU %u, vector %u (placeholder)\n", cpu, vector);
}

/**
 * x86_send_ipi_all - Send IPI to all CPUs on x86_64
 * @vector: IPI vector
 */
void x86_send_ipi_all(u32 vector)
{
    /* Placeholder - would use APIC broadcast IPI */
    pr_debug("IRQ: x86_64 broadcast IPI, vector %u (placeholder)\n", vector);
}
#endif

#if defined(ARCH_ARM64)
/**
 * arm_send_ipi - Send IPI on ARM64
 * @cpu: Target CPU
 * @vector: IPI vector
 */
void arm_send_ipi(u32 cpu, u32 vector)
{
    /* Placeholder - would use GIC IPI */
    pr_debug("IRQ: ARM64 IPI to CPU %u, vector %u (placeholder)\n", cpu, vector);
}

/**
 * arm_send_ipi_all - Send IPI to all CPUs on ARM64
 * @vector: IPI vector
 */
void arm_send_ipi_all(u32 vector)
{
    /* Placeholder - would use GIC broadcast IPI */
    pr_debug("IRQ: ARM64 broadcast IPI, vector %u (placeholder)\n", vector);
}
#endif
