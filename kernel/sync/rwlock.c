/*
 * NEXUS OS - Read-Write Lock Implementation
 * kernel/sync/rwlock.c
 */

#include "sync.h"

/*===========================================================================*/
/*                         RWLOCK CONFIGURATION                              */
/*===========================================================================*/

#define RWLOCK_SPIN_LIMIT       10000
#define RWLOCK_DEBUG            1

/* Lock state bit definitions */
#define RWLOCK_WRITE_BIT        0x80000000
#define RWLOCK_READ_MASK        0x7FFFFFFF
#define RWLOCK_WAITING_WRITERS  0x40000000

/* Architecture-specific pause instruction */
#define cpu_relax()             __asm__ __volatile__("pause" ::: "memory")

/*===========================================================================*/
/*                         RWLOCK GLOBAL DATA                                */
/*===========================================================================*/

static struct {
    spinlock_t lock;              /* Global statistics lock */
    atomic_t total_read_locks;    /* Total read lock acquisitions */
    atomic_t total_write_locks;   /* Total write lock acquisitions */
    atomic_t total_read_contentions; /* Total read contentions */
    atomic_t total_write_contentions; /* Total write contentions */
    atomic_t active_readers;      /* Currently active readers */
    atomic_t active_writers;      /* Currently active writers */
    u64 total_read_hold_time;     /* Total read hold time */
    u64 total_write_hold_time;    /* Total write hold time */
    u64 max_read_hold_time;       /* Maximum read hold time */
    u64 max_write_hold_time;      /* Maximum write hold time */
} rwlock_global = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .total_read_locks = ATOMIC_INIT(0),
    .total_write_locks = ATOMIC_INIT(0),
    .total_read_contentions = ATOMIC_INIT(0),
    .total_write_contentions = ATOMIC_INIT(0),
    .active_readers = ATOMIC_INIT(0),
    .active_writers = ATOMIC_INIT(0),
    .total_read_hold_time = 0,
    .total_write_hold_time = 0,
    .max_read_hold_time = 0,
    .max_write_hold_time = 0,
};

/*===========================================================================*/
/*                         RWLOCK INITIALIZATION                             */
/*===========================================================================*/

/**
 * rwlock_init - Initialize a read-write lock
 * @lock: Read-write lock to initialize
 *
 * Initializes the rwlock to unlocked state with default values.
 */
void rwlock_init(rwlock_t *lock)
{
    if (!lock) {
        return;
    }

    lock->lock = RWLOCK_UNLOCKED;
    spin_lock_init(&lock->write_lock);
    lock->magic = RWLOCK_MAGIC;
    lock->name = "unnamed";
    lock->readers = 0;
    lock->writer_cpu = 0xFFFFFFFF;
    lock->max_readers = 0;
    lock->read_count = 0;
    lock->write_count = 0;
    lock->read_contentions = 0;
    lock->write_contentions = 0;

    pr_debug("RWLock: Initialized lock %p\n", lock);
}

/**
 * rwlock_init_named - Initialize a read-write lock with a name
 * @lock: Read-write lock to initialize
 * @name: Lock name for debugging
 */
void rwlock_init_named(rwlock_t *lock, const char *name)
{
    if (!lock) {
        return;
    }

    lock->lock = RWLOCK_UNLOCKED;
    spin_lock_init(&lock->write_lock);
    lock->magic = RWLOCK_MAGIC;
    lock->name = name ? name : "unnamed";
    lock->readers = 0;
    lock->writer_cpu = 0xFFFFFFFF;
    lock->max_readers = 0;
    lock->read_count = 0;
    lock->write_count = 0;
    lock->read_contentions = 0;
    lock->write_contentions = 0;

    pr_debug("RWLock: Initialized lock '%s' at %p\n", name, lock);
}

/*===========================================================================*/
/*                         READ LOCK ACQUISITION                             */
/*===========================================================================*/

/**
 * __read_lock_slowpath - Slow path for read lock acquisition
 * @lock: Read-write lock to acquire
 *
 * Called when there's a writer or waiting writers. Uses busy-waiting
 * to acquire the read lock.
 */
static void __read_lock_slowpath(rwlock_t *lock)
{
    u32 spin_count = 0;
    u32 old_lock;
    u32 new_lock;
    bool contended = false;

    while (1) {
        old_lock = lock->lock;

        /* Check if we can acquire read lock */
        if (!(old_lock & RWLOCK_WRITE_BIT) && !(old_lock & RWLOCK_WAITING_WRITERS)) {
            new_lock = old_lock + 1;

            if (__sync_val_compare_and_swap(&lock->lock, old_lock, new_lock) == old_lock) {
                /* Read lock acquired */
                lock->readers++;
                if (lock->readers > lock->max_readers) {
                    lock->max_readers = lock->readers;
                }
                lock->read_count++;
                if (contended) {
                    lock->read_contentions++;
                    atomic_inc(&rwlock_global.total_read_contentions);
                }
                atomic_inc(&rwlock_global.total_read_locks);
                atomic_inc(&rwlock_global.active_readers);
                return;
            }
        }

        /* Spin with backoff */
        spin_count++;
        if (spin_count < RWLOCK_SPIN_LIMIT) {
            for (u32 i = 0; i < (spin_count < 100 ? spin_count : 100); i++) {
                cpu_relax();
            }
        } else {
            /* Yield after too much spinning */
            delay_ms(0);
            spin_count = 0;
        }

        barrier();
        contended = true;
    }
}

/**
 * read_lock - Acquire a read lock
 * @lock: Read-write lock to acquire
 *
 * Acquires the read lock for shared access. Multiple readers can hold
 * the lock concurrently, but writers are excluded.
 */
void read_lock(rwlock_t *lock)
{
    u32 old_lock;
    u32 new_lock;

    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return;
    }

    /* Fast path: try atomic acquire */
    old_lock = lock->lock;
    if (!(old_lock & RWLOCK_WRITE_BIT) && !(old_lock & RWLOCK_WAITING_WRITERS)) {
        new_lock = old_lock + 1;
        if (__sync_val_compare_and_swap(&lock->lock, old_lock, new_lock) == old_lock) {
            lock->readers++;
            if (lock->readers > lock->max_readers) {
                lock->max_readers = lock->readers;
            }
            lock->read_count++;
            atomic_inc(&rwlock_global.total_read_locks);
            atomic_inc(&rwlock_global.active_readers);
            return;
        }
    }

    /* Slow path */
    __read_lock_slowpath(lock);
}

/**
 * read_trylock - Try to acquire a read lock without blocking
 * @lock: Read-write lock to acquire
 *
 * Returns: true if lock acquired, false otherwise
 */
bool read_trylock(rwlock_t *lock)
{
    u32 old_lock;
    u32 new_lock;

    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return false;
    }

    old_lock = lock->lock;
    if (!(old_lock & RWLOCK_WRITE_BIT) && !(old_lock & RWLOCK_WAITING_WRITERS)) {
        new_lock = old_lock + 1;
        if (__sync_val_compare_and_swap(&lock->lock, old_lock, new_lock) == old_lock) {
            lock->readers++;
            if (lock->readers > lock->max_readers) {
                lock->max_readers = lock->readers;
            }
            lock->read_count++;
            atomic_inc(&rwlock_global.total_read_locks);
            atomic_inc(&rwlock_global.active_readers);
            return true;
        }
    }

    return false;
}

/*===========================================================================*/
/*                         READ LOCK RELEASE                                 */
/*===========================================================================*/

/**
 * read_unlock - Release a read lock
 * @lock: Read-write lock to release
 *
 * Releases the read lock, potentially allowing a waiting writer to proceed.
 */
void read_unlock(rwlock_t *lock)
{
    u32 old_lock;
    u32 new_lock;

    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return;
    }

    /* Decrement reader count */
    do {
        old_lock = lock->lock;
        new_lock = old_lock - 1;
    } while (__sync_val_compare_and_swap(&lock->lock, old_lock, new_lock) != old_lock);

    lock->readers--;

    mb(); /* Memory barrier before release */

    atomic_dec(&rwlock_global.active_readers);
}

/*===========================================================================*/
/*                         WRITE LOCK ACQUISITION                            */
/*===========================================================================*/

/**
 * __write_lock_slowpath - Slow path for write lock acquisition
 * @lock: Read-write lock to acquire
 *
 * Called when there are active readers. Uses busy-waiting to acquire
 * the write lock.
 */
static void __write_lock_slowpath(rwlock_t *lock)
{
    u32 spin_count = 0;
    u32 old_lock;
    bool contended = false;

    /* First, set the waiting writer flag */
    do {
        old_lock = lock->lock;
    } while (__sync_val_compare_and_swap(&lock->lock, old_lock,
                                          old_lock | RWLOCK_WAITING_WRITERS) != old_lock);

    /* Acquire the internal write lock */
    spin_lock(&lock->write_lock);

    while (1) {
        old_lock = lock->lock;

        /* Check if we can acquire write lock (no readers) */
        if ((old_lock & RWLOCK_READ_MASK) == 0) {
            /* Try to set write bit and clear waiting flag */
            if (__sync_val_compare_and_swap(&lock->lock, old_lock,
                                             RWLOCK_WRITE_BIT) == old_lock) {
                /* Write lock acquired */
                lock->writer_cpu = get_cpu_id();
                lock->write_count++;
                if (contended) {
                    lock->write_contentions++;
                    atomic_inc(&rwlock_global.total_write_contentions);
                }
                atomic_inc(&rwlock_global.total_write_locks);
                atomic_inc(&rwlock_global.active_writers);
                return;
            }
        }

        /* Spin with backoff */
        spin_count++;
        if (spin_count < RWLOCK_SPIN_LIMIT) {
            for (u32 i = 0; i < (spin_count < 100 ? spin_count : 100); i++) {
                cpu_relax();
            }
        } else {
            /* Yield after too much spinning */
            delay_ms(0);
            spin_count = 0;
        }

        barrier();
        contended = true;
    }
}

/**
 * write_lock - Acquire a write lock
 * @lock: Read-write lock to acquire
 *
 * Acquires the write lock for exclusive access. All readers and other
 * writers are excluded while the write lock is held.
 */
void write_lock(rwlock_t *lock)
{
    u32 old_lock;

    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return;
    }

    /* Fast path: try atomic acquire when no readers */
    old_lock = lock->lock;
    if ((old_lock & RWLOCK_READ_MASK) == 0 &&
        __sync_val_compare_and_swap(&lock->lock, old_lock, RWLOCK_WRITE_BIT) == old_lock) {
        lock->writer_cpu = get_cpu_id();
        lock->write_count++;
        atomic_inc(&rwlock_global.total_write_locks);
        atomic_inc(&rwlock_global.active_writers);
        return;
    }

    /* Slow path */
    __write_lock_slowpath(lock);
}

/**
 * write_trylock - Try to acquire a write lock without blocking
 * @lock: Read-write lock to acquire
 *
 * Returns: true if lock acquired, false otherwise
 */
bool write_trylock(rwlock_t *lock)
{
    u32 old_lock;

    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return false;
    }

    old_lock = lock->lock;
    if ((old_lock & RWLOCK_READ_MASK) == 0 &&
        __sync_val_compare_and_swap(&lock->lock, old_lock, RWLOCK_WRITE_BIT) == old_lock) {
        lock->writer_cpu = get_cpu_id();
        lock->write_count++;
        atomic_inc(&rwlock_global.total_write_locks);
        atomic_inc(&rwlock_global.active_writers);
        return true;
    }

    return false;
}

/*===========================================================================*/
/*                         WRITE LOCK RELEASE                                */
/*===========================================================================*/

/**
 * write_unlock - Release a write lock
 * @lock: Read-write lock to release
 *
 * Releases the write lock, allowing readers or the next waiting writer
 * to proceed.
 */
void write_unlock(rwlock_t *lock)
{
    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return;
    }

    /* Clear write bit and waiting writers flag */
    mb(); /* Memory barrier before release */

    lock->writer_cpu = 0xFFFFFFFF;
    lock->lock = RWLOCK_UNLOCKED;

    /* Release the internal write lock */
    spin_unlock(&lock->write_lock);

    atomic_dec(&rwlock_global.active_writers);
}

/*===========================================================================*/
/*                         RWLOCK STATE QUERIES                              */
/*===========================================================================*/

/**
 * rwlock_is_locked - Check if rwlock is locked (by anyone)
 * @lock: Read-write lock to check
 *
 * Returns: true if locked, false if unlocked
 */
bool rwlock_is_locked(rwlock_t *lock)
{
    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return false;
    }

    return lock->lock != RWLOCK_UNLOCKED;
}

/**
 * rwlock_is_write_locked - Check if rwlock is write-locked
 * @lock: Read-write lock to check
 *
 * Returns: true if write-locked, false otherwise
 */
bool rwlock_is_write_locked(rwlock_t *lock)
{
    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return false;
    }

    return (lock->lock & RWLOCK_WRITE_BIT) != 0;
}

/**
 * rwlock_readers - Get number of active readers
 * @lock: Read-write lock to query
 *
 * Returns: Number of readers holding the lock
 */
u32 rwlock_readers(rwlock_t *lock)
{
    if (!lock || lock->magic != RWLOCK_MAGIC) {
        return 0;
    }

    return lock->readers;
}

/*===========================================================================*/
/*                         RWLOCK STATISTICS                                 */
/*===========================================================================*/

/**
 * rwlock_stats - Get read-write lock statistics
 * @lock: Read-write lock to query
 * @stats: Statistics structure to fill
 */
void rwlock_stats(rwlock_t *lock, struct rwlock_stats *stats)
{
    if (!lock || lock->magic != RWLOCK_MAGIC || !stats) {
        return;
    }

    memset(stats, 0, sizeof(struct rwlock_stats));

    stats->total_read_acquisitions = lock->read_count;
    stats->total_write_acquisitions = lock->write_count;
    /* Additional stats would be tracked with timestamps */
}

/**
 * rwlock_global_stats - Print global rwlock statistics
 */
void rwlock_global_stats(void)
{
    spin_lock(&rwlock_global.lock);

    printk("\n=== Global RWLock Statistics ===\n");
    printk("Total Read Locks: %d\n", atomic_read(&rwlock_global.total_read_locks));
    printk("Total Write Locks: %d\n", atomic_read(&rwlock_global.total_write_locks));
    printk("Total Read Contentions: %d\n", atomic_read(&rwlock_global.total_read_contentions));
    printk("Total Write Contentions: %d\n", atomic_read(&rwlock_global.total_write_contentions));
    printk("Active Readers: %d\n", atomic_read(&rwlock_global.active_readers));
    printk("Active Writers: %d\n", atomic_read(&rwlock_global.active_writers));
    printk("Total Read Hold Time: %llu ticks\n",
           (unsigned long long)rwlock_global.total_read_hold_time);
    printk("Total Write Hold Time: %llu ticks\n",
           (unsigned long long)rwlock_global.total_write_hold_time);
    printk("Max Read Hold Time: %llu ticks\n",
           (unsigned long long)rwlock_global.max_read_hold_time);
    printk("Max Write Hold Time: %llu ticks\n",
           (unsigned long long)rwlock_global.max_write_hold_time);

    spin_unlock(&rwlock_global.lock);
}

/**
 * rwlock_dump - Dump all rwlock information
 */
void rwlock_dump(void)
{
    printk("\n=== RWLock Dump ===\n");
    rwlock_global_stats();
}

/*===========================================================================*/
/*                         RWLOCK INITIALIZATION                             */
/*===========================================================================*/

/**
 * rwlock_subsystem_init - Initialize rwlock subsystem
 */
void rwlock_subsystem_init(void)
{
    pr_info("Initializing Read-Write Lock Subsystem...\n");

    spin_lock_init(&rwlock_global.lock);
    atomic_set(&rwlock_global.total_read_locks, 0);
    atomic_set(&rwlock_global.total_write_locks, 0);
    atomic_set(&rwlock_global.total_read_contentions, 0);
    atomic_set(&rwlock_global.total_write_contentions, 0);
    atomic_set(&rwlock_global.active_readers, 0);
    atomic_set(&rwlock_global.active_writers, 0);
    rwlock_global.total_read_hold_time = 0;
    rwlock_global.total_write_hold_time = 0;
    rwlock_global.max_read_hold_time = 0;
    rwlock_global.max_write_hold_time = 0;

    pr_info("  Spin limit: %u iterations\n", RWLOCK_SPIN_LIMIT);
    pr_info("  Debug mode: %s\n", RWLOCK_DEBUG ? "enabled" : "disabled");
    pr_info("Read-Write Lock Subsystem initialized.\n");
}
