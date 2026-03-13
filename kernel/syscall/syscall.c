/*
 * NEXUS OS - System Call Infrastructure
 * kernel/syscall/syscall.c
 *
 * Main syscall handling infrastructure including entry/exit,
 * validation, and dispatch mechanisms.
 */

#include "syscall.h"
#include "../sched/sched.h"
#include "../mm/mm.h"

/* External syscall table from syscall_table.c */
extern struct syscall_entry syscall_table[NR_SYSCALLS];

/*===========================================================================*/
/*                         SYSCALL STATISTICS                                */
/*===========================================================================*/

/* Per-syscall invocation counters */
struct syscall_stat_entry {
    atomic_t count;
    u64 total_time_ns;
    u64 min_time_ns;
    u64 max_time_ns;
};
static struct syscall_stat_entry syscall_stats[NR_SYSCALLS];

/* Global syscall statistics */
static struct {
    atomic_t total_calls;
    atomic_t failed_calls;
    u64 start_time;
} syscall_global_stats;

/*===========================================================================*/
/*                         USER SPACE MEMORY ACCESS                          */
/*===========================================================================*/

/**
 * access_ok - Check if user space address is valid
 * @addr: User space address to check
 * @size: Size of memory region
 *
 * Validates that the given user space address range is accessible
 * from kernel space. This is a basic check; actual page fault
 * handling provides the real protection.
 *
 * Returns: true if address is valid, false otherwise
 */
bool access_ok(const void *addr, size_t size)
{
    /* Check for NULL pointer */
    if (!addr) {
        return false;
    }

    /* Check for kernel space addresses */
    if ((u64)addr >= KERNEL_VIRTUAL_BASE) {
        return false;
    }

    /* Check for wraparound */
    if ((u64)addr + size < (u64)addr) {
        return false;
    }

    /* Check if address + size exceeds user space limit */
    /* For x86_64, user space is typically 0x0000000000000000 to 0x00007FFFFFFFFFFF */
    if ((u64)addr + size > 0x00007FFFFFFFFFFFULL) {
        return false;
    }

    return true;
}

/**
 * __copy_from_user - Internal function to copy from user space
 * @to: Destination kernel buffer
 * @from: Source user buffer
 * @n: Number of bytes to copy
 *
 * Internal implementation without validation. Caller must ensure
 * addresses are valid.
 *
 * Returns: 0 on success, -EFAULT on failure
 */
static s64 __copy_from_user(void *to, const void *from, size_t n)
{
    if (!to || !from || n == 0) {
        return -EFAULT;
    }

    /* In a real implementation, this would use architecture-specific
     * instructions with exception handling for page faults */
    memcpy(to, from, n);

    return 0;
}

/**
 * copy_from_user - Copy data from user space to kernel space
 * @to: Destination kernel buffer
 * @from: Source user buffer
 * @n: Number of bytes to copy
 *
 * Safely copies data from user space to kernel space with
 * proper validation and error handling.
 *
 * Returns: 0 on success, -EFAULT on failure
 */
s64 copy_from_user(void *to, const void *from, size_t n)
{
    if (!access_ok(from, n)) {
        return -EFAULT;
    }

    return __copy_from_user(to, from, n);
}

/**
 * __copy_to_user - Internal function to copy to user space
 * @to: Destination user buffer
 * @from: Source kernel buffer
 * @n: Number of bytes to copy
 *
 * Internal implementation without validation. Caller must ensure
 * addresses are valid.
 *
 * Returns: 0 on success, -EFAULT on failure
 */
static s64 __copy_to_user(void *to, const void *from, size_t n)
{
    if (!to || !from || n == 0) {
        return -EFAULT;
    }

    /* In a real implementation, this would use architecture-specific
     * instructions with exception handling for page faults */
    memcpy(to, from, n);

    return 0;
}

/**
 * copy_to_user - Copy data from kernel space to user space
 * @to: Destination user buffer
 * @from: Source kernel buffer
 * @n: Number of bytes to copy
 *
 * Safely copies data from kernel space to user space with
 * proper validation and error handling.
 *
 * Returns: 0 on success, -EFAULT on failure
 */
s64 copy_to_user(void *to, const void *from, size_t n)
{
    if (!access_ok(to, n)) {
        return -EFAULT;
    }

    return __copy_to_user(to, from, n);
}

/**
 * strncpy_from_user - Copy string from user space
 * @dest: Destination kernel buffer
 * @src: Source user string
 * @count: Maximum number of bytes to copy
 *
 * Copies a null-terminated string from user space to kernel space,
 * ensuring the destination is null-terminated.
 *
 * Returns: Number of bytes copied (excluding null), -EFAULT on error
 */
s64 strncpy_from_user(char *dest, const char *src, size_t count)
{
    char c;
    size_t i;

    if (!access_ok(src, 1)) {
        return -EFAULT;
    }

    if (!dest || !src || count == 0) {
        return -EFAULT;
    }

    for (i = 0; i < count; i++) {
        if (__copy_from_user(&c, src + i, 1) != 0) {
            return -EFAULT;
        }

        dest[i] = c;

        if (c == '\0') {
            return i;
        }
    }

    /* Ensure null termination */
    if (i < count) {
        dest[i] = '\0';
    }

    return count;
}

/**
 * put_user - Write value to user space
 * @x: Value to write
 * @ptr: User space pointer
 *
 * Writes a value to user space with proper validation.
 *
 * Returns: 0 on success, -EFAULT on failure
 */
s64 put_user(u64 x, u64 *ptr)
{
    if (!access_ok(ptr, sizeof(u64))) {
        return -EFAULT;
    }

    return __copy_to_user(ptr, &x, sizeof(u64));
}

/**
 * get_user - Read value from user space
 * @x: Pointer to store value
 * @ptr: User space pointer
 *
 * Reads a value from user space with proper validation.
 *
 * Returns: 0 on success, -EFAULT on failure
 */
s64 get_user(u64 *x, const u64 *ptr)
{
    u64 val;

    if (!access_ok(ptr, sizeof(u64))) {
        return -EFAULT;
    }

    if (__copy_from_user(&val, ptr, sizeof(u64)) != 0) {
        return -EFAULT;
    }

    *x = val;
    return 0;
}

/*===========================================================================*/
/*                         SYSCALL VALIDATION                                */
/*===========================================================================*/

/**
 * syscall_validate_args - Validate syscall arguments
 * @args: Syscall arguments
 * @nargs: Expected number of arguments
 *
 * Performs basic validation of syscall arguments.
 *
 * Returns: 0 if valid, -EINVAL if invalid
 */
static s64 syscall_validate_args(struct syscall_args *args, u32 nargs)
{
    if (!args) {
        return -EINVAL;
    }

    /* Check syscall number */
    if (args->nr >= NR_SYSCALLS) {
        return -ENOSYS;
    }

    /* Validate pointer arguments based on syscall number */
    /* This is a simplified check; real implementation would be more thorough */

    return 0;
}

/**
 * syscall_validate_ptr - Validate a user space pointer argument
 * @ptr: Pointer to validate
 * @size: Expected size of object
 * @flags: Validation flags
 *
 * Validation flags:
 *   0x01 - Must be readable
 *   0x02 - Must be writable
 *   0x04 - Must be aligned
 *
 * Returns: 0 if valid, -EFAULT if invalid
 */
static s64 syscall_validate_ptr(const void *ptr, size_t size, u32 flags)
{
    if (!ptr) {
        return -EFAULT;
    }

    if (!access_ok(ptr, size)) {
        return -EFAULT;
    }

    if ((flags & 0x04) && !IS_ALIGNED((u64)ptr, sizeof(u64))) {
        return -EFAULT;
    }

    return 0;
}

/*===========================================================================*/
/*                         SYSCALL DISPATCH                                  */
/*===========================================================================*/

/**
 * syscall_enter - Syscall entry hook
 * @args: Syscall arguments
 *
 * Called when entering a syscall. Performs accounting and tracing.
 */
static void syscall_enter(struct syscall_args *args)
{
    u64 nr = args->nr;

    if (nr < NR_SYSCALLS) {
        atomic_inc(&syscall_stats[nr].count);
        atomic_inc(&syscall_global_stats.total_calls);
    }
}

/**
 * syscall_exit - Syscall exit hook
 * @args: Syscall arguments
 * @ret: Syscall return value
 * @duration_ns: Syscall duration in nanoseconds
 *
 * Called when exiting a syscall. Performs accounting and tracing.
 */
void syscall_exit(struct syscall_args *args, s64 ret, u64 duration_ns)
{
    u64 nr = args->nr;

    if (nr < NR_SYSCALLS) {
        /* Update timing statistics */
        syscall_stats[nr].total_time_ns += duration_ns;

        if (duration_ns < syscall_stats[nr].min_time_ns ||
            syscall_stats[nr].min_time_ns == 0) {
            syscall_stats[nr].min_time_ns = duration_ns;
        }

        if (duration_ns > syscall_stats[nr].max_time_ns) {
            syscall_stats[nr].max_time_ns = duration_ns;
        }

        /* Track failed calls */
        if (ret < 0) {
            atomic_inc(&syscall_global_stats.failed_calls);
        }
    }
}

/**
 * syscall_dispatch - Dispatch system call to handler
 * @frame: CPU register state from syscall entry
 *
 * Main syscall dispatch function. Extracts arguments from the
 * register frame and calls the appropriate handler.
 *
 * Returns: Syscall return value
 */
s64 syscall_dispatch(struct syscall_frame *frame)
{
    struct syscall_args args;
    s64 ret;
    u64 start_time;

    if (!frame) {
        return -EFAULT;
    }

    /* Extract syscall number and arguments from frame */
    args.nr = frame->rax;
    args.arg1 = frame->rdi;
    args.arg2 = frame->rsi;
    args.arg3 = frame->rdx;
    args.arg4 = frame->r10;
    args.arg5 = frame->r8;
    args.arg6 = frame->r9;

    /* Validate syscall number */
    if (args.nr >= NR_SYSCALLS) {
        pr_debug("Invalid syscall number: %lu\n", args.nr);
        return -ENOSYS;
    }

    /* Get handler from syscall table */
    syscall_handler_t handler = syscall_get_handler(args.nr);
    if (!handler) {
        pr_debug("Unimplemented syscall: %lu (%s)\n",
                 args.nr, syscall_get_name(args.nr));
        return -ENOSYS;
    }

    /* Entry hook */
    syscall_enter(&args);
    start_time = get_ticks();

    /* Call handler */
    ret = handler(&args);

    /* Exit hook */
    syscall_exit(&args, ret, (get_ticks() - start_time) * NS_PER_TICK);

    return ret;
}

/**
 * syscall_handler - Alternative syscall handler interface
 * @args: Syscall arguments structure
 *
 * Direct syscall handler that takes arguments structure.
 * Used by architecture-specific entry code.
 *
 * Returns: Syscall return value
 */
s64 syscall_handler(struct syscall_args *args)
{
    s64 ret;
    u64 start_time;

    if (!args) {
        return -EFAULT;
    }

    /* Validate syscall number */
    if (args->nr >= NR_SYSCALLS) {
        return -ENOSYS;
    }

    /* Get handler from syscall table */
    syscall_handler_t handler = syscall_get_handler(args->nr);
    if (!handler) {
        return -ENOSYS;
    }

    /* Entry hook */
    syscall_enter(args);
    start_time = get_ticks();

    /* Call handler */
    ret = handler(args);

    /* Exit hook */
    syscall_exit(args, ret, (get_ticks() - start_time) * NS_PER_TICK);

    return ret;
}

/*===========================================================================*/
/*                         SYSCALL REGISTRATION                              */
/*===========================================================================*/

/**
 * syscall_register - Register a syscall handler
 * @nr: Syscall number
 * @handler: Handler function
 * @name: Syscall name (for debugging)
 * @nargs: Number of arguments
 *
 * Registers a handler for the specified syscall number.
 *
 * Returns: 0 on success, negative error code on failure
 */
s64 syscall_register(u64 nr, syscall_handler_t handler, const char *name, u32 nargs)
{
    if (nr >= NR_SYSCALLS) {
        return -EINVAL;
    }

    if (!handler) {
        return -EINVAL;
    }

    syscall_table[nr].handler = handler;
    syscall_table[nr].name = name;
    syscall_table[nr].nargs = nargs;
    syscall_table[nr].flags = SYSCALL_F_NATIVE;

    pr_debug("Registered syscall %lu: %s\n", nr, name);

    return 0;
}

/**
 * syscall_unregister - Unregister a syscall handler
 * @nr: Syscall number
 *
 * Unregisters the handler for the specified syscall number.
 *
 * Returns: 0 on success, negative error code on failure
 */
s64 syscall_unregister(u64 nr)
{
    if (nr >= NR_SYSCALLS) {
        return -EINVAL;
    }

    syscall_table[nr].handler = NULL;
    syscall_table[nr].name = NULL;
    syscall_table[nr].nargs = 0;
    syscall_table[nr].flags = 0;

    return 0;
}

/*===========================================================================*/
/*                         SYSCALL STATISTICS                                */
/*===========================================================================*/

/**
 * syscall_get_count - Get invocation count for syscall
 * @nr: Syscall number
 *
 * Returns: Number of times syscall has been called
 */
u64 syscall_get_count(u64 nr)
{
    if (nr >= NR_SYSCALLS) {
        return 0;
    }

    return atomic_read(&syscall_stats[nr].count);
}

/**
 * syscall_reset_count - Reset invocation count for syscall
 * @nr: Syscall number
 */
void syscall_reset_count(u64 nr)
{
    if (nr >= NR_SYSCALLS) {
        return;
    }

    atomic_set(&syscall_stats[nr].count, 0);
    syscall_stats[nr].total_time_ns = 0;
    syscall_stats[nr].min_time_ns = 0;
    syscall_stats[nr].max_time_ns = 0;
}

/**
 * syscall_stats - Print syscall statistics
 *
 * Displays invocation counts and timing information for all syscalls.
 */
void syscall_stats_print(void)
{
    u64 i;
    u64 total;

    printk("\n=== Syscall Statistics ===\n");
    printk("Total Calls: %d\n", atomic_read(&syscall_global_stats.total_calls));
    printk("Failed Calls: %d\n", atomic_read(&syscall_global_stats.failed_calls));
    printk("\n%-6s %-24s %12s %12s %12s %12s\n",
           "NR", "Name", "Calls", "Min(ns)", "Max(ns)", "Avg(ns)");
    printk("------------------------------------------------------------------------\n");

    total = 0;
    for (i = 0; i < NR_SYSCALLS; i++) {
        u64 count = atomic_read(&syscall_stats[i].count);
        if (count > 0) {
            u64 avg = syscall_stats[i].total_time_ns / count;
            const char *name = syscall_get_name(i);

            printk("%-6lu %-24s %12lu %12lu %12lu %12lu\n",
                   i, name ? name : "unknown",
                   count,
                   syscall_stats[i].min_time_ns,
                   syscall_stats[i].max_time_ns,
                   avg);

            total += count;
        }
    }

    printk("\nTotal: %lu syscalls\n", total);
}

/*===========================================================================*/
/*                         SYSCALL INITIALIZATION                            */
/*===========================================================================*/

/**
 * syscall_init - Initialize syscall subsystem
 *
 * Initializes the syscall infrastructure including statistics
 * and the syscall table.
 */
void syscall_init(void)
{
    u64 i;

    pr_info("Initializing System Call Interface...\n");

    /* Initialize statistics */
    for (i = 0; i < NR_SYSCALLS; i++) {
        atomic_set(&syscall_stats[i].count, 0);
        syscall_stats[i].total_time_ns = 0;
        syscall_stats[i].min_time_ns = 0;
        syscall_stats[i].max_time_ns = 0;
    }

    atomic_set(&syscall_global_stats.total_calls, 0);
    atomic_set(&syscall_global_stats.failed_calls, 0);
    syscall_global_stats.start_time = get_time_ms();

    /* Initialize syscall table */
    syscall_table_init();

    pr_info("System Call Interface initialized.\n");
    pr_info("  Total syscalls: %d\n", NR_SYSCALLS);
}

/*===========================================================================*/
/*                         ARCHITECTURE INTERFACES                           */
/*===========================================================================*/

/*
 * These functions are implemented in architecture-specific code:
 *
 * - syscall_entry: Assembly entry point for syscalls
 * - syscall_exit: Assembly exit point for syscalls
 *
 * For x86_64, these would use the SYSCALL/SYSRET instructions
 * or the legacy INT 0x80 interface.
 */

extern void syscall_entry_asm(void);
extern void syscall_exit_asm(void);
