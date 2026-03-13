/*
 * NEXUS OS - Atomic Operations Implementation
 * kernel/sync/atomic.c
 */

#include "sync.h"

/*===========================================================================*/
/*                         BASIC ATOMIC OPERATIONS                           */
/*===========================================================================*/

/**
 * atomic_read - Read atomic value
 * @v: Atomic value to read
 *
 * Returns: Current value of atomic variable
 */
s32 atomic_read(const atomic_t *v)
{
    return v->counter;
}

/**
 * atomic_set - Set atomic value
 * @v: Atomic value to set
 * @i: Value to set
 */
void atomic_set(atomic_t *v, s32 i)
{
    v->counter = i;
}

/*===========================================================================*/
/*                         ATOMIC CONFIGURATION                              */
/*===========================================================================*/

/*===========================================================================*/
/*                         ATOMIC CONFIGURATION                              */
/*===========================================================================*/

#define ATOMIC_DEBUG            1

/* Memory ordering flags */
#define ATOMIC_RELAXED          0
#define ATOMIC_ACQUIRE          1
#define ATOMIC_RELEASE          2
#define ATOMIC_ACQ_REL          3
#define ATOMIC_SEQ_CST          4

/*===========================================================================*/
/*                         ATOMIC GLOBAL DATA                                */
/*===========================================================================*/

static struct {
    spinlock_t lock;              /* Statistics lock */
    atomic_t total_operations;    /* Total atomic operations */
    atomic_t cmpxchg_success;     /* Successful compare-and-swap */
    atomic_t cmpxchg_failure;     /* Failed compare-and-swap */
    atomic_t bit_operations;      /* Bit operations */
} atomic_global = {
    .lock = { .lock = SPINLOCK_UNLOCKED },
    .total_operations = ATOMIC_INIT(0),
    .cmpxchg_success = ATOMIC_INIT(0),
    .cmpxchg_failure = ATOMIC_INIT(0),
    .bit_operations = ATOMIC_INIT(0),
};

/*===========================================================================*/
/*                         ATOMIC ARITHMETIC OPERATIONS                      */
/*===========================================================================*/

/**
 * atomic_add - Atomically add to atomic value
 * @i: Value to add
 * @v: Atomic value
 *
 * Atomically adds @i to @v.
 */
void atomic_add(s32 i, atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_add(&v->counter, i);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic_sub - Atomically subtract from atomic value
 * @i: Value to subtract
 * @v: Atomic value
 *
 * Atomically subtracts @i from @v.
 */
void atomic_sub(s32 i, atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_sub(&v->counter, i);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic_inc - Atomically increment atomic value
 * @v: Atomic value
 *
 * Atomically increments @v by 1.
 */
void atomic_inc(atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_add(&v->counter, 1);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic_dec - Atomically decrement atomic value
 * @v: Atomic value
 *
 * Atomically decrements @v by 1.
 */
void atomic_dec(atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_sub(&v->counter, 1);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic_add_return - Atomically add and return new value
 * @i: Value to add
 * @v: Atomic value
 *
 * Atomically adds @i to @v and returns the new value.
 *
 * Returns: New value after addition
 */
s32 atomic_add_return(s32 i, atomic_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_add_and_fetch(&v->counter, i);
}

/**
 * atomic_sub_return - Atomically subtract and return new value
 * @i: Value to subtract
 * @v: Atomic value
 *
 * Atomically subtracts @i from @v and returns the new value.
 *
 * Returns: New value after subtraction
 */
s32 atomic_sub_return(s32 i, atomic_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_sub_and_fetch(&v->counter, i);
}

/**
 * atomic_inc_return - Atomically increment and return new value
 * @v: Atomic value
 *
 * Atomically increments @v by 1 and returns the new value.
 *
 * Returns: New value after increment
 */
s32 atomic_inc_return(atomic_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_add_and_fetch(&v->counter, 1);
}

/**
 * atomic_dec_return - Atomically decrement and return new value
 * @v: Atomic value
 *
 * Atomically decrements @v by 1 and returns the new value.
 *
 * Returns: New value after decrement
 */
s32 atomic_dec_return(atomic_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_sub_and_fetch(&v->counter, 1);
}

/**
 * atomic_dec_and_test - Atomically decrement and test for zero
 * @v: Atomic value
 *
 * Atomically decrements @v by 1 and returns true if the result is zero.
 *
 * Returns: true if result is zero, false otherwise
 */
bool atomic_dec_and_test(atomic_t *v)
{
    if (!v) {
        return false;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_sub_and_fetch(&v->counter, 1) == 0;
}

/**
 * atomic_inc_and_test - Atomically increment and test for zero
 * @v: Atomic value
 *
 * Atomically increments @v by 1 and returns true if the result is zero.
 *
 * Returns: true if result is zero, false otherwise
 */
bool atomic_inc_and_test(atomic_t *v)
{
    if (!v) {
        return false;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_add_and_fetch(&v->counter, 1) == 0;
}

/**
 * atomic_add_negative - Atomically add and test for negative
 * @i: Value to add
 * @v: Atomic value
 *
 * Atomically adds @i to @v and returns true if the result is negative.
 *
 * Returns: true if result is negative, false otherwise
 */
bool atomic_add_negative(s32 i, atomic_t *v)
{
    if (!v) {
        return false;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_add_and_fetch(&v->counter, i) < 0;
}

/*===========================================================================*/
/*                         64-BIT ATOMIC OPERATIONS                          */
/*===========================================================================*/

/**
 * atomic64_add - Atomically add to 64-bit atomic value
 * @i: Value to add
 * @v: 64-bit atomic value
 */
void atomic64_add(s64 i, atomic64_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_add(&v->counter, i);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic64_sub - Atomically subtract from 64-bit atomic value
 * @i: Value to subtract
 * @v: 64-bit atomic value
 */
void atomic64_sub(s64 i, atomic64_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_sub(&v->counter, i);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic64_inc - Atomically increment 64-bit atomic value
 * @v: 64-bit atomic value
 */
void atomic64_inc(atomic64_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_add(&v->counter, 1);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic64_dec - Atomically decrement 64-bit atomic value
 * @v: 64-bit atomic value
 */
void atomic64_dec(atomic64_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_sub(&v->counter, 1);
    atomic_inc(&atomic_global.total_operations);
}

/**
 * atomic64_add_return - Atomically add and return new 64-bit value
 * @i: Value to add
 * @v: 64-bit atomic value
 *
 * Returns: New value after addition
 */
s64 atomic64_add_return(s64 i, atomic64_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_add_and_fetch(&v->counter, i);
}

/**
 * atomic64_sub_return - Atomically subtract and return new 64-bit value
 * @i: Value to subtract
 * @v: 64-bit atomic value
 *
 * Returns: New value after subtraction
 */
s64 atomic64_sub_return(s64 i, atomic64_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_sub_and_fetch(&v->counter, i);
}

/**
 * atomic64_inc_return - Atomically increment and return new 64-bit value
 * @v: 64-bit atomic value
 *
 * Returns: New value after increment
 */
s64 atomic64_inc_return(atomic64_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_add_and_fetch(&v->counter, 1);
}

/**
 * atomic64_dec_return - Atomically decrement and return new 64-bit value
 * @v: 64-bit atomic value
 *
 * Returns: New value after decrement
 */
s64 atomic64_dec_return(atomic64_t *v)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_sub_and_fetch(&v->counter, 1);
}

/*===========================================================================*/
/*                         ATOMIC BITWISE OPERATIONS                         */
/*===========================================================================*/

/**
 * atomic_and - Atomically AND with atomic value
 * @mask: Mask to AND with
 * @v: Atomic value
 */
void atomic_and(u32 mask, atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_and(&v->counter, mask);
    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);
}

/**
 * atomic_or - Atomically OR with atomic value
 * @mask: Mask to OR with
 * @v: Atomic value
 */
void atomic_or(u32 mask, atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_or(&v->counter, mask);
    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);
}

/**
 * atomic_xor - Atomically XOR with atomic value
 * @mask: Mask to XOR with
 * @v: Atomic value
 */
void atomic_xor(u32 mask, atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_xor(&v->counter, mask);
    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);
}

/**
 * atomic_andnot - Atomically AND NOT with atomic value
 * @mask: Mask to AND NOT with
 * @v: Atomic value
 */
void atomic_andnot(u32 mask, atomic_t *v)
{
    if (!v) {
        return;
    }

    __sync_fetch_and_and(&v->counter, ~mask);
    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);
}

/*===========================================================================*/
/*                         ATOMIC COMPARE-AND-SWAP                           */
/*===========================================================================*/

/**
 * atomic_cmpxchg - Atomically compare and exchange
 * @v: Atomic value
 * @old: Expected value
 * @new: New value to store
 *
 * Atomically compares @v with @old. If equal, stores @new in @v.
 *
 * Returns: Original value of @v
 */
s32 atomic_cmpxchg(atomic_t *v, s32 old, s32 new)
{
    s32 result;

    if (!v) {
        return old;
    }

    result = __sync_val_compare_and_swap(&v->counter, old, new);
    atomic_inc(&atomic_global.total_operations);

    if (result == old) {
        atomic_inc(&atomic_global.cmpxchg_success);
    } else {
        atomic_inc(&atomic_global.cmpxchg_failure);
    }

    return result;
}

/**
 * atomic64_cmpxchg - Atomically compare and exchange (64-bit)
 * @v: 64-bit atomic value
 * @old: Expected value
 * @new: New value to store
 *
 * Returns: Original value of @v
 */
s64 atomic64_cmpxchg(atomic64_t *v, s64 old, s64 new)
{
    s64 result;

    if (!v) {
        return old;
    }

    result = __sync_val_compare_and_swap(&v->counter, old, new);
    atomic_inc(&atomic_global.total_operations);

    if (result == old) {
        atomic_inc(&atomic_global.cmpxchg_success);
    } else {
        atomic_inc(&atomic_global.cmpxchg_failure);
    }

    return result;
}

/**
 * atomic_xchg - Atomically exchange value
 * @v: Atomic value
 * @new: New value to store
 *
 * Atomically stores @new in @v and returns the old value.
 *
 * Returns: Old value of @v
 */
s32 atomic_xchg(atomic_t *v, s32 new)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_lock_test_and_set(&v->counter, new);
}

/**
 * atomic64_xchg - Atomically exchange value (64-bit)
 * @v: 64-bit atomic value
 * @new: New value to store
 *
 * Returns: Old value of @v
 */
s64 atomic64_xchg(atomic64_t *v, s64 new)
{
    if (!v) {
        return 0;
    }

    atomic_inc(&atomic_global.total_operations);
    return __sync_lock_test_and_set(&v->counter, new);
}

/*===========================================================================*/
/*                         ATOMIC BIT OPERATIONS                             */
/*===========================================================================*/

/**
 * atomic_test_and_set_bit - Atomically test and set a bit
 * @nr: Bit number (0-31)
 * @v: Atomic value
 *
 * Atomically sets the bit and returns its old value.
 *
 * Returns: Old value of the bit
 */
bool atomic_test_and_set_bit(u32 nr, atomic_t *v)
{
    u32 mask;
    s32 old;

    if (!v || nr >= 32) {
        return false;
    }

    mask = 1U << nr;
    old = __sync_fetch_and_or(&v->counter, mask);

    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);

    return (old & mask) != 0;
}

/**
 * atomic_set_bit - Atomically set a bit
 * @nr: Bit number (0-31)
 * @v: Atomic value
 */
void atomic_set_bit(u32 nr, atomic_t *v)
{
    u32 mask;

    if (!v || nr >= 32) {
        return;
    }

    mask = 1U << nr;
    __sync_fetch_and_or(&v->counter, mask);

    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);
}

/**
 * atomic_clear_bit - Atomically clear a bit
 * @nr: Bit number (0-31)
 * @v: Atomic value
 */
void atomic_clear_bit(u32 nr, atomic_t *v)
{
    u32 mask;

    if (!v || nr >= 32) {
        return;
    }

    mask = ~(1U << nr);
    __sync_fetch_and_and(&v->counter, mask);

    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);
}

/**
 * atomic_test_bit - Test a bit (non-atomic read)
 * @nr: Bit number (0-31)
 * @v: Atomic value
 *
 * Returns: true if bit is set, false otherwise
 */
bool atomic_test_bit(u32 nr, atomic_t *v)
{
    u32 mask;

    if (!v || nr >= 32) {
        return false;
    }

    mask = 1U << nr;
    return (atomic_read(v) & mask) != 0;
}

/**
 * atomic_test_and_clear_bit - Atomically test and clear a bit
 * @nr: Bit number (0-31)
 * @v: Atomic value
 *
 * Atomically clears the bit and returns its old value.
 *
 * Returns: Old value of the bit
 */
bool atomic_test_and_clear_bit(u32 nr, atomic_t *v)
{
    u32 mask;
    s32 old;

    if (!v || nr >= 32) {
        return false;
    }

    mask = 1U << nr;
    old = __sync_fetch_and_and(&v->counter, ~mask);

    atomic_inc(&atomic_global.total_operations);
    atomic_inc(&atomic_global.bit_operations);

    return (old & mask) != 0;
}

/*===========================================================================*/
/*                         MEMORY BARRIERS                                   */
/*===========================================================================*/

/**
 * atomic_mb - Full memory barrier
 * @v: Atomic value (for ordering)
 *
 * Ensures all memory operations before this point are complete
 * before any operations after this point begin.
 */
void atomic_mb(atomic_t *v)
{
    if (!v) {
        return;
    }

    /* Full memory barrier */
    mb();
}

/**
 * atomic_rmb - Read memory barrier
 *
 * Ensures all reads before this point are complete before any
 * reads after this point begin.
 */
void atomic_rmb(void)
{
    rmb();
}

/**
 * atomic_wmb - Write memory barrier
 *
 * Ensures all writes before this point are complete before any
 * writes after this point begin.
 */
void atomic_wmb(void)
{
    wmb();
}

/**
 * smp_mb - SMP full memory barrier
 *
 * Full memory barrier for SMP systems.
 */
void smp_mb(void)
{
    mb();
}

/**
 * smp_rmb - SMP read memory barrier
 */
void smp_rmb(void)
{
    rmb();
}

/**
 * smp_wmb - SMP write memory barrier
 */
void smp_wmb(void)
{
    wmb();
}

/**
 * smp_read_barrier_depends - Read barrier for data dependencies
 */
void smp_read_barrier_depends(void)
{
    barrier();
}

/*===========================================================================*/
/*                         ATOMIC STATISTICS                                 */
/*===========================================================================*/

/**
 * atomic_stats - Print atomic operation statistics
 */
void atomic_stats(void)
{
    spin_lock(&atomic_global.lock);

    printk("\n=== Atomic Operation Statistics ===\n");
    printk("Total Operations: %d\n", atomic_read(&atomic_global.total_operations));
    printk("CMPXCHG Success: %d\n", atomic_read(&atomic_global.cmpxchg_success));
    printk("CMPXCHG Failure: %d\n", atomic_read(&atomic_global.cmpxchg_failure));
    printk("Bit Operations: %d\n", atomic_read(&atomic_global.bit_operations));

    if (atomic_read(&atomic_global.total_operations) > 0) {
        u32 cmpxchg_rate = (atomic_read(&atomic_global.cmpxchg_success) * 100) /
                           (atomic_read(&atomic_global.cmpxchg_success) +
                            atomic_read(&atomic_global.cmpxchg_failure));
        printk("CMPXCHG Success Rate: %u%%\n", cmpxchg_rate);
    }

    spin_unlock(&atomic_global.lock);
}

/**
 * atomic_dump - Dump all atomic information
 */
void atomic_dump(void)
{
    printk("\n=== Atomic Operations Dump ===\n");
    atomic_stats();
}

/*===========================================================================*/
/*                         ATOMIC INITIALIZATION                             */
/*===========================================================================*/

/**
 * atomic_subsystem_init - Initialize atomic operations subsystem
 */
void atomic_subsystem_init(void)
{
    pr_info("Initializing Atomic Operations Subsystem...\n");

    spin_lock_init(&atomic_global.lock);
    atomic_set(&atomic_global.total_operations, 0);
    atomic_set(&atomic_global.cmpxchg_success, 0);
    atomic_set(&atomic_global.cmpxchg_failure, 0);
    atomic_set(&atomic_global.bit_operations, 0);

    pr_info("  Debug mode: %s\n", ATOMIC_DEBUG ? "enabled" : "disabled");
    pr_info("Atomic Operations Subsystem initialized.\n");
}
