/*
 * NEXUS OS - Synchronization Primitives Header
 * kernel/sync/sync.h
 */

#ifndef _NEXUS_SYNC_H
#define _NEXUS_SYNC_H

#include "../include/kernel.h"

/*===========================================================================*/
/*                         SYNCHRONIZATION CONSTANTS                         */
/*===========================================================================*/

/* Spinlock Constants */
#define SPINLOCK_UNLOCKED       0
#define SPINLOCK_LOCKED         1
#define SPINLOCK_MAGIC          0xSPN00001

/* Read-Write Lock Constants */
#define RWLOCK_UNLOCKED         0
#define RWLOCK_READ_LOCKED      0x10000000
#define RWLOCK_WRITE_LOCKED     0x80000000
#define RWLOCK_MAGIC            0xRWL00001
#define RWLOCK_MAX_READERS      1024

/* Atomic Constants */
#define ATOMIC_INIT(i)          { (i) }
#define ATOMIC64_INIT(i)        { (i) }

/* Wait Queue Constants */
#define WAITQUEUE_MAGIC         0xWQ0000001
#define WAITQUEUE_TIMEOUT       0x00000001
#define WAITQUEUE_EXCLUSIVE     0x00000002
#define WAITQUEUE_WOKEN         0x00000004

/* Barrier Constants */
#define BARRIER_MAGIC           0xBAR00001
#define BARRIER_FLAG_DESTROY    0x00000001

/* Condition Variable Constants */
#define COND_MAGIC              0xCND00001
#define COND_FLAG_WAITERS       0x00000001

/*===========================================================================*/
/*                         SPINLOCK STRUCTURES                               */
/*===========================================================================*/

/**
 * spinlock_t - Spinlock structure
 *
 * A spinlock is a low-level synchronization primitive that uses
 * busy-waiting to acquire the lock. Suitable for short critical sections.
 */
typedef struct {
    volatile u32 lock;        /* Lock state */
    u32 magic;                /* Magic number for validation */
    const char *name;         /* Lock name for debugging */
    u32 owner_cpu;            /* CPU that owns the lock */
    u64 acquire_time;         /* Time lock was acquired */
    u64 total_hold_time;      /* Total time held */
    u64 acquire_count;        /* Number of acquisitions */
    u64 contention_count;     /* Number of contentions */
} spinlock_t;

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
};

/*===========================================================================*/
/*                         READ-WRITE LOCK STRUCTURES                        */
/*===========================================================================*/

/**
 * rwlock_t - Read-write lock structure
 *
 * A read-write lock allows multiple readers or a single writer.
 * Readers can hold the lock concurrently, writers have exclusive access.
 */
typedef struct {
    volatile u32 lock;        /* Lock state (readers count + write flag) */
    spinlock_t write_lock;    /* Write exclusion spinlock */
    u32 magic;                /* Magic number for validation */
    const char *name;         /* Lock name for debugging */
    u32 readers;              /* Current reader count */
    u32 writer_cpu;           /* CPU of current writer */
    u32 max_readers;          /* Maximum concurrent readers */
    u64 read_count;           /* Number of read acquisitions */
    u64 write_count;          /* Number of write acquisitions */
    u64 read_contentions;     /* Read contention count */
    u64 write_contentions;    /* Write contention count */
} rwlock_t;

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
};

/*===========================================================================*/
/*                         ATOMIC OPERATION STRUCTURES                       */
/*===========================================================================*/

/**
 * atomic_t - Atomic integer
 *
 * Provides atomic operations on a 32-bit signed integer.
 */
typedef struct {
    volatile s32 counter;     /* Counter value */
} atomic_t;

/**
 * atomic64_t - Atomic 64-bit integer
 *
 * Provides atomic operations on a 64-bit signed integer.
 */
typedef struct {
    volatile s64 counter;     /* Counter value */
} atomic64_t;

/**
 * atomic_bitmask_t - Atomic bitmask
 *
 * Provides atomic bit operations on a 32-bit value.
 */
typedef struct {
    volatile u32 mask;        /* Bitmask value */
} atomic_bitmask_t;

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
} wait_queue_head_t;

/* Wait queue callback function type */
typedef int (*wait_queue_func_t)(wait_queue_entry_t *, u32, void *);

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
    wait_queue_head_t waitq;  /* Wait queue */
    u32 count;                /* Total threads */
    u32 waiting;              /* Threads currently waiting */
    u32 phase;                /* Current phase */
    u32 flags;                /* Barrier flags */
} barrier_t;

/*===========================================================================*/
/*                         CONDITION VARIABLE STRUCTURES                     */
/*===========================================================================*/

/**
 * condvar_t - Condition variable
 *
 * Used with a mutex to wait for a condition to become true.
 */
typedef struct {
    u32 magic;                /* Magic number */
    spinlock_t lock;          /* Protection lock */
    wait_queue_head_t waitq;  /* Wait queue */
    u32 waiters;              /* Number of waiters */
    u32 flags;                /* Condition flags */
} condvar_t;

/*===========================================================================*/
/*                         SPINLOCK API                                      */
/*===========================================================================*/

/* Initialization */
void spin_lock_init(spinlock_t *lock);
void spin_lock_init_named(spinlock_t *lock, const char *name);

/* Lock Operations */
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
bool spin_trylock(spinlock_t *lock);
void spin_lock_irq(spinlock_t *lock);
void spin_unlock_irq(spinlock_t *lock);
void spin_lock_irqsave(spinlock_t *lock, unsigned long *flags);
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

/* Lock State */
bool spin_is_locked(spinlock_t *lock);
bool spin_is_contended(spinlock_t *lock);
u32 spin_lock_cpu(spinlock_t *lock);

/* Debugging */
void spin_lock_assert(spinlock_t *lock);
void spin_lock_stats(spinlock_t *lock, struct spinlock_stats *stats);

/*===========================================================================*/
/*                         READ-WRITE LOCK API                               */
/*===========================================================================*/

/* Initialization */
void rwlock_init(rwlock_t *lock);
void rwlock_init_named(rwlock_t *lock, const char *name);

/* Read Lock Operations */
void read_lock(rwlock_t *lock);
void read_unlock(rwlock_t *lock);
bool read_trylock(rwlock_t *lock);

/* Write Lock Operations */
void write_lock(rwlock_t *lock);
void write_unlock(rwlock_t *lock);
bool write_trylock(rwlock_t *lock);

/* Lock State */
bool rwlock_is_locked(rwlock_t *lock);
bool rwlock_is_write_locked(rwlock_t *lock);
u32 rwlock_readers(rwlock_t *lock);

/* Debugging */
void rwlock_stats(rwlock_t *lock, struct rwlock_stats *stats);

/*===========================================================================*/
/*                         ATOMIC OPERATIONS API                             */
/*===========================================================================*/

/* Initialization */
#define atomic_set(v, i)        ((v)->counter = (i))
#define atomic_read(v)          ((v)->counter)
#define atomic64_set(v, i)      ((v)->counter = (i))
#define atomic64_read(v)        ((v)->counter)

/* Arithmetic Operations */
void atomic_add(s32 i, atomic_t *v);
void atomic_sub(s32 i, atomic_t *v);
void atomic_inc(atomic_t *v);
void atomic_dec(atomic_t *v);
s32 atomic_add_return(s32 i, atomic_t *v);
s32 atomic_sub_return(s32 i, atomic_t *v);
s32 atomic_inc_return(atomic_t *v);
s32 atomic_dec_return(atomic_t *v);

/* 64-bit Arithmetic */
void atomic64_add(s64 i, atomic64_t *v);
void atomic64_sub(s64 i, atomic64_t *v);
void atomic64_inc(atomic64_t *v);
void atomic64_dec(atomic64_t *v);
s64 atomic64_add_return(s64 i, atomic64_t *v);
s64 atomic64_sub_return(s64 i, atomic64_t *v);

/* Bitwise Operations */
void atomic_and(u32 mask, atomic_t *v);
void atomic_or(u32 mask, atomic_t *v);
void atomic_xor(u32 mask, atomic_t *v);
void atomic_andnot(u32 mask, atomic_t *v);

/* Compare and Swap */
s32 atomic_cmpxchg(atomic_t *v, s32 old, s32 new);
s64 atomic64_cmpxchg(atomic64_t *v, s64 old, s64 new);
s32 atomic_xchg(atomic_t *v, s32 new);
s64 atomic64_xchg(atomic64_t *v, s64 new);

/* Bit Operations */
bool atomic_test_and_set_bit(u32 nr, atomic_t *v);
void atomic_set_bit(u32 nr, atomic_t *v);
void atomic_clear_bit(u32 nr, atomic_t *v);
bool atomic_test_bit(u32 nr, atomic_t *v);
bool atomic_test_and_clear_bit(u32 nr, atomic_t *v);

/* Memory Barriers */
void atomic_mb(atomic_t *v);
void atomic_rmb(void);
void atomic_wmb(void);

/*===========================================================================*/
/*                         WAIT QUEUE API                                    */
/*===========================================================================*/

/* Initialization */
void init_waitqueue_head(wait_queue_head_t *wq);
void init_waitqueue_head_named(wait_queue_head_t *wq, const char *name);

/* Wait Queue Entry */
void init_wait(wait_queue_entry_t *wait);
void init_wait_entry(wait_queue_entry_t *wait, struct task_struct *task);

/* Wait Operations */
void wait_event(wait_queue_head_t *wq, int condition);
void wait_event_interruptible(wait_queue_head_t *wq, int condition);
void wait_event_timeout(wait_queue_head_t *wq, int condition, u64 timeout_ms);
void wait_event_lock_irq(wait_queue_head_t *wq, int condition, spinlock_t *lock);

/* Wake Operations */
void wake_up(wait_queue_head_t *wq);
void wake_up_interruptible(wait_queue_head_t *wq);
void wake_up_all(wait_queue_head_t *wq);
void wake_up_nr(wait_queue_head_t *wq, u32 nr);
void wake_up_interruptible_sync(wait_queue_head_t *wq);

/* Waiter Management */
void add_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wait);
void remove_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wait);
void prepare_to_wait(wait_queue_head_t *wq, wait_queue_entry_t *wait, int state);
void finish_wait(wait_queue_head_t *wq, wait_queue_entry_t *wait);

/* Query Operations */
bool waitqueue_active(wait_queue_head_t *wq);
u32 waitqueue_count(wait_queue_head_t *wq);

/*===========================================================================*/
/*                         BARRIER API                                       */
/*===========================================================================*/

/* Initialization */
void barrier_init(barrier_t *barrier, u32 count);
void barrier_destroy(barrier_t *barrier);

/* Barrier Operations */
void barrier_wait(barrier_t *barrier);
int barrier_wait_timeout(barrier_t *barrier, u64 timeout_ms);
void barrier_reset(barrier_t *barrier, u32 count);

/* Query Operations */
u32 barrier_waiting(barrier_t *barrier);
u32 barrier_count(barrier_t *barrier);

/*===========================================================================*/
/*                         CONDITION VARIABLE API                            */
/*===========================================================================*/

/* Initialization */
void condvar_init(condvar_t *cond);
void condvar_destroy(condvar_t *cond);

/* Condition Variable Operations */
void condvar_wait(condvar_t *cond, mutex_t *mutex);
int condvar_wait_interruptible(condvar_t *cond, mutex_t *mutex);
int condvar_wait_timeout(condvar_t *cond, mutex_t *mutex, u64 timeout_ms);
void condvar_signal(condvar_t *cond);
void condvar_broadcast(condvar_t *cond);

/* Query Operations */
u32 condvar_waiters(condvar_t *cond);

/*===========================================================================*/
/*                         SYNCHRONIZATION STATISTICS                        */
/*===========================================================================*/

void sync_stats(void);
void spinlock_global_stats(void);
void rwlock_global_stats(void);
void waitqueue_global_stats(void);

/*===========================================================================*/
/*                         SYNCHRONIZATION INITIALIZATION                    */
/*===========================================================================*/

void sync_init(void);

#endif /* _NEXUS_SYNC_H */
