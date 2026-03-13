/*
 * NEXUS OS - Synchronization Primitives Header
 * kernel/sync/sync.h
 *
 * Complete synchronization primitives for kernel threading and concurrency
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _NEXUS_SYNC_H
#define _NEXUS_SYNC_H

#include "../include/kernel.h"

/* Forward declarations */
struct task_struct;

/*===========================================================================*/
/*                         SYNCHRONIZATION CONSTANTS                         */
/*===========================================================================*/

/* Spinlock Constants */
#define SPINLOCK_UNLOCKED       0
#define SPINLOCK_LOCKED         1
#define SPINLOCK_MAGIC          0x504E0001

/* Read-Write Lock Constants */
#define RWLOCK_UNLOCKED         0
#define RWLOCK_READ_LOCKED      0x10000000
#define RWLOCK_WRITE_LOCKED     0x80000000
#define RWLOCK_MAGIC            0x52570001
#define RWLOCK_MAX_READERS      1024

/* Atomic Constants */
#define ATOMIC_INIT(i)          { (i) }
#define ATOMIC64_INIT(i)        { (i) }

/* Wait Queue Constants */
#define WAITQUEUE_MAGIC         0x57510001
#define WAITQUEUE_TIMEOUT       0x00000001
#define WAITQUEUE_EXCLUSIVE     0x00000002
#define WAITQUEUE_WOKEN         0x00000004

/* Barrier Constants */
#define BARRIER_MAGIC           0x42410001
#define BARRIER_FLAG_DESTROY    0x00000001

/* Condition Variable Constants */
#define COND_MAGIC              0x434E0001
#define COND_FLAG_WAITERS       0x00000001

/* Semaphore Constants */
#define SEMAPHORE_MAGIC         0x53450001
#define SEMAPHORE_MAX_COUNT     0x7FFFFFFF

/*===========================================================================*/
/*                         SPINLOCK STRUCTURES                               */
/*===========================================================================*/

/**
 * spinlock_t - Spinlock structure (defined in types.h)
 *
 * A spinlock is a low-level synchronization primitive that uses
 * busy-waiting to acquire the lock. Suitable for short critical sections.
 */
/* Already defined in types.h - do not redefine */

/**
 * spinlock_stats - Spinlock statistics
 */
struct spinlock_stats {
    u64 total_acquisitions;   /* Total acquisitions */
    u64 total_contentions;    /* Total contentions */
    u64 total_hold_time;      /* Total hold time (ns) */
    u64 max_hold_time;        /* Maximum hold time (ns) */
    u64 total_wait_time;      /* Total wait time (ns) */
    u64 max_wait_time;        /* Maximum wait time (ns) */
    u64 recursive_count;      /* Recursive lock count */
    u64 irq_disabled_count;   /* IRQ disabled count */
};

/*===========================================================================*/
/*                         READ-WRITE LOCK STRUCTURES                        */
/*===========================================================================*/

/**
 * rwlock_t - Read-write lock structure (defined in types.h)
 *
 * A read-write lock allows multiple readers or a single writer.
 * Readers can hold the lock concurrently, writers have exclusive access.
 */
/* Already defined in types.h - do not redefine */

/**
 * rwlock_stats - Read-write lock statistics
 */
struct rwlock_stats {
    u64 total_read_acquisitions;
    u64 total_write_acquisitions;
    u64 total_read_hold_time;
    u64 total_write_hold_time;
    u64 max_read_hold_time;
    u64 max_write_hold_time;
    u64 reader_starvation_count;
    u64 writer_starvation_count;
    u64 upgrade_count;        /* Read to write upgrades */
    u64 downgrade_count;      /* Write to read downgrades */
};

/*===========================================================================*/
/*                         ATOMIC OPERATION STRUCTURES                       */
/*===========================================================================*/

/**
 * atomic_t - Atomic integer (defined in types.h)
 * atomic64_t - Atomic 64-bit integer (defined in types.h)
 */
/* Already defined in types.h - do not redefine */

/**
 * atomic_bitmask_t - Atomic bitmask
 *
 * Provides atomic bit operations on a 32-bit value.
 */
typedef struct {
    volatile u32 mask;        /* Bitmask value */
    u32 magic;                /* Magic number for validation */
} atomic_bitmask_t;

/**
 * atomic_stats - Atomic operation statistics
 */
struct atomic_stats {
    u64 read_count;
    u64 write_count;
    u64 cmpxchg_success;
    u64 cmpxchg_failure;
    u64 add_count;
    u64 sub_count;
    u64 inc_count;
    u64 dec_count;
};

/*===========================================================================*/
/*                         WAIT QUEUE STRUCTURES                             */
/*===========================================================================*/

/**
 * wait_queue_entry - Wait queue entry
 */
typedef struct wait_queue_entry {
    struct list_head list;        /* List entry */
    struct task_struct *task;     /* Waiting task */
    u32 flags;                    /* Entry flags */
    void *private;                /* Private data */
    int (*func)(struct wait_queue_entry *, u32, void *); /* Callback */
    u64 enqueue_time;             /* Time enqueued */
    u64 timeout;                  /* Timeout in ms */
} wait_queue_entry_t;

/**
 * wait_queue_head - Wait queue head
 */
typedef struct {
    u32 magic;                /* Magic number */
    spinlock_t lock;          /* Protection lock */
    struct list_head head;    /* Waiter list */
    u32 waiters;              /* Number of waiters */
    u32 wakeups;              /* Number of wakeups */
    const char *name;         /* Queue name */
    u64 total_wait_time;      /* Total wait time */
    u64 max_wait_time;        /* Maximum wait time */
} wait_queue_head_t;

/* Type alias for wait_queue_head_t */
typedef wait_queue_head_t wait_queue_t;

/* Wait queue callback function type */
typedef int (*wait_queue_func_t)(wait_queue_entry_t *, u32, void *);

/* Wait queue flags */
#define WQ_FLAG_EXCLUSIVE     0x01
#define WQ_FLAG_SYNC          0x02
#define WQ_FLAG_FREE_ON_WAKE  0x04

/**
 * wait_queue_stats - Wait queue statistics
 */
struct wait_queue_stats {
    u64 total_enqueues;
    u64 total_dequeues;
    u64 total_wakeups;
    u64 total_timeouts;
    u64 total_wait_time;
    u64 max_wait_time;
    u64 spurious_wakeups;
};

/*===========================================================================*/
/*                         BARRIER STRUCTURES                                */
/*===========================================================================*/

/**
 * barrier_t - Thread barrier
 *
 * Synchronization point where multiple threads wait for each other.
 */
typedef struct {
    u32 magic;                /* Magic number */
    spinlock_t lock;          /* Protection lock */
    u32 count;                /* Number of threads */
    u32 waiting;              /* Number of waiting threads */
    u32 phase;                /* Current phase */
    u32 flags;                /* Barrier flags */
    wait_queue_head_t waitq;  /* Wait queue */
    u64 creation_time;        /* Creation time */
    const char *name;         /* Barrier name */
} barrier_t;

/**
 * barrier_stats - Barrier statistics
 */
struct barrier_stats {
    u64 total_arrivals;
    u64 total_departures;
    u64 total_wait_time;
    u64 max_wait_time;
    u64 contention_count;
    u64 timeout_count;
    u64 destroy_count;
};

/*===========================================================================*/
/*                         CONDITION VARIABLE STRUCTURES                     */
/*===========================================================================*/

/**
 * condvar_t - Condition variable
 *
 * Provides wait/notify synchronization between threads.
 */
typedef struct {
    u32 magic;                /* Magic number */
    spinlock_t lock;          /* Protection lock */
    wait_queue_head_t waitq;  /* Wait queue */
    u32 waiters;              /* Number of waiters */
    u32 signals;              /* Pending signals */
    u32 flags;                /* Condition flags */
    u64 creation_time;        /* Creation time */
    const char *name;         /* Condition name */
} condvar_t;

/**
 * condvar_stats - Condition variable statistics
 */
struct condvar_stats {
    u64 total_waits;
    u64 total_signals;
    u64 total_broadcasts;
    u64 total_timeouts;
    u64 total_wait_time;
    u64 max_wait_time;
    u64 spurious_wakeups;
};

/*===========================================================================*/
/*                         SEMAPHORE STRUCTURES                              */
/*===========================================================================*/

/**
 * semaphore_t - Counting semaphore
 *
 * Provides counting semaphore synchronization.
 */
typedef struct {
    u32 magic;                /* Magic number */
    spinlock_t lock;          /* Protection lock */
    wait_queue_head_t waitq;  /* Wait queue */
    s32 count;                /* Semaphore count */
    s32 max;                  /* Maximum count */
    u32 flags;                /* Semaphore flags */
    u64 creation_time;        /* Creation time */
    const char *name;         /* Semaphore name */
} semaphore_t;

/**
 * semaphore_stats - Semaphore statistics
 */
struct semaphore_stats {
    u64 total_waits;
    u64 total_posts;
    u64 total_trywaits;
    u64 trywait_success;
    u64 trywait_failure;
    u64 total_wait_time;
    u64 max_wait_time;
    u64 timeout_count;
};

/*===========================================================================*/
/*                         MUTEX STRUCTURES                                  */
/*===========================================================================*/

/**
 * mutex_t - Mutex structure (defined in types.h)
 */
/* Already defined in types.h */

/**
 * mutex_stats - Mutex statistics
 */
struct mutex_stats {
    u64 total_locks;
    u64 total_unlocks;
    u64 total_contentions;
    u64 total_wait_time;
    u64 max_wait_time;
    u64 recursive_count;
    u64 trylock_success;
    u64 trylock_failure;
};

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Spinlock functions */
void spinlock_init(spinlock_t *lock);
void spinlock_init_named(spinlock_t *lock, const char *name);
void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);
bool spinlock_trylock(spinlock_t *lock);
void spinlock_lock_irqsave(spinlock_t *lock, u64 *flags);
void spinlock_unlock_irqrestore(spinlock_t *lock, u64 flags);
void spinlock_lock_irq(spinlock_t *lock);
void spinlock_unlock_irq(spinlock_t *lock);
void spinlock_destroy(spinlock_t *lock);
bool spinlock_is_locked(spinlock_t *lock);
void spinlock_get_stats(spinlock_t *lock, struct spinlock_stats *stats);

/* Read-write lock functions */
void rwlock_init(rwlock_t *lock);
void rwlock_init_named(rwlock_t *lock, const char *name);
void read_lock(rwlock_t *lock);
void read_unlock(rwlock_t *lock);
void write_lock(rwlock_t *lock);
void write_unlock(rwlock_t *lock);
bool read_trylock(rwlock_t *lock);
bool write_trylock(rwlock_t *lock);
void rwlock_destroy(rwlock_t *lock);
bool rwlock_is_read_locked(rwlock_t *lock);
bool rwlock_is_write_locked(rwlock_t *lock);
void rwlock_get_stats(rwlock_t *lock, struct rwlock_stats *stats);

/* Atomic operations - defined as inline functions in kernel.h */
/* atomic_inc, atomic_dec, atomic_add_return, atomic_cmpxchg, atomic_read, atomic_set */

/* Additional atomic operations */
void atomic_add(s32 i, atomic_t *v);
void atomic_sub(s32 i, atomic_t *v);
s32 atomic_sub_return(s32 i, atomic_t *v);
s32 atomic_xchg(atomic_t *v, s32 new);
void atomic_and(u32 mask, atomic_t *v);
void atomic_or(u32 mask, atomic_t *v);
void atomic_xor(u32 mask, atomic_t *v);
bool atomic_inc_and_test(atomic_t *v);
bool atomic_dec_and_test(atomic_t *v);

/* Atomic 64-bit operations */
void atomic64_inc(atomic64_t *v);
void atomic64_dec(atomic64_t *v);
void atomic64_add(s64 i, atomic64_t *v);
void atomic64_sub(s64 i, atomic64_t *v);
s64 atomic64_read(atomic64_t *v);
void atomic64_set(atomic64_t *v, s64 i);
s64 atomic64_cmpxchg(atomic64_t *v, s64 old, s64 new);

/* Atomic bitmask operations */
void atomic_bitmask_init(atomic_bitmask_t *mask);
void atomic_bitmask_set(atomic_bitmask_t *mask, u32 bit);
void atomic_bitmask_clear(atomic_bitmask_t *mask, u32 bit);
bool atomic_bitmask_test(atomic_bitmask_t *mask, u32 bit);
bool atomic_bitmask_test_and_set(atomic_bitmask_t *mask, u32 bit);
bool atomic_bitmask_test_and_clear(atomic_bitmask_t *mask, u32 bit);

/* Wait queue functions */
void wait_queue_init(wait_queue_head_t *wq);
void wait_queue_init_named(wait_queue_head_t *wq, const char *name);
void wait_queue_add(wait_queue_head_t *wq, wait_queue_entry_t *entry);
void wait_queue_remove(wait_queue_head_t *wq, wait_queue_entry_t *entry);
void wait_queue_wake(wait_queue_head_t *wq);
void wait_queue_wake_all(wait_queue_head_t *wq);
void wait_queue_wake_nr(wait_queue_head_t *wq, u32 nr);
void wait_queue_wake_up(wait_queue_head_t *wq, u32 mode, int sync);
void wait_queue_entry_init(wait_queue_entry_t *entry, struct task_struct *task);
int wait_queue_default_func(wait_queue_entry_t *entry, u32 mode, void *key);
void wait_queue_sleep(wait_queue_head_t *wq, u32 timeout_ms);
bool wait_queue_active(wait_queue_head_t *wq);
void wait_queue_get_stats(wait_queue_head_t *wq, struct wait_queue_stats *stats);

/* Wait event macros */
void wait_event(wait_queue_head_t *wq, int condition);
int wait_event_interruptible(wait_queue_head_t *wq, int condition);

/* Barrier functions */
void barrier_init(barrier_t *barrier, u32 count);
void barrier_init_named(barrier_t *barrier, u32 count, const char *name);
void barrier_wait(barrier_t *barrier);
int barrier_wait_timeout(barrier_t *barrier, u32 timeout_ms);
void barrier_destroy(barrier_t *barrier);
void barrier_get_stats(barrier_t *barrier, struct barrier_stats *stats);

/* Condition variable functions */
void condvar_init(condvar_t *cond);
void condvar_init_named(condvar_t *cond, const char *name);
void condvar_wait(condvar_t *cond, spinlock_t *lock);
int condvar_wait_timeout(condvar_t *cond, spinlock_t *lock, u32 timeout_ms);
void condvar_signal(condvar_t *cond);
void condvar_broadcast(condvar_t *cond);
void condvar_destroy(condvar_t *cond);
void condvar_get_stats(condvar_t *cond, struct condvar_stats *stats);

/* Semaphore functions */
void semaphore_init(semaphore_t *sem, s32 count, s32 max);
void semaphore_init_named(semaphore_t *sem, s32 count, s32 max, const char *name);
void semaphore_wait(semaphore_t *sem);
int semaphore_wait_timeout(semaphore_t *sem, u32 timeout_ms);
void semaphore_post(semaphore_t *sem);
bool semaphore_trywait(semaphore_t *sem);
void semaphore_destroy(semaphore_t *sem);
void semaphore_get_stats(semaphore_t *sem, struct semaphore_stats *stats);

/* Mutex functions */
void mutex_init(mutex_t *mutex);
void mutex_init_named(mutex_t *mutex, const char *name);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
bool mutex_trylock(mutex_t *mutex);
void mutex_destroy(mutex_t *mutex);
bool mutex_is_locked(mutex_t *mutex);
void mutex_get_stats(mutex_t *mutex, struct mutex_stats *stats);

/* Memory barriers - defined in kernel.h */
/* Compiler barriers */
#ifndef barrier
#define barrier() __asm__ __volatile__ ("" ::: "memory")
#endif
#ifndef READ_ONCE
#define READ_ONCE(x) (*(volatile typeof(x) *)&(x))
#endif
#ifndef WRITE_ONCE
#define WRITE_ONCE(x, val) do { *(volatile typeof(x) *)&(x) = (val); } while (0)
#endif

#endif /* _NEXUS_SYNC_H */
