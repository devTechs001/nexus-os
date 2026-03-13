/*
 * NEXUS OS - Wait Queue Implementation
 * kernel/sync/waitqueue.c
 */

#include "sync.h"

/*===========================================================================*/
/*                         WAIT QUEUE CONFIGURATION                          */
/*===========================================================================*/

#define WAITQUEUE_MAGIC_VALUE   0x57510001  /* "WQ" magic */
#define WAITQUEUE_DEBUG         1

/* Wait states */
#define WAIT_STATE_RUNNING      0
#define WAIT_STATE_SLEEPING     1
#define WAIT_STATE_WOKEN        2
#define WAIT_STATE_TIMEOUT      3

/*===========================================================================*/
/*                         WAIT QUEUE GLOBAL DATA                            */
/*===========================================================================*/

static struct {
    spinlock_t lock;              /* Global statistics lock */
    atomic_t total_waitqueues;    /* Total wait queues created */
    atomic_t total_waiters;       /* Total waiters added */
    atomic_t total_wakeups;       /* Total wakeups */
    atomic_t active_waiters;      /* Currently waiting */
    u64 total_wait_time;          /* Total time spent waiting */
    u64 max_wait_time;            /* Maximum wait time */
} waitqueue_global = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .total_waitqueues = ATOMIC_INIT(0),
    .total_waiters = ATOMIC_INIT(0),
    .total_wakeups = ATOMIC_INIT(0),
    .active_waiters = ATOMIC_INIT(0),
    .total_wait_time = 0,
    .max_wait_time = 0,
};

/*===========================================================================*/
/*                         WAIT QUEUE INITIALIZATION                         */
/*===========================================================================*/

/**
 * wait_queue_init - Initialize a wait queue head (alias)
 * @wq: Wait queue to initialize
 */
void wait_queue_init(wait_queue_head_t *wq)
{
    init_waitqueue_head(wq);
}

/**
 * init_waitqueue_head - Initialize a wait queue head
 * @wq: Wait queue to initialize
 *
 * Initializes the wait queue to empty state.
 */
void init_waitqueue_head(wait_queue_head_t *wq)
{
    if (!wq) {
        return;
    }

    wq->magic = WAITQUEUE_MAGIC_VALUE;
    spin_lock_init(&wq->lock);
    INIT_LIST_HEAD(&wq->head);
    wq->waiters = 0;
    wq->wakeups = 0;
    wq->name = "unnamed";

    atomic_inc(&waitqueue_global.total_waitqueues);

    pr_debug("WaitQueue: Initialized queue %p\n", wq);
}

/**
 * init_waitqueue_head_named - Initialize a wait queue with a name
 * @wq: Wait queue to initialize
 * @name: Queue name for debugging
 */
void init_waitqueue_head_named(wait_queue_head_t *wq, const char *name)
{
    if (!wq) {
        return;
    }

    wq->magic = WAITQUEUE_MAGIC_VALUE;
    spin_lock_init(&wq->lock);
    INIT_LIST_HEAD(&wq->head);
    wq->waiters = 0;
    wq->wakeups = 0;
    wq->name = name ? name : "unnamed";

    atomic_inc(&waitqueue_global.total_waitqueues);

    pr_debug("WaitQueue: Initialized queue '%s' at %p\n", name, wq);
}

/**
 * init_wait - Initialize a wait queue entry
 * @wait: Wait entry to initialize
 */
void init_wait(wait_queue_entry_t *wait)
{
    if (!wait) {
        return;
    }

    INIT_LIST_HEAD(&wait->list);
    wait->task = NULL;
    wait->flags = 0;
    wait->private = NULL;
    wait->func = NULL;
}

/**
 * init_wait_entry - Initialize a wait queue entry with task
 * @wait: Wait entry to initialize
 * @task: Task to associate with entry
 */
void init_wait_entry(wait_queue_entry_t *wait, struct task_struct *task)
{
    if (!wait) {
        return;
    }

    INIT_LIST_HEAD(&wait->list);
    wait->task = task;
    wait->flags = 0;
    wait->private = NULL;
    wait->func = NULL;
}

/*===========================================================================*/
/*                         WAITER MANAGEMENT                                 */
/*===========================================================================*/

/**
 * add_wait_queue - Add a waiter to wait queue
 * @wq: Wait queue
 * @wait: Wait entry to add
 *
 * Adds the waiter to the wait queue. The caller must hold the
 * wait queue lock or use appropriate synchronization.
 */
void add_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wait)
{
    if (!wq || !wait || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return;
    }

    spin_lock(&wq->lock);

    list_add_tail(&wait->list, &wq->head);
    wq->waiters++;

    spin_unlock(&wq->lock);

    atomic_inc(&waitqueue_global.total_waiters);
    atomic_inc(&waitqueue_global.active_waiters);

    pr_debug("WaitQueue: Added waiter %p to queue %p (%s)\n",
             wait, wq, wq->name);
}

/**
 * remove_wait_queue - Remove a waiter from wait queue
 * @wq: Wait queue
 * @wait: Wait entry to remove
 */
void remove_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wait)
{
    if (!wq || !wait || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return;
    }

    spin_lock(&wq->lock);

    if (!list_empty(&wait->list)) {
        list_del_init(&wait->list);
        if (wq->waiters > 0) {
            wq->waiters--;
        }
    }

    spin_unlock(&wq->lock);

    atomic_dec(&waitqueue_global.active_waiters);

    pr_debug("WaitQueue: Removed waiter %p from queue %p\n", wait, wq);
}

/**
 * prepare_to_wait - Prepare to wait on a wait queue
 * @wq: Wait queue
 * @wait: Wait entry
 * @state: Task state (TASK_INTERRUPTIBLE, etc.)
 *
 * Sets up the wait entry and adds it to the queue.
 */
void prepare_to_wait(wait_queue_head_t *wq, wait_queue_entry_t *wait, int state)
{
    if (!wq || !wait || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return;
    }

    wait->flags = state;
    add_wait_queue(wq, wait);

    /* In real implementation: set_task_state(current, state) */
}

/**
 * finish_wait - Finish waiting on a wait queue
 * @wq: Wait queue
 * @wait: Wait entry
 *
 * Removes the wait entry from the queue and cleans up.
 */
void finish_wait(wait_queue_head_t *wq, wait_queue_entry_t *wait)
{
    if (!wq || !wait || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return;
    }

    /* In real implementation: set_task_state(current, TASK_RUNNING) */

    remove_wait_queue(wq, wait);
}

/*===========================================================================*/
/*                         WAIT OPERATIONS                                   */
/*===========================================================================*/

/**
 * __wait_event_common - Common wait implementation
 * @wq: Wait queue
 * @condition: Condition to wait for
 * @timeout_ms: Timeout in milliseconds (0 for infinite)
 * @interruptible: Whether wait is interruptible
 *
 * Returns: 0 if condition met, -ETIMEDOUT on timeout, -EINTR if interrupted
 */
static int __wait_event_common(wait_queue_head_t *wq, int condition,
                                u64 timeout_ms, bool interruptible)
{
    wait_queue_entry_t wait;
    u64 start_time;
    u64 elapsed;
    int ret = 0;

    if (!wq || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return -EINVAL;
    }

    init_wait(&wait);
    start_time = get_time_ms();

    while (!condition) {
        prepare_to_wait(wq, &wait, interruptible ? 1 : 0);

        /* Check condition again after adding to queue */
        if (condition) {
            finish_wait(wq, &wait);
            break;
        }

        /* Wait with optional timeout */
        if (timeout_ms > 0) {
            elapsed = get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                finish_wait(wq, &wait);
                ret = -ETIMEDOUT;
                break;
            }

            /* Sleep for remaining time or small interval */
            delay_ms(1);
        } else {
            /* Infinite wait */
            delay_ms(1);
        }

        /* Check for signals if interruptible */
        if (interruptible && signal_pending_current()) {
            finish_wait(wq, &wait);
            ret = -EINTR;
            break;
        }
    }

    /* Record wait time */
    if (ret == 0) {
        elapsed = get_time_ms() - start_time;
        spin_lock(&waitqueue_global.lock);
        waitqueue_global.total_wait_time += elapsed;
        if (elapsed > waitqueue_global.max_wait_time) {
            waitqueue_global.max_wait_time = elapsed;
        }
        spin_unlock(&waitqueue_global.lock);
    }

    return ret;
}

/**
 * wait_event - Wait on a wait queue (uninterruptible)
 * @wq: Wait queue
 * @condition: Condition to wait for
 *
 * Blocks until the condition becomes true. Cannot be interrupted
 * by signals.
 */
void wait_event(wait_queue_head_t *wq, int condition)
{
    __wait_event_common(wq, condition, 0, false);
}

/**
 * wait_event_interruptible - Wait on a wait queue (interruptible)
 * @wq: Wait queue
 * @condition: Condition to wait for
 *
 * Blocks until the condition becomes true or a signal is received.
 *
 * Returns: 0 if condition met, -EINTR if interrupted
 */
int wait_event_interruptible(wait_queue_head_t *wq, int condition)
{
    return __wait_event_common(wq, condition, 0, true);
}

/**
 * wait_event_timeout - Wait on a wait queue with timeout
 * @wq: Wait queue
 * @condition: Condition to wait for
 * @timeout_ms: Timeout in milliseconds
 *
 * Blocks until the condition becomes true or timeout expires.
 *
 * Returns: 0 if condition met, -ETIMEDOUT on timeout
 */
int wait_event_timeout(wait_queue_head_t *wq, int condition, u64 timeout_ms)
{
    return __wait_event_common(wq, condition, timeout_ms, false);
}

/**
 * wait_event_lock_irq - Wait on a wait queue with lock
 * @wq: Wait queue
 * @condition: Condition to wait for
 * @lock: Spinlock to release while waiting
 *
 * Releases the lock, waits for condition, then reacquires the lock.
 */
void wait_event_lock_irq(wait_queue_head_t *wq, int condition, spinlock_t *lock)
{
    wait_queue_entry_t wait;

    if (!wq || wq->magic != WAITQUEUE_MAGIC_VALUE || !lock) {
        return;
    }

    init_wait(&wait);

    while (!condition) {
        prepare_to_wait(wq, &wait, 0);
        spin_unlock_irq(lock);

        /* Wait */
        delay_ms(1);

        spin_lock_irq(lock);
        finish_wait(wq, &wait);
    }
}

/*===========================================================================*/
/*                         WAKE OPERATIONS                                   */
/*===========================================================================*/

/**
 * __wake_up_common - Common wake up implementation
 * @wq: Wait queue
 * @nr: Number of waiters to wake (0 for all)
 * @exclusive: Whether to wake exclusively
 */
static void __wake_up_common(wait_queue_head_t *wq, u32 nr, bool exclusive)
{
    wait_queue_entry_t *wait;
    wait_queue_entry_t *next;
    u32 woken = 0;

    if (!wq || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return;
    }

    spin_lock(&wq->lock);

    list_for_each_entry_safe(wait, next, &wq->head, list) {
        /* Wake up the waiter */
        wait->flags |= WAITQUEUE_WOKEN;

        /* In real implementation: wake_up_process(wait->task) */

        woken++;

        /* Remove from list if not exclusive wait */
        if (!exclusive) {
            list_del_init(&wait->list);
            if (wq->waiters > 0) {
                wq->waiters--;
            }
        }

        /* Check if we've woken enough waiters */
        if (nr > 0 && woken >= nr) {
            break;
        }

        /* For exclusive wake, only wake one */
        if (exclusive) {
            break;
        }
    }

    wq->wakeups += woken;

    spin_unlock(&wq->lock);

    atomic_add(woken, &waitqueue_global.total_wakeups);
    atomic_sub(woken, &waitqueue_global.active_waiters);

    pr_debug("WaitQueue: Woke up %u waiters on queue %p (%s)\n",
             woken, wq, wq->name);
}

/**
 * wake_up - Wake up waiters on a wait queue
 * @wq: Wait queue
 *
 * Wakes up all waiters on the queue.
 */
void wake_up(wait_queue_head_t *wq)
{
    __wake_up_common(wq, 0, false);
}

/**
 * wake_up_interruptible - Wake up interruptible waiters
 * @wq: Wait queue
 *
 * Wakes up all interruptible waiters on the queue.
 */
void wake_up_interruptible(wait_queue_head_t *wq)
{
    __wake_up_common(wq, 0, false);
}

/**
 * wake_up_all - Wake up all waiters
 * @wq: Wait queue
 *
 * Explicitly wakes up all waiters (same as wake_up).
 */
void wake_up_all(wait_queue_head_t *wq)
{
    __wake_up_common(wq, 0, false);
}

/**
 * wake_up_nr - Wake up specific number of waiters
 * @wq: Wait queue
 * @nr: Number of waiters to wake
 */
void wake_up_nr(wait_queue_head_t *wq, u32 nr)
{
    __wake_up_common(wq, nr, false);
}

/**
 * wake_up_interruptible_sync - Wake up with memory synchronization
 * @wq: Wait queue
 *
 * Wakes up waiters with proper memory ordering.
 */
void wake_up_interruptible_sync(wait_queue_head_t *wq)
{
    mb(); /* Memory barrier before wake */
    __wake_up_common(wq, 0, false);
}

/**
 * wake_up_interruptible_sync_poll - Poll-based wake up
 * @wq: Wait queue
 * @poll_bits: Poll event bits
 *
 * Returns: Poll events
 */
u32 wake_up_interruptible_sync_poll(wait_queue_head_t *wq, u32 poll_bits)
{
    mb();
    __wake_up_common(wq, 0, false);
    return poll_bits;
}

/*===========================================================================*/
/*                         QUERY OPERATIONS                                  */
/*===========================================================================*/

/**
 * waitqueue_active - Check if wait queue has waiters
 * @wq: Wait queue
 *
 * Returns: true if there are waiters, false otherwise
 */
bool waitqueue_active(wait_queue_head_t *wq)
{
    bool active;

    if (!wq || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return false;
    }

    spin_lock(&wq->lock);
    active = !list_empty(&wq->head);
    spin_unlock(&wq->lock);

    return active;
}

/**
 * waitqueue_count - Get number of waiters
 * @wq: Wait queue
 *
 * Returns: Number of waiters on the queue
 */
u32 waitqueue_count(wait_queue_head_t *wq)
{
    u32 count;

    if (!wq || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return 0;
    }

    spin_lock(&wq->lock);
    count = wq->waiters;
    spin_unlock(&wq->lock);

    return count;
}

/**
 * waitqueue_wakeups - Get number of wakeups
 * @wq: Wait queue
 *
 * Returns: Total number of wakeups on this queue
 */
u32 waitqueue_wakeups(wait_queue_head_t *wq)
{
    u32 wakeups;

    if (!wq || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return 0;
    }

    spin_lock(&wq->lock);
    wakeups = wq->wakeups;
    spin_unlock(&wq->lock);

    return wakeups;
}

/*===========================================================================*/
/*                         WAIT QUEUE STATISTICS                             */
/*===========================================================================*/

/**
 * waitqueue_stats - Print wait queue statistics
 * @wq: Wait queue
 */
void waitqueue_stats(wait_queue_head_t *wq)
{
    if (!wq || wq->magic != WAITQUEUE_MAGIC_VALUE) {
        return;
    }

    printk("\n=== Wait Queue Statistics ===\n");
    printk("Queue Address: %p\n", wq);
    printk("Queue Name: %s\n", wq->name);
    printk("Current Waiters: %u\n", wq->waiters);
    printk("Total Wakeups: %u\n", wq->wakeups);
}

/**
 * waitqueue_global_stats - Print global wait queue statistics
 */
void waitqueue_global_stats(void)
{
    spin_lock(&waitqueue_global.lock);

    printk("\n=== Global Wait Queue Statistics ===\n");
    printk("Total Wait Queues: %d\n", atomic_read(&waitqueue_global.total_waitqueues));
    printk("Total Waiters Added: %d\n", atomic_read(&waitqueue_global.total_waiters));
    printk("Total Wakeups: %d\n", atomic_read(&waitqueue_global.total_wakeups));
    printk("Active Waiters: %d\n", atomic_read(&waitqueue_global.active_waiters));
    printk("Total Wait Time: %llu ms\n",
           (unsigned long long)waitqueue_global.total_wait_time);
    printk("Max Wait Time: %llu ms\n",
           (unsigned long long)waitqueue_global.max_wait_time);

    spin_unlock(&waitqueue_global.lock);
}

/**
 * waitqueue_dump - Dump all wait queue information
 */
void waitqueue_dump(void)
{
    printk("\n=== Wait Queue Dump ===\n");
    waitqueue_global_stats();
}

/*===========================================================================*/
/*                         WAIT QUEUE INITIALIZATION                         */
/*===========================================================================*/

/**
 * waitqueue_subsystem_init - Initialize wait queue subsystem
 */
void waitqueue_subsystem_init(void)
{
    pr_info("Initializing Wait Queue Subsystem...\n");

    spin_lock_init(&waitqueue_global.lock);
    atomic_set(&waitqueue_global.total_waitqueues, 0);
    atomic_set(&waitqueue_global.total_waiters, 0);
    atomic_set(&waitqueue_global.total_wakeups, 0);
    atomic_set(&waitqueue_global.active_waiters, 0);
    waitqueue_global.total_wait_time = 0;
    waitqueue_global.max_wait_time = 0;

    pr_info("  Debug mode: %s\n", WAITQUEUE_DEBUG ? "enabled" : "disabled");
    pr_info("Wait Queue Subsystem initialized.\n");
}
