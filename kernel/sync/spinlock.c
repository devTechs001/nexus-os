/*
 * NEXUS OS - Spinlock Implementation
 * kernel/sync/spinlock.c
 */

#include "sync.h"

/*===========================================================================*/
/*                         SPINLOCK CONFIGURATION                            */
/*===========================================================================*/

#define SPINLOCK_SPIN_LIMIT     10000
#define SPINLOCK_DEBUG          1

/* Architecture-specific pause instruction */
#define cpu_relax()             __asm__ __volatile__("pause" ::: "memory")

/*===========================================================================*/
/*                         SPINLOCK GLOBAL DATA                              */
/*===========================================================================*/

static struct {
    spinlock_t lock;              /* Global statistics lock */
    atomic_t total_locks;         /* Total lock acquisitions */
    atomic_t total_contentions;   /* Total contentions */
    atomic_t active_locks;        /* Currently held locks */
    u64 total_spin_time;          /* Total time spent spinning */
    u64 max_spin_time;            /* Maximum spin time */
} spinlock_global = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .total_locks = ATOMIC_INIT(0),
    .total_contentions = ATOMIC_INIT(0),
    .active_locks = ATOMIC_INIT(0),
    .total_spin_time = 0,
    .max_spin_time = 0,
};

/*===========================================================================*/
/*                         SPINLOCK INITIALIZATION                           */
/*===========================================================================*/

/**
 * spin_lock_init - Initialize a spinlock
 * @lock: Spinlock to initialize
 *
 * Initializes the spinlock to unlocked state with default values.
 */
void spin_lock_init(spinlock_t *lock)
{
    if (!lock) {
        return;
    }

    lock->lock = SPINLOCK_UNLOCKED;
    lock->magic = SPINLOCK_MAGIC;
    lock->name = "unnamed";
    lock->owner_cpu = 0xFFFFFFFF;
    lock->acquire_time = 0;
    lock->total_hold_time = 0;
    lock->acquire_count = 0;
    lock->contention_count = 0;

    pr_debug("Spinlock: Initialized lock %p\n", lock);
}

/**
 * spin_lock_init_named - Initialize a spinlock with a name
 * @lock: Spinlock to initialize
 * @name: Lock name for debugging
 */
void spin_lock_init_named(spinlock_t *lock, const char *name)
{
    if (!lock) {
        return;
    }

    lock->lock = SPINLOCK_UNLOCKED;
    lock->magic = SPINLOCK_MAGIC;
    lock->name = name ? name : "unnamed";
    lock->owner_cpu = 0xFFFFFFFF;
    lock->acquire_time = 0;
    lock->total_hold_time = 0;
    lock->acquire_count = 0;
    lock->contention_count = 0;

    pr_debug("Spinlock: Initialized lock '%s' at %p\n", name, lock);
}

/*===========================================================================*/
/*                         SPINLOCK ACQUISITION                              */
/*===========================================================================*/

/**
 * __spin_lock_slowpath - Slow path for spinlock acquisition
 * @lock: Spinlock to acquire
 *
 * Called when the fast path fails. Uses busy-waiting with exponential
 * backoff to acquire the lock.
 */
static void __spin_lock_slowpath(spinlock_t *lock)
{
    u32 spin_count = 0;
    u64 start_time = 0;
    u64 end_time;
    u64 spin_time;
    bool contended = false;

#if SPINLOCK_DEBUG
    start_time = get_ticks();
#endif

    while (1) {
        /* Try to acquire lock */
        if (lock->lock == SPINLOCK_UNLOCKED &&
            __sync_val_compare_and_swap(&lock->lock, SPINLOCK_UNLOCKED, SPINLOCK_LOCKED) == SPINLOCK_UNLOCKED) {
            /* Lock acquired */
#if SPINLOCK_DEBUG
            end_time = get_ticks();
            spin_time = end_time - start_time;

            if (spin_time > 0) {
                contended = true;
                spin_lock(&spinlock_global.lock);
                spinlock_global.total_spin_time += spin_time;
                if (spin_time > spinlock_global.max_spin_time) {
                    spinlock_global.max_spin_time = spin_time;
                }
                spin_unlock(&spinlock_global.lock);
            }
#endif
            lock->owner_cpu = get_cpu_id();
            lock->acquire_time = get_ticks();
            lock->acquire_count++;
            if (contended) {
                lock->contention_count++;
                atomic_inc(&spinlock_global.total_contentions);
            }
            atomic_inc(&spinlock_global.total_locks);
            atomic_inc(&spinlock_global.active_locks);
            return;
        }

        /* Spin with backoff */
        spin_count++;
        if (spin_count < SPINLOCK_SPIN_LIMIT) {
            /* Busy wait with pause instruction */
            for (u32 i = 0; i < (spin_count < 100 ? spin_count : 100); i++) {
                cpu_relax();
            }
        } else {
            /* Yield after too much spinning */
            /* In real implementation: schedule() */
            delay_ms(0);
            spin_count = 0;
        }

        /* Memory barrier to ensure we see updated lock state */
        barrier();
    }
}

/**
 * spin_lock - Acquire a spinlock
 * @lock: Spinlock to acquire
 *
 * Acquires the spinlock, blocking (spinning) until the lock is available.
 * This function disables preemption and should be used for short critical
 * sections only.
 */
void spin_lock(spinlock_t *lock)
{
    if (!lock || lock->magic != SPINLOCK_MAGIC) {
        return;
    }

    /* Fast path: try atomic acquire */
    if (lock->lock == SPINLOCK_UNLOCKED &&
        __sync_val_compare_and_swap(&lock->lock, SPINLOCK_UNLOCKED, SPINLOCK_LOCKED) == SPINLOCK_UNLOCKED) {
        lock->owner_cpu = get_cpu_id();
        lock->acquire_time = get_ticks();
        lock->acquire_count++;
        atomic_inc(&spinlock_global.total_locks);
        atomic_inc(&spinlock_global.active_locks);
        return;
    }

    /* Slow path */
    __spin_lock_slowpath(lock);
}

/**
 * spin_trylock - Try to acquire a spinlock without blocking
 * @lock: Spinlock to acquire
 *
 * Returns: true if lock acquired, false otherwise
 */
bool spin_trylock(spinlock_t *lock)
{
    if (!lock || lock->magic != SPINLOCK_MAGIC) {
        return false;
    }

    if (lock->lock == SPINLOCK_UNLOCKED &&
        __sync_val_compare_and_swap(&lock->lock, SPINLOCK_UNLOCKED, SPINLOCK_LOCKED) == SPINLOCK_UNLOCKED) {
        lock->owner_cpu = get_cpu_id();
        lock->acquire_time = get_ticks();
        lock->acquire_count++;
        atomic_inc(&spinlock_global.total_locks);
        atomic_inc(&spinlock_global.active_locks);
        return true;
    }

    return false;
}

/*===========================================================================*/
/*                         SPINLOCK RELEASE                                  */
/*===========================================================================*/

/**
 * spin_unlock - Release a spinlock
 * @lock: Spinlock to release
 *
 * Releases the spinlock, making it available for other CPUs.
 */
void spin_unlock(spinlock_t *lock)
{
    u64 hold_time;

    if (!lock || lock->magic != SPINLOCK_MAGIC) {
        return;
    }

    /* Calculate hold time */
    if (lock->acquire_time != 0) {
        hold_time = get_ticks() - lock->acquire_time;
        lock->total_hold_time += hold_time;
    }

    /* Reset lock state */
    lock->owner_cpu = 0xFFFFFFFF;
    lock->acquire_time = 0;

    /* Memory barrier before unlock */
    mb();

    /* Release lock */
    lock->lock = SPINLOCK_UNLOCKED;

    atomic_dec(&spinlock_global.active_locks);
}

/*===========================================================================*/
/*                         IRQ-SAFE SPINLOCK OPERATIONS                      */
/*===========================================================================*/

/**
 * spin_lock_irq - Acquire spinlock and disable interrupts
 * @lock: Spinlock to acquire
 *
 * Disables local interrupts and acquires the spinlock.
 * Use when the lock may be taken from interrupt context.
 */
void spin_lock_irq(spinlock_t *lock)
{
    local_irq_disable();
    spin_lock(lock);
}

/**
 * spin_unlock_irq - Release spinlock and enable interrupts
 * @lock: Spinlock to release
 *
 * Releases the spinlock and enables local interrupts.
 */
void spin_unlock_irq(spinlock_t *lock)
{
    spin_unlock(lock);
    local_irq_enable();
}

/**
 * spin_lock_irqsave - Acquire spinlock and save interrupt state
 * @lock: Spinlock to acquire
 * @flags: Storage for interrupt flags
 *
 * Saves the current interrupt state, disables interrupts, and acquires
 * the spinlock.
 */
void spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
    /* In real implementation: save flags and disable interrupts */
    *flags = 0;
    local_irq_disable();
    spin_lock(lock);
}

/**
 * spin_unlock_irqrestore - Release spinlock and restore interrupt state
 * @lock: Spinlock to release
 * @flags: Saved interrupt flags
 *
 * Releases the spinlock and restores the saved interrupt state.
 */
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
    spin_unlock(lock);
    /* In real implementation: restore flags */
    if (flags) {
        local_irq_enable();
    }
}

/*===========================================================================*/
/*                         SPINLOCK STATE QUERIES                            */
/*===========================================================================*/

/**
 * spin_is_locked - Check if spinlock is locked
 * @lock: Spinlock to check
 *
 * Returns: true if locked, false if unlocked
 */
bool spin_is_locked(spinlock_t *lock)
{
    if (!lock || lock->magic != SPINLOCK_MAGIC) {
        return false;
    }

    return lock->lock == SPINLOCK_LOCKED;
}

/**
 * spin_is_contended - Check if spinlock has contention
 * @lock: Spinlock to check
 *
 * Returns: true if there is contention, false otherwise
 */
bool spin_is_contended(spinlock_t *lock)
{
    if (!lock || lock->magic != SPINLOCK_MAGIC) {
        return false;
    }

    return lock->contention_count > 0;
}

/**
 * spin_lock_cpu - Get CPU that owns the lock
 * @lock: Spinlock to query
 *
 * Returns: CPU ID of owner, or 0xFFFFFFFF if unlocked
 */
u32 spin_lock_cpu(spinlock_t *lock)
{
    if (!lock || lock->magic != SPINLOCK_MAGIC) {
        return 0xFFFFFFFF;
    }

    return lock->owner_cpu;
}

/*===========================================================================*/
/*                         SPINLOCK DEBUGGING                                */
/*===========================================================================*/

/**
 * spin_lock_assert - Assert that spinlock is held
 * @lock: Spinlock to check
 *
 * Panics if the lock is not held by the current CPU.
 */
void spin_lock_assert(spinlock_t *lock)
{
    if (!lock || lock->magic != SPINLOCK_MAGIC) {
        kernel_panic("Spinlock: Invalid lock pointer\n");
    }

    if (lock->lock != SPINLOCK_LOCKED) {
        kernel_panic("Spinlock: Lock %p is not held\n", lock);
    }

    if (lock->owner_cpu != get_cpu_id()) {
        kernel_panic("Spinlock: Lock %p held by CPU %u, not %u\n",
                     lock, lock->owner_cpu, get_cpu_id());
    }
}

/**
 * spin_lock_stats - Get spinlock statistics
 * @lock: Spinlock to query
 * @stats: Statistics structure to fill
 */
void spin_lock_stats(spinlock_t *lock, struct spinlock_stats *stats)
{
    if (!lock || lock->magic != SPINLOCK_MAGIC || !stats) {
        return;
    }

    memset(stats, 0, sizeof(struct spinlock_stats));

    stats->total_acquisitions = lock->acquire_count;
    stats->total_contentions = lock->contention_count;
    stats->total_hold_time = lock->total_hold_time;

    if (lock->acquire_count > 0) {
        /* Calculate average hold time */
        /* stats->avg_hold_time = lock->total_hold_time / lock->acquire_count; */
    }
}

/*===========================================================================*/
/*                         SPINLOCK STATISTICS                               */
/*===========================================================================*/

/**
 * spinlock_global_stats - Print global spinlock statistics
 */
void spinlock_global_stats(void)
{
    spin_lock(&spinlock_global.lock);

    printk("\n=== Global Spinlock Statistics ===\n");
    printk("Total Lock Acquisitions: %d\n", atomic_read(&spinlock_global.total_locks));
    printk("Total Contentions: %d\n", atomic_read(&spinlock_global.total_contentions));
    printk("Active Locks: %d\n", atomic_read(&spinlock_global.active_locks));
    printk("Total Spin Time: %llu ticks\n", (unsigned long long)spinlock_global.total_spin_time);
    printk("Max Spin Time: %llu ticks\n", (unsigned long long)spinlock_global.max_spin_time);

    if (atomic_read(&spinlock_global.total_locks) > 0) {
        u32 contention_rate = (atomic_read(&spinlock_global.total_contentions) * 100) /
                              atomic_read(&spinlock_global.total_locks);
        printk("Contention Rate: %u%%\n", contention_rate);
    }

    spin_unlock(&spinlock_global.lock);
}

/**
 * spinlock_dump - Dump all spinlock information
 */
void spinlock_dump(void)
{
    printk("\n=== Spinlock Dump ===\n");
    spinlock_global_stats();
}

/*===========================================================================*/
/*                         SPINLOCK INITIALIZATION                           */
/*===========================================================================*/

/**
 * spinlock_subsystem_init - Initialize spinlock subsystem
 */
void spinlock_subsystem_init(void)
{
    pr_info("Initializing Spinlock Subsystem...\n");

    spin_lock_init(&spinlock_global.lock);
    atomic_set(&spinlock_global.total_locks, 0);
    atomic_set(&spinlock_global.total_contentions, 0);
    atomic_set(&spinlock_global.active_locks, 0);
    spinlock_global.total_spin_time = 0;
    spinlock_global.max_spin_time = 0;

    pr_info("  Spin limit: %u iterations\n", SPINLOCK_SPIN_LIMIT);
    pr_info("  Debug mode: %s\n", SPINLOCK_DEBUG ? "enabled" : "disabled");
    pr_info("Spinlock Subsystem initialized.\n");
}
