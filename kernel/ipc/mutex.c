/*
 * NEXUS OS - Kernel Mutex Implementation
 * kernel/ipc/mutex.c
 */

#include "ipc.h"

/*===========================================================================*/
/*                         MUTEX CONFIGURATION                               */
/*===========================================================================*/

#define MUTEX_MAGIC         0x4D555401
#define MUTEX_MAX_WAITERS   1024
#define MUTEX_SPIN_COUNT    1000

/* Mutex States */
#define MUTEX_STATE_UNLOCKED    0
#define MUTEX_STATE_LOCKED      1
#define MUTEX_STATE_CONTENDED   2

/* Mutex Flags */
#define MUTEX_FLAG_WAITERS      0x00000001
#define MUTEX_FLAG_HANDOFF      0x00000002
#define MUTEX_FLAG_DESTROYING   0x00000004

/*===========================================================================*/
/*                         MUTEX GLOBAL DATA                                 */
/*===========================================================================*/

static struct {
    spinlock_t lock;              /* Global mutex lock */
    atomic_t mutex_count;         /* Active mutexes */
    atomic_t total_created;       /* Total created */
    atomic_t total_destroyed;     /* Total destroyed */
    atomic_t total_acquisitions;  /* Total lock acquisitions */
    atomic_t total_contentions;   /* Total contentions */
    struct list_head mutex_list;  /* List of all mutexes */
} mutex_global = {
    .lock = { .lock = 0 },
    .mutex_count = ATOMIC_INIT(0),
    .total_created = ATOMIC_INIT(0),
    .total_destroyed = ATOMIC_INIT(0),
    .total_acquisitions = ATOMIC_INIT(0),
    .total_contentions = ATOMIC_INIT(0),
};

/*===========================================================================*/
/*                         MUTEX WAITER                                      */
/*===========================================================================*/

/**
 * mutex_waiter - Mutex wait queue entry
 */
struct mutex_waiter {
    struct list_head list;      /* Wait list entry */
    struct task_struct *task;   /* Waiting task */
    u32 state;                  /* Waiter state */
    u64 wait_time;              /* Time started waiting */
    u64 wakeup_time;            /* Time woken up */
    bool woken;                 /* Has been woken */
};

/*===========================================================================*/
/*                         MUTEX INITIALIZATION                              */
/*===========================================================================*/

/**
 * mutex_init - Initialize a mutex
 * @mutex: Mutex to initialize
 *
 * Initializes the mutex to unlocked state.
 */
void mutex_init(mutex_t *mutex)
{
    if (!mutex) {
        return;
    }

    atomic_set(&mutex->count, 1); /* Unlocked */
    spin_lock_init(&mutex->wait_lock);
    INIT_LIST_HEAD(&mutex->wait_list);

    mutex->magic = MUTEX_MAGIC;
    mutex->flags = 0;
    mutex->owner = NULL;
    mutex->spin_count = MUTEX_SPIN_COUNT;

    /* Update global statistics */
    spin_lock(&mutex_global.lock);
    atomic_inc(&mutex_global.mutex_count);
    atomic_inc(&mutex_global.total_created);
    list_add(&mutex->global_list, &mutex_global.mutex_list);
    spin_unlock(&mutex_global.lock);

    pr_debug("Mutex: Initialized mutex %p\n", mutex);
}

/**
 * mutex_init_locked - Initialize a mutex in locked state
 * @mutex: Mutex to initialize
 */
void mutex_init_locked(mutex_t *mutex)
{
    if (!mutex) {
        return;
    }

    atomic_set(&mutex->count, 0); /* Locked */
    spin_lock_init(&mutex->wait_lock);
    INIT_LIST_HEAD(&mutex->wait_list);

    mutex->magic = MUTEX_MAGIC;
    mutex->flags = 0;
    mutex->owner = current;
    mutex->spin_count = MUTEX_SPIN_COUNT;

    /* Update global statistics */
    spin_lock(&mutex_global.lock);
    atomic_inc(&mutex_global.mutex_count);
    atomic_inc(&mutex_global.total_created);
    list_add(&mutex->global_list, &mutex_global.mutex_list);
    spin_unlock(&mutex_global.lock);

    pr_debug("Mutex: Initialized locked mutex %p\n", mutex);
}

/**
 * mutex_destroy - Destroy a mutex
 * @mutex: Mutex to destroy
 */
void mutex_destroy(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return;
    }

    pr_debug("Mutex: Destroying mutex %p\n", mutex);

    /* Check if mutex is locked */
    if (atomic_read(&mutex->count) == 0) {
        pr_err("Mutex: Attempt to destroy locked mutex %p\n", mutex);
        return;
    }

    /* Mark as destroying */
    spin_lock(&mutex->wait_lock);
    mutex->flags |= MUTEX_FLAG_DESTROYING;
    spin_unlock(&mutex->wait_lock);

    /* Wake up all waiters */
    /* In real implementation: wake_up_all(&mutex->wait_list) */

    /* Update global statistics */
    spin_lock(&mutex_global.lock);
    atomic_dec(&mutex_global.mutex_count);
    atomic_inc(&mutex_global.total_destroyed);
    list_del(&mutex->global_list);
    spin_unlock(&mutex_global.lock);

    mutex->magic = 0;
}

/*===========================================================================*/
/*                         MUTEX LOCKING                                     */
/*===========================================================================*/

/**
 * mutex_spin_lock - Spin trying to acquire mutex
 * @mutex: Mutex to lock
 *
 * Returns: true if lock acquired, false if should sleep
 */
static bool mutex_spin_lock(mutex_t *mutex)
{
    u32 i;

    for (i = 0; i < mutex->spin_count; i++) {
        if (atomic_cmpxchg(&mutex->count, 1, 0) == 1) {
            return true;
        }

        /* Pause instruction for better hyperthreading */
        /* In real implementation: cpu_relax() */
        barrier();
    }

    return false;
}

/**
 * mutex_lock_slowpath - Slow path for mutex lock
 * @mutex: Mutex to lock
 */
static void mutex_lock_slowpath(mutex_t *mutex)
{
    struct mutex_waiter waiter;
    struct task_struct *task;

    task = current;

    /* Create waiter entry */
    waiter.task = task;
    waiter.state = 0;
    waiter.wait_time = get_time_ms();
    waiter.wakeup_time = 0;
    waiter.woken = false;
    INIT_LIST_HEAD(&waiter.list);

    /* Add to wait list */
    spin_lock(&mutex->wait_lock);
    mutex->flags |= MUTEX_FLAG_WAITERS;
    list_add_tail(&waiter.list, &mutex->wait_list);
    spin_unlock(&mutex->wait_lock);

    /* Update statistics */
    atomic_inc(&mutex_global.total_contentions);

    /* Wait for lock */
    while (1) {
        /* Try to acquire */
        if (atomic_cmpxchg(&mutex->count, 1, 0) == 1) {
            break;
        }

        /* Check if destroying */
        if (mutex->flags & MUTEX_FLAG_DESTROYING) {
            break;
        }

        /* Check for signals */
        if (signal_pending_current()) {
            /* Remove from wait list */
            spin_lock(&mutex->wait_lock);
            list_del(&waiter.list);
            if (list_empty(&mutex->wait_list)) {
                mutex->flags &= ~MUTEX_FLAG_WAITERS;
            }
            spin_unlock(&mutex->wait_lock);
            return;
        }

        /* Sleep */
        /* In real implementation: schedule() */
        delay_ms(1);
    }

    /* Remove from wait list */
    spin_lock(&mutex->wait_lock);
    list_del(&waiter.list);
    if (list_empty(&mutex->wait_list)) {
        mutex->flags &= ~MUTEX_FLAG_WAITERS;
    }
    spin_unlock(&mutex->wait_lock);

    waiter.wakeup_time = get_time_ms();

    /* Set owner */
    mutex->owner = task;

    pr_debug("Mutex: Acquired mutex %p after %llu ms wait\n",
             mutex, waiter.wakeup_time - waiter.wait_time);
}

/**
 * mutex_lock - Acquire a mutex
 * @mutex: Mutex to lock
 *
 * Blocks until the mutex is acquired.
 */
void mutex_lock(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return;
    }

    /* Fast path: try atomic acquire */
    if (atomic_cmpxchg(&mutex->count, 1, 0) == 1) {
        mutex->owner = current;
        atomic_inc(&mutex_global.total_acquisitions);
        return;
    }

    /* Check for recursive lock */
    if (mutex->owner == current) {
        pr_err("Mutex: Recursive lock attempt on mutex %p\n", mutex);
        return;
    }

    /* Slow path */
    mutex_lock_slowpath(mutex);
    atomic_inc(&mutex_global.total_acquisitions);
}

/**
 * mutex_lock_interruptible - Acquire mutex (interruptible)
 * @mutex: Mutex to lock
 *
 * Returns: 0 on success, -EINTR if interrupted
 */
int mutex_lock_interruptible(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return -EINVAL;
    }

    /* Fast path */
    if (atomic_cmpxchg(&mutex->count, 1, 0) == 1) {
        mutex->owner = current;
        atomic_inc(&mutex_global.total_acquisitions);
        return 0;
    }

    /* Check for recursive lock */
    if (mutex->owner == current) {
        return -EDEADLK;
    }

    /* Try spinning first */
    if (mutex_spin_lock(mutex)) {
        mutex->owner = current;
        atomic_inc(&mutex_global.total_acquisitions);
        return 0;
    }

    /* Slow path with interrupt check */
    /* In real implementation: use wait_event_interruptible */
    mutex_lock_slowpath(mutex);
    atomic_inc(&mutex_global.total_acquisitions);

    return 0;
}

/**
 * mutex_trylock - Try to acquire mutex without blocking
 * @mutex: Mutex to lock
 *
 * Returns: true if lock acquired, false otherwise
 */
bool mutex_trylock(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return false;
    }

    if (atomic_cmpxchg(&mutex->count, 1, 0) == 1) {
        mutex->owner = current;
        atomic_inc(&mutex_global.total_acquisitions);
        return true;
    }

    return false;
}

/*===========================================================================*/
/*                         MUTEX UNLOCKING                                   */
/*===========================================================================*/

/**
 * mutex_unlock_slowpath - Slow path for mutex unlock
 * @mutex: Mutex to unlock
 */
static void mutex_unlock_slowpath(mutex_t *mutex)
{
    struct mutex_waiter *waiter;

    spin_lock(&mutex->wait_lock);

    /* Check for waiters */
    if (!list_empty(&mutex->wait_list)) {
        /* Wake up first waiter */
        waiter = list_first_entry(&mutex->wait_list, struct mutex_waiter, list);
        if (waiter) {
            waiter->woken = true;
            /* In real implementation: wake_up_process(waiter->task) */
        }
    }

    mutex->owner = NULL;

    spin_unlock(&mutex->wait_lock);
}

/**
 * mutex_unlock - Release a mutex
 * @mutex: Mutex to unlock
 */
void mutex_unlock(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return;
    }

    /* Check ownership */
    if (mutex->owner != current) {
        pr_err("Mutex: Unlock by non-owner on mutex %p\n", mutex);
        return;
    }

    /* Memory barrier before unlock */
    mb();

    /* Try fast path */
    if (atomic_read(&mutex->count) == 0 &&
        atomic_cmpxchg(&mutex->count, 0, 1) == 0) {
        mutex->owner = NULL;
        return;
    }

    /* Slow path with waiters */
    mutex_unlock_slowpath(mutex);
}

/*===========================================================================*/
/*                         RECURSIVE MUTEX                                   */
/*===========================================================================*/

/**
 * mutex_init_recursive - Initialize a recursive mutex
 * @mutex: Mutex to initialize
 */
void mutex_init_recursive(mutex_t *mutex)
{
    if (!mutex) {
        return;
    }

    atomic_set(&mutex->count, 1);
    spin_lock_init(&mutex->wait_lock);
    INIT_LIST_HEAD(&mutex->wait_list);

    mutex->magic = MUTEX_MAGIC;
    mutex->flags = 0;
    mutex->owner = NULL;
    mutex->recursion_count = 0;
    mutex->spin_count = MUTEX_SPIN_COUNT;

    /* Update global statistics */
    spin_lock(&mutex_global.lock);
    atomic_inc(&mutex_global.mutex_count);
    atomic_inc(&mutex_global.total_created);
    list_add(&mutex->global_list, &mutex_global.mutex_list);
    spin_unlock(&mutex_global.lock);

    pr_debug("Mutex: Initialized recursive mutex %p\n", mutex);
}

/**
 * mutex_lock_recursive - Acquire recursive mutex
 * @mutex: Mutex to lock
 */
void mutex_lock_recursive(mutex_t *mutex)
{
    struct task_struct *curr_task;

    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return;
    }

    curr_task = current;

    /* Check if we already own the mutex */
    if (mutex->owner == curr_task) {
        mutex->recursion_count++;
        return;
    }

    /* Lock normally */
    mutex_lock(mutex);
    mutex->recursion_count = 1;
}

/**
 * mutex_unlock_recursive - Release recursive mutex
 * @mutex: Mutex to unlock
 */
void mutex_unlock_recursive(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return;
    }

    /* Check ownership */
    if (mutex->owner != current) {
        pr_err("Mutex: Recursive unlock by non-owner on mutex %p\n", mutex);
        return;
    }

    /* Decrement recursion count */
    if (mutex->recursion_count > 1) {
        mutex->recursion_count--;
        return;
    }

    /* Fully unlock */
    mutex->recursion_count = 0;
    mutex_unlock(mutex);
}

/*===========================================================================*/
/*                         ADAPTIVE MUTEX                                    */
/*===========================================================================*/

/**
 * mutex_init_adaptive - Initialize an adaptive mutex
 * @mutex: Mutex to initialize
 * @spin_count: Number of spin iterations before sleeping
 */
void mutex_init_adaptive(mutex_t *mutex, u32 spin_count)
{
    mutex_init(mutex);
    mutex->spin_count = spin_count;
}

/**
 * mutex_lock_adaptive - Acquire adaptive mutex
 * @mutex: Mutex to lock
 *
 * Spins for a configurable number of iterations before sleeping.
 */
void mutex_lock_adaptive(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return;
    }

    /* Fast path */
    if (atomic_cmpxchg(&mutex->count, 1, 0) == 1) {
        mutex->owner = current;
        atomic_inc(&mutex_global.total_acquisitions);
        return;
    }

    /* Check for recursive lock */
    if (mutex->owner == current) {
        pr_err("Mutex: Recursive lock attempt on adaptive mutex %p\n", mutex);
        return;
    }

    /* Adaptive spinning */
    if (mutex_spin_lock(mutex)) {
        mutex->owner = current;
        atomic_inc(&mutex_global.total_acquisitions);
        return;
    }

    /* Sleep */
    mutex_lock_slowpath(mutex);
    atomic_inc(&mutex_global.total_acquisitions);
}

/*===========================================================================*/
/*                         MUTEX DEBUGGING                                   */
/*===========================================================================*/

/**
 * mutex_is_locked - Check if mutex is locked
 * @mutex: Mutex to check
 *
 * Returns: true if locked, false if unlocked
 */
bool mutex_is_locked(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return false;
    }

    return atomic_read(&mutex->count) == 0;
}

/**
 * mutex_owner - Get mutex owner
 * @mutex: Mutex to query
 *
 * Returns: Pointer to owner thread or NULL
 */
struct task_struct *mutex_owner(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return NULL;
    }

    return mutex->owner;
}

/**
 * mutex_waiters - Get number of waiters
 * @mutex: Mutex to query
 *
 * Returns: Number of threads waiting
 */
u32 mutex_waiters(mutex_t *mutex)
{
    u32 count;

    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return 0;
    }

    spin_lock(&mutex->wait_lock);
    count = 0;
    /* In real implementation: count list entries */
    spin_unlock(&mutex->wait_lock);

    return count;
}

/*===========================================================================*/
/*                         MUTEX STATISTICS                                  */
/*===========================================================================*/

/**
 * mutex_stats - Print mutex statistics
 * @mutex: Mutex to report on
 */
void mutex_stats(mutex_t *mutex)
{
    if (!mutex || mutex->magic != MUTEX_MAGIC) {
        return;
    }

    printk("\n=== Mutex Statistics ===\n");
    printk("Mutex Address: %p\n", mutex);
    printk("State: %s\n", mutex_is_locked(mutex) ? "LOCKED" : "UNLOCKED");
    printk("Owner: %p\n", mutex->owner);
    printk("Recursion Count: %u\n", mutex->recursion_count);
    printk("Spin Count: %u\n", mutex->spin_count);
    printk("Flags: 0x%x\n", mutex->flags);
}

/**
 * mutex_global_stats - Print global mutex statistics
 */
static void mutex_global_stats(void)
{
    spin_lock(&mutex_global.lock);

    printk("\n=== Global Mutex Statistics ===\n");
    printk("Active Mutexes: %d\n", atomic_read(&mutex_global.mutex_count));
    printk("Total Created: %d\n", atomic_read(&mutex_global.total_created));
    printk("Total Destroyed: %d\n", atomic_read(&mutex_global.total_destroyed));
    printk("Total Acquisitions: %d\n", atomic_read(&mutex_global.total_acquisitions));
    printk("Total Contentions: %d\n", atomic_read(&mutex_global.total_contentions));

    spin_unlock(&mutex_global.lock);
}

/*===========================================================================*/
/*                         MUTEX INITIALIZATION                              */
/*===========================================================================*/

/**
 * mutex_init_subsystem - Initialize mutex subsystem
 */
void mutex_init_subsystem(void)
{
    pr_info("Initializing Mutex Subsystem...\n");

    spin_lock_init(&mutex_global.lock);
    INIT_LIST_HEAD(&mutex_global.mutex_list);

    atomic_set(&mutex_global.mutex_count, 0);
    atomic_set(&mutex_global.total_created, 0);
    atomic_set(&mutex_global.total_destroyed, 0);
    atomic_set(&mutex_global.total_acquisitions, 0);
    atomic_set(&mutex_global.total_contentions, 0);

    pr_info("  Default spin count: %u\n", MUTEX_SPIN_COUNT);
    pr_info("  Maximum waiters: %d\n", MUTEX_MAX_WAITERS);
    pr_info("Mutex Subsystem initialized.\n");
}
