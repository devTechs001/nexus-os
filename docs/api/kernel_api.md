# NEXUS-OS Kernel API Reference

## Overview

This document provides comprehensive documentation for the NEXUS-OS kernel programming interface. The kernel API is designed for kernel module developers and system programmers.

## Table of Contents

1. [Basic Types and Constants](#basic-types-and-constants)
2. [Memory Management](#memory-management)
3. [Process and Thread Management](#process-and-thread-management)
4. [Synchronization Primitives](#synchronization-primitives)
5. [Inter-Process Communication](#inter-process-communication)
6. [Interrupt Handling](#interrupt-handling)
7. [Time and Timers](#time-and-timers)
8. [Logging and Debugging](#logging-and-debugging)
9. [Device Driver Interface](#device-driver-interface)
10. [Filesystem Interface](#filesystem-interface)

---

## Basic Types and Constants

### Fundamental Types

```c
/* Integer Types */
typedef signed char         s8;      /* 8-bit signed */
typedef unsigned char       u8;      /* 8-bit unsigned */
typedef signed short        s16;     /* 16-bit signed */
typedef unsigned short      u16;     /* 16-bit unsigned */
typedef signed int          s32;     /* 32-bit signed */
typedef unsigned int        u32;     /* 32-bit unsigned */
typedef signed long long    s64;     /* 64-bit signed */
typedef unsigned long long  u64;     /* 64-bit unsigned */

/* Size and Pointer Types */
typedef u64                 size_t;
typedef s64                 ssize_t;
typedef u64                 uintptr_t;
typedef s64                 intptr_t;

/* Address Types */
typedef u64                 phys_addr_t;   /* Physical address */
typedef u64                 virt_addr_t;   /* Virtual address */
typedef u64                 dma_addr_t;    /* DMA address */

/* ID Types */
typedef u32                 pid_t;     /* Process ID */
typedef u32                 tid_t;     /* Thread ID */
typedef u32                 uid_t;     /* User ID */
typedef u32                 gid_t;     /* Group ID */
typedef u32                 mode_t;    /* File mode */
typedef s64                 time_t;    /* Time value */
```

### Result Codes

```c
typedef enum {
    NEXUS_OK        = 0,      /* Success */
    NEXUS_ERROR     = -1,     /* Generic error */
    NEXUS_ENOMEM    = -2,     /* Out of memory */
    NEXUS_EINVAL    = -3,     /* Invalid argument */
    NEXUS_ENOENT    = -4,     /* No such entry */
    NEXUS_EBUSY     = -5,     /* Device busy */
    NEXUS_EACCES    = -6,     /* Access denied */
    NEXUS_EIO       = -7,     /* I/O error */
    NEXUS_ETIMEDOUT = -8,     /* Timeout */
    NEXUS_ENOSYS    = -9,     /* Not implemented */
    NEXUS_EEXIST    = -10,    /* Already exists */
    NEXUS_EAGAIN    = -11,    /* Try again */
    NEXUS_ENODEV    = -12,    /* No such device */
    NEXUS_ENOTSUPP  = -13,    /* Not supported */
} nexus_result_t;
```

### Utility Macros

```c
/* Array and Alignment */
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#define ALIGN(x, a)         (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)    ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)    (((x) & ((a) - 1)) == 0)

/* Min/Max */
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)    MIN(MAX(x, lo), hi)

/* Bit Operations */
#define BIT(n)              (1ULL << (n))
#define BITS_PER_LONG       64
#define BITS_PER_BYTE       8

/* Size Helpers */
#define KB(x)               ((x) * 1024ULL)
#define MB(x)               ((x) * 1024ULL * 1024ULL)
#define GB(x)               ((x) * 1024ULL * 1024ULL * 1024ULL)

/* Branch Prediction */
#define likely(x)           __builtin_expect(!!(x), 1)
#define unlikely(x)         __builtin_expect(!!(x), 0)

/* Container Of */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member)); })
```

---

## Memory Management

### Kernel Allocation

```c
/**
 * kmalloc - Allocate kernel memory
 * @size: Number of bytes to allocate
 * 
 * Returns: Pointer to allocated memory, or NULL on failure
 * 
 * Note: Memory is not zeroed. Use kzalloc() for zeroed memory.
 */
void *kmalloc(size_t size);

/**
 * kzalloc - Allocate zeroed kernel memory
 * @size: Number of bytes to allocate
 * 
 * Returns: Pointer to zeroed allocated memory, or NULL on failure
 */
void *kzalloc(size_t size);

/**
 * krealloc - Reallocate kernel memory
 * @ptr: Previously allocated memory
 * @new_size: New size in bytes
 * 
 * Returns: Pointer to reallocated memory, or NULL on failure
 */
void *krealloc(void *ptr, size_t new_size);

/**
 * kfree - Free kernel memory
 * @ptr: Pointer to memory to free
 */
void kfree(void *ptr);

/**
 * vmalloc - Allocate virtually contiguous memory
 * @size: Number of bytes to allocate
 * 
 * Returns: Pointer to allocated memory, or NULL on failure
 * 
 * Note: Memory may be physically non-contiguous
 */
void *vmalloc(size_t size);

/**
 * vfree - Free vmalloc'd memory
 * @ptr: Pointer to memory to free
 */
void vfree(void *ptr);
```

### Page Allocation

```c
/**
 * alloc_pages - Allocate contiguous physical pages
 * @order: Order of allocation (2^order pages)
 * @flags: Allocation flags (GFP_*)
 * 
 * Returns: Pointer to page structure, or NULL on failure
 */
struct page *alloc_pages(gfp_t flags, unsigned int order);

/**
 * free_pages - Free contiguous physical pages
 * @addr: Virtual address of first page
 * @order: Order of allocation
 */
void free_pages(unsigned long addr, unsigned int order);

/**
 * get_zeroed_page - Allocate and zero a single page
 * @flags: Allocation flags
 * 
 * Returns: Virtual address of allocated page, or 0 on failure
 */
unsigned long get_zeroed_page(gfp_t flags);
```

### Memory Mapping

```c
/**
 * ioremap - Map physical I/O memory to virtual address
 * @phys_addr: Physical address to map
 * @size: Size of region to map
 * 
 * Returns: Virtual address, or NULL on failure
 */
void __iomem *ioremap(phys_addr_t phys_addr, size_t size);

/**
 * ioremap_wc - Map I/O memory with write-combining
 * @phys_addr: Physical address to map
 * @size: Size of region to map
 * 
 * Returns: Virtual address, or NULL on failure
 */
void __iomem *ioremap_wc(phys_addr_t phys_addr, size_t size);

/**
 * iounmap - Unmap I/O memory
 * @addr: Virtual address to unmap
 */
void iounmap(void __iomem *addr);

/**
 * memcpy_toio - Copy data to I/O memory
 * @dest: Destination I/O address
 * @src: Source address
 * @count: Number of bytes to copy
 */
void memcpy_toio(void __iomem *dest, const void *src, size_t count);

/**
 * memcpy_fromio - Copy data from I/O memory
 * @dest: Destination address
 * @src: Source I/O address
 * @count: Number of bytes to copy
 */
void memcpy_fromio(void *dest, const void __iomem *src, size_t count);
```

---

## Process and Thread Management

### Process Operations

```c
/**
 * kernel_thread - Create a kernel thread
 * @fn: Thread function
 * @arg: Argument to thread function
 * @flags: Thread flags
 * 
 * Returns: Thread ID on success, negative error on failure
 */
tid_t kernel_thread(int (*fn)(void *), void *arg, u64 flags);

/**
 * kernel_wait - Wait for thread to exit
 * @tid: Thread ID to wait for
 * @status: Optional status output
 * 
 * Returns: Thread ID on success, negative error on failure
 */
tid_t kernel_wait(tid_t tid, int *status);

/**
 * kthread_create - Create a bindable kernel thread
 * @threadfn: Thread function
 * @data: Data for thread function
 * @namefmt: Name format string
 * 
 * Returns: Thread structure, or ERR_PTR on failure
 */
struct task_struct *kthread_create(int (*threadfn)(void *data),
                                    void *data,
                                    const char *namefmt, ...);

/**
 * kthread_run - Create and start a kernel thread
 * @threadfn: Thread function
 * @data: Data for thread function
 * @namefmt: Name format string
 * 
 * Returns: Thread structure, or ERR_PTR on failure
 */
struct task_struct *kthread_run(int (*threadfn)(void *data),
                                 void *data,
                                 const char *namefmt, ...);

/**
 * kthread_stop - Stop a kernel thread
 * @thread: Thread to stop
 * 
 * Returns: Thread's exit code
 */
int kthread_stop(struct task_struct *thread);
```

### Current Process/Thread

```c
/**
 * current - Get current thread structure
 */
#define current (get_current_thread())

/**
 * current_pid - Get current process ID
 */
#define current_pid() (current->pid)

/**
 * current_tid - Get current thread ID
 */
#define current_tid() (current->tid)

/**
 * get_task_struct - Increment task reference count
 * @task: Task to reference
 */
void get_task_struct(struct task_struct *task);

/**
 * put_task_struct - Decrement task reference count
 * @task: Task to dereference
 */
void put_task_struct(struct task_struct *task);
```

---

## Synchronization Primitives

### Spinlocks

```c
/**
 * spin_lock - Acquire spinlock
 * @lock: Spinlock to acquire
 * 
 * Note: Disables interrupts on local CPU
 */
void spin_lock(spinlock_t *lock);

/**
 * spin_unlock - Release spinlock
 * @lock: Spinlock to release
 */
void spin_unlock(spinlock_t *lock);

/**
 * spin_lock_irqsave - Acquire spinlock and save IRQ state
 * @lock: Spinlock to acquire
 * @flags: Variable to save IRQ flags
 */
#define spin_lock_irqsave(lock, flags) \
    do { local_irq_save(flags); spin_lock(lock); } while(0)

/**
 * spin_unlock_irqrestore - Release spinlock and restore IRQ state
 * @lock: Spinlock to release
 * @flags: Saved IRQ flags
 */
#define spin_unlock_irqrestore(lock, flags) \
    do { spin_unlock(lock); local_irq_restore(flags); } while(0)

/**
 * spin_trylock - Try to acquire spinlock without blocking
 * @lock: Spinlock to acquire
 * 
 * Returns: true if acquired, false if already held
 */
bool spin_trylock(spinlock_t *lock);

/**
 * spin_lock_init - Initialize spinlock
 * @lock: Spinlock to initialize
 */
void spin_lock_init(spinlock_t *lock);

/* Static spinlock initialization */
#define __SPIN_LOCK_UNLOCKED(name) { .lock = 0 }
#define DEFINE_SPINLOCK(name) spinlock_t name = __SPIN_LOCK_UNLOCKED(name)
```

### Mutexes

```c
/**
 * mutex_lock - Acquire mutex (sleeping)
 * @lock: Mutex to acquire
 */
void mutex_lock(mutex_t *lock);

/**
 * mutex_unlock - Release mutex
 * @lock: Mutex to release
 */
void mutex_unlock(mutex_t *lock);

/**
 * mutex_trylock - Try to acquire mutex without sleeping
 * @lock: Mutex to acquire
 * 
 * Returns: true if acquired, false if already held
 */
bool mutex_trylock(mutex_t *lock);

/**
 * mutex_lock_interruptible - Acquire mutex (interruptible)
 * @lock: Mutex to acquire
 * 
 * Returns: 0 on success, -EINTR if interrupted
 */
int mutex_lock_interruptible(mutex_t *lock);

/**
 * mutex_init - Initialize mutex
 * @lock: Mutex to initialize
 */
void mutex_init(mutex_t *lock);

/* Static mutex initialization */
#define __MUTEX_INITIALIZER(name) { .count = ATOMIC_INIT(1) }
#define DEFINE_MUTEX(name) mutex_t name = __MUTEX_INITIALIZER(name)
```

### Read-Write Locks

```c
/**
 * read_lock - Acquire read lock
 * @lock: RW lock to acquire
 */
void read_lock(rwlock_t *lock);

/**
 * read_unlock - Release read lock
 * @lock: RW lock to release
 */
void read_unlock(rwlock_t *lock);

/**
 * write_lock - Acquire write lock
 * @lock: RW lock to acquire
 */
void write_lock(rwlock_t *lock);

/**
 * write_unlock - Release write lock
 * @lock: RW lock to release
 */
void write_unlock(rwlock_t *lock);

/**
 * read_lock_irqsave - Acquire read lock with IRQ save
 * @lock: RW lock to acquire
 * @flags: Variable to save IRQ flags
 */
#define read_lock_irqsave(lock, flags) \
    do { local_irq_save(flags); read_lock(lock); } while(0)

/**
 * write_lock_irqsave - Acquire write lock with IRQ save
 * @lock: RW lock to acquire
 * @flags: Variable to save IRQ flags
 */
#define write_lock_irqsave(lock, flags) \
    do { local_irq_save(flags); write_lock(lock); } while(0)
```

### Semaphores

```c
/**
 * down - Acquire semaphore (sleeping)
 * @sem: Semaphore to acquire
 */
void down(semaphore_t *sem);

/**
 * up - Release semaphore
 * @sem: Semaphore to release
 */
void up(semaphore_t *sem);

/**
 * down_trylock - Try to acquire semaphore without sleeping
 * @sem: Semaphore to acquire
 * 
 * Returns: 0 if acquired, -EAGAIN if not available
 */
int down_trylock(semaphore_t *sem);

/**
 * down_interruptible - Acquire semaphore (interruptible)
 * @sem: Semaphore to acquire
 * 
 * Returns: 0 on success, -EINTR if interrupted
 */
int down_interruptible(semaphore_t *sem);

/**
 * sema_init - Initialize semaphore
 * @sem: Semaphore to initialize
 * @val: Initial count
 */
void sema_init(semaphore_t *sem, int val);

/* Common initializations */
#define DEFINE_SEMAPHORE(name, val) \
    semaphore_t name = { .count = val }
#define DEFINE_MUTEX_SEM(name) \
    semaphore_t name = { .count = 1 }
```

### Atomic Operations

```c
/**
 * atomic_read - Read atomic value
 * @v: Atomic variable
 */
static inline int atomic_read(const atomic_t *v);

/**
 * atomic_set - Set atomic value
 * @v: Atomic variable
 * @i: Value to set
 */
static inline void atomic_set(atomic_t *v, int i);

/**
 * atomic_inc - Increment atomic value
 * @v: Atomic variable
 */
static inline void atomic_inc(atomic_t *v);

/**
 * atomic_dec - Decrement atomic value
 * @v: Atomic variable
 */
static inline void atomic_dec(atomic_t *v);

/**
 * atomic_dec_and_test - Decrement and test if zero
 * @v: Atomic variable
 * 
 * Returns: true if result is zero
 */
static inline bool atomic_dec_and_test(atomic_t *v);

/**
 * atomic_add - Add to atomic value
 * @i: Value to add
 * @v: Atomic variable
 */
static inline void atomic_add(int i, atomic_t *v);

/**
 * atomic_sub - Subtract from atomic value
 * @i: Value to subtract
 * @v: Atomic variable
 */
static inline void atomic_sub(int i, atomic_t *v);

/**
 * atomic_cmpxchg - Atomic compare and exchange
 * @v: Atomic variable
 * @old: Expected value
 * @new: New value
 * 
 * Returns: Old value
 */
static inline int atomic_cmpxchg(atomic_t *v, int old, int new);

/**
 * atomic_add_return - Add and return new value
 * @i: Value to add
 * @v: Atomic variable
 * 
 * Returns: New value
 */
static inline int atomic_add_return(int i, atomic_t *v);
```

---

## Inter-Process Communication

### Pipes

```c
/**
 * create_pipe_files - Create pipe file descriptors
 * @fildes: Array to store file descriptors
 * @flags: Pipe flags
 * 
 * Returns: 0 on success, negative error on failure
 */
int create_pipe_files(int fildes[2], int flags);

/**
 * pipe_read - Read from pipe
 * @pipe: Pipe structure
 * @buf: Buffer to read into
 * @count: Maximum bytes to read
 * 
 * Returns: Bytes read, or negative error
 */
ssize_t pipe_read(struct pipe_inode_info *pipe, char *buf, size_t count);

/**
 * pipe_write - Write to pipe
 * @pipe: Pipe structure
 * @buf: Buffer to write from
 * @count: Bytes to write
 * 
 * Returns: Bytes written, or negative error
 */
ssize_t pipe_write(struct pipe_inode_info *pipe, const char *buf, size_t count);
```

### Message Queues

```c
/**
 * msg_queue_create - Create message queue
 * @key: Queue key
 * @max_messages: Maximum messages
 * @max_size: Maximum queue size
 * 
 * Returns: Queue ID on success, negative error on failure
 */
int msg_queue_create(key_t key, size_t max_messages, size_t max_size);

/**
 * msg_queue_send - Send message to queue
 * @msqid: Queue ID
 * @msg: Message to send
 * @msgflg: Flags
 * 
 * Returns: 0 on success, negative error on failure
 */
int msg_queue_send(int msqid, struct msgbuf *msg, int msgflg);

/**
 * msg_queue_recv - Receive message from queue
 * @msqid: Queue ID
 * @msg: Buffer for message
 * @msgsz: Maximum message size
 * @msgtyp: Message type (0 = any)
 * @msgflg: Flags
 * 
 * Returns: Message size on success, negative error on failure
 */
int msg_queue_recv(int msqid, struct msgbuf *msg, size_t msgsz, 
                   long msgtyp, int msgflg);

/**
 * msg_queue_destroy - Destroy message queue
 * @msqid: Queue ID
 * 
 * Returns: 0 on success, negative error on failure
 */
int msg_queue_destroy(int msqid);
```

### Shared Memory

```c
/**
 * shm_create - Create shared memory segment
 * @key: Segment key
 * @size: Segment size
 * @shmflg: Flags
 * 
 * Returns: Segment ID on success, negative error on failure
 */
int shm_create(key_t key, size_t size, int shmflg);

/**
 * shm_attach - Attach to shared memory segment
 * @shmid: Segment ID
 * @shmaddr: Preferred address (NULL for auto)
 * @shmflg: Flags
 * 
 * Returns: Segment address on success, ERR_PTR on failure
 */
void *shm_attach(int shmid, void *shmaddr, int shmflg);

/**
 * shm_detach - Detach from shared memory segment
 * @shmaddr: Segment address
 * 
 * Returns: 0 on success, negative error on failure
 */
int shm_detach(void *shmaddr);

/**
 * shm_destroy - Destroy shared memory segment
 * @shmid: Segment ID
 * 
 * Returns: 0 on success, negative error on failure
 */
int shm_destroy(int shmid);
```

---

## Interrupt Handling

### IRQ Management

```c
/**
 * request_irq - Register interrupt handler
 * @irq: Interrupt number
 * @handler: Handler function
 * @flags: IRQ flags
 * @name: Handler name
 * @dev_id: Cookie for handler
 * 
 * Returns: 0 on success, negative error on failure
 */
int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *dev_id);

/**
 * free_irq - Free interrupt handler
 * @irq: Interrupt number
 * @dev_id: Cookie for handler
 */
void free_irq(unsigned int irq, void *dev_id);

/**
 * enable_irq - Enable interrupt
 * @irq: Interrupt number
 */
void enable_irq(unsigned int irq);

/**
 * disable_irq - Disable interrupt
 * @irq: Interrupt number
 */
void disable_irq(unsigned int irq);

/**
 * disable_irq_nosync - Disable interrupt without waiting
 * @irq: Interrupt number
 */
void disable_irq_nosync(unsigned int irq);

/* IRQ flags */
#define IRQF_SHARED         0x00000080
#define IRQF_TRIGGER_RISING 0x00002000
#define IRQF_TRIGGER_FALLING 0x00004000
#define IRQF_TRIGGER_HIGH   0x00008000
#define IRQF_TRIGGER_LOW    0x00010000
#define IRQF_ONESHOT        0x00000001
```

### Interrupt Control

```c
/**
 * local_irq_save - Save and disable local interrupts
 * @flags: Variable to save flags
 */
#define local_irq_save(flags) \
    do { flags = arch_local_irq_save(); } while(0)

/**
 * local_irq_restore - Restore local interrupt state
 * @flags: Saved flags
 */
#define local_irq_restore(flags) \
    do { arch_local_irq_restore(flags); } while(0)

/**
 * local_irq_disable - Disable local interrupts
 */
#define local_irq_disable() \
    do { arch_local_irq_disable(); } while(0)

/**
 * local_irq_enable - Enable local interrupts
 */
#define local_irq_enable() \
    do { arch_local_irq_enable(); } while(0)
```

---

## Time and Timers

### Time Functions

```c
/**
 * get_ticks - Get current tick count
 * 
 * Returns: Current tick count since boot
 */
u64 get_ticks(void);

/**
 * get_time_ns - Get current time in nanoseconds
 * 
 * Returns: Current time in nanoseconds since boot
 */
u64 get_time_ns(void);

/**
 * get_time_ms - Get current time in milliseconds
 * 
 * Returns: Current time in milliseconds since boot
 */
u64 get_time_ms(void);

/**
 * get_realtime - Get wall-clock time
 * @ts: Timespec structure to fill
 * 
 * Returns: 0 on success, negative error on failure
 */
int get_realtime(struct timespec *ts);

/**
 * get_monotonic - Get monotonic time
 * @ts: Timespec structure to fill
 * 
 * Returns: 0 on success, negative error on failure
 */
int get_monotonic(struct timespec *ts);
```

### Delays

```c
/**
 * delay_ms - Busy wait for milliseconds
 * @ms: Milliseconds to wait
 */
void delay_ms(u64 ms);

/**
 * delay_us - Busy wait for microseconds
 * @us: Microseconds to wait
 */
void delay_us(u64 us);

/**
 * msleep - Sleep for milliseconds (interruptible)
 * @ms: Milliseconds to sleep
 */
void msleep(unsigned int ms);

/**
 * usleep_range - Sleep for microseconds range
 * @min: Minimum microseconds
 * @max: Maximum microseconds
 */
void usleep_range(unsigned long min, unsigned long max);
```

### Timers

```c
/**
 * timer_setup - Set up timer
 * @timer: Timer structure
 * @callback: Callback function
 * @flags: Timer flags
 */
void timer_setup(struct timer_list *timer,
                 void (*callback)(struct timer_list *),
                 unsigned int flags);

/**
 * mod_timer - Modify timer expiration
 * @timer: Timer to modify
 * @expires: New expiration time (jiffies)
 * 
 * Returns: 0 if timer was pending, 1 if already expired
 */
int mod_timer(struct timer_list *timer, unsigned long expires);

/**
 * del_timer - Delete timer
 * @timer: Timer to delete
 * 
 * Returns: 1 if timer was pending, 0 if already expired/inactive
 */
int del_timer(struct timer_list *timer);

/**
 * del_timer_sync - Delete timer and wait for callback completion
 * @timer: Timer to delete
 * 
 * Returns: 1 if timer was pending, 0 if already expired/inactive
 */
int del_timer_sync(struct timer_list *timer);

/* Timer macros */
#define TIMER_DEFERRED      0x00000001

/* Jiffies conversion */
#define msecs_to_jiffies(m) ((m) * HZ / 1000)
#define secs_to_jiffies(s)  ((s) * HZ)
```

---

## Logging and Debugging

### Printk

```c
/**
 * printk - Print kernel message
 * @fmt: Format string
 * 
 * Returns: Number of characters printed
 */
int printk(const char *fmt, ...);

/* Log levels */
#define pr_emerg(fmt, ...)   printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)   printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)    printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)     printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)    printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)  printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)    printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)   printk(KERN_DEBUG fmt, ##__VA_ARGS__)

/* Convenience macros */
#define pr_cont(fmt, ...)    printk(KERN_CONT fmt, ##__VA_ARGS__)
#define pr_once(fmt, ...)    printk_once(fmt, ##__VA_ARGS__)
#define pr_ratelimited(fmt, ...) printk_ratelimited(fmt, ##__VA_ARGS__)
```

### Assertions and Bugs

```c
/**
 * BUG - Kernel bug (panic)
 * 
 * Note: Does not return
 */
void BUG(void) __noreturn;

/**
 * BUG_ON - Panic if condition is true
 * @condition: Condition to check
 */
#define BUG_ON(condition) \
    do { if (unlikely(condition)) BUG(); } while(0)

/**
 * WARN - Print warning
 * @condition: Condition
 * @fmt: Format string
 * 
 * Returns: condition value
 */
#define WARN(condition, fmt, ...) ({              \
    bool __ret = (condition);                     \
    if (unlikely(__ret))                          \
        pr_warn(fmt, ##__VA_ARGS__);              \
    __ret;                                        \
})

/**
 * WARN_ON - Warn if condition is true
 * @condition: Condition to check
 * 
 * Returns: condition value
 */
#define WARN_ON(condition) WARN(condition, "WARN_ON at %s:%d\n", \
                                  __FILE__, __LINE__)

/**
 * ASSERT - Debug assertion
 * @expr: Expression to check
 */
#ifdef CONFIG_DEBUG
#define ASSERT(expr) \
    do { if (unlikely(!(expr))) \
        kernel_assert_failed(#expr, __FILE__, __LINE__); \
    } while(0)
#else
#define ASSERT(expr) ((void)0)
#endif
```

---

## Device Driver Interface

### Device Registration

```c
/**
 * device_register - Register device
 * @dev: Device structure
 * 
 * Returns: 0 on success, negative error on failure
 */
int device_register(struct device *dev);

/**
 * device_unregister - Unregister device
 * @dev: Device structure
 */
void device_unregister(struct device *dev);

/**
 * device_create - Create device
 * @class: Device class
 * @parent: Parent device
 * @devt: Device number
 * @drvdata: Driver data
 * @fmt: Name format
 * 
 * Returns: Device pointer, or ERR_PTR on failure
 */
struct device *device_create(struct class *class, struct device *parent,
                             dev_t devt, void *drvdata,
                             const char *fmt, ...);

/**
 * device_destroy - Destroy device
 * @class: Device class
 * @devt: Device number
 */
void device_destroy(struct class *class, dev_t devt);
```

### Character Devices

```c
/**
 * register_chrdev_region - Register character device region
 * @first: First device number
 * @count: Number of devices
 * @name: Device name
 * 
 * Returns: 0 on success, negative error on failure
 */
int register_chrdev_region(dev_t first, unsigned int count, const char *name);

/**
 * unregister_chrdev_region - Unregister character device region
 * @first: First device number
 * @count: Number of devices
 */
void unregister_chrdev_region(dev_t first, unsigned int count);

/**
 * cdev_init - Initialize character device
 * @cdev: Character device structure
 * @fops: File operations
 */
void cdev_init(struct cdev *cdev, const struct file_operations *fops);

/**
 * cdev_add - Add character device
 * @cdev: Character device structure
 * @dev: Device number
 * @count: Device count
 * 
 * Returns: 0 on success, negative error on failure
 */
int cdev_add(struct cdev *cdev, dev_t dev, unsigned int count);

/**
 * cdev_del - Delete character device
 * @cdev: Character device structure
 */
void cdev_del(struct cdev *cdev);
```

### Platform Devices

```c
/**
 * platform_device_register - Register platform device
 * @pdev: Platform device
 * 
 * Returns: 0 on success, negative error on failure
 */
int platform_device_register(struct platform_device *pdev);

/**
 * platform_device_unregister - Unregister platform device
 * @pdev: Platform device
 */
void platform_device_unregister(struct platform_device *pdev);

/**
 * platform_get_resource - Get platform resource
 * @pdev: Platform device
 * @type: Resource type
 * @num: Resource number
 * 
 * Returns: Resource pointer, or NULL if not found
 */
struct resource *platform_get_resource(struct platform_device *pdev,
                                       unsigned int type,
                                       unsigned int num);

/**
 * platform_get_irq - Get platform IRQ
 * @pdev: Platform device
 * @num: IRQ number
 * 
 * Returns: IRQ number, or negative error
 */
int platform_get_irq(struct platform_device *pdev, unsigned int num);
```

---

## Filesystem Interface

### VFS Operations

```c
/**
 * vfs_open - Open file
 * @path: Path to file
 * @file: File structure
 * 
 * Returns: 0 on success, negative error on failure
 */
int vfs_open(const struct path *path, struct file *file);

/**
 * vfs_close - Close file
 * @file: File structure
 * 
 * Returns: 0 on success, negative error on failure
 */
int vfs_close(struct file *file);

/**
 * vfs_read - Read from file
 * @file: File structure
 * @buf: Buffer to read into
 * @count: Bytes to read
 * @pos: File position
 * 
 * Returns: Bytes read, or negative error
 */
ssize_t vfs_read(struct file *file, char *buf, size_t count, loff_t *pos);

/**
 * vfs_write - Write to file
 * @file: File structure
 * @buf: Buffer to write from
 * @count: Bytes to write
 * @pos: File position
 * 
 * Returns: Bytes written, or negative error
 */
ssize_t vfs_write(struct file *file, const char *buf, size_t count, loff_t *pos);

/**
 * vfs_ioctl - IOCTL on file
 * @file: File structure
 * @cmd: Command
 * @arg: Argument
 * 
 * Returns: Result, or negative error
 */
long vfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
```

### Path Operations

```c
/**
 * kern_path - Lookup path
 * @name: Path name
 * @flags: Lookup flags
 * @path: Path structure
 * 
 * Returns: 0 on success, negative error on failure
 */
int kern_path(const char *name, unsigned int flags, struct path *path);

/**
 * path_put - Release path reference
 * @path: Path structure
 */
void path_put(struct path *path);

/**
 * vfs_create - Create file
 * @dir: Directory inode
 * @dentry: Dentry for new file
 * @mode: File mode
 * @lookup_flags: Lookup flags
 * 
 * Returns: 0 on success, negative error on failure
 */
int vfs_create(struct inode *dir, struct dentry *dentry,
               umode_t mode, bool lookup_flags);

/**
 * vfs_mkdir - Create directory
 * @dir: Parent directory inode
 * @dentry: Dentry for new directory
 * @mode: Directory mode
 * 
 * Returns: 0 on success, negative error on failure
 */
int vfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

/**
 * vfs_unlink - Unlink file
 * @dir: Directory inode
 * @dentry: Dentry to unlink
 * @delegated_inode: Delegated inode (optional)
 * 
 * Returns: 0 on success, negative error on failure
 */
int vfs_unlink(struct inode *dir, struct dentry *dentry,
               struct inode **delegated_inode);
```

---

## Module Interface

### Module Registration

```c
/**
 * module_init - Module initialization macro
 * @initfunc: Initialization function
 * 
 * Usage: module_init(my_init_function);
 */
#define module_init(initfunc) \
    static inline initcall_t __module_init(void) \
    { return initfunc; }

/**
 * module_exit - Module exit macro
 * @exitfunc: Exit function
 * 
 * Usage: module_exit(my_exit_function);
 */
#define module_exit(exitfunc) \
    static inline exitcall_t __module_exit(void) \
    { return exitfunc; }

/**
 * MODULE_LICENSE - Module license
 * @license: License string
 * 
 * Usage: MODULE_LICENSE("GPL");
 */
#define MODULE_LICENSE(license) \
    static const char __module_license[] = license

/**
 * MODULE_AUTHOR - Module author
 * @author: Author string
 */
#define MODULE_AUTHOR(author) \
    static const char __module_author[] = author

/**
 * MODULE_DESCRIPTION - Module description
 * @description: Description string
 */
#define MODULE_DESCRIPTION(description) \
    static const char __module_description[] = description

/**
 * MODULE_VERSION - Module version
 * @version: Version string
 */
#define MODULE_VERSION(version) \
    static const char __module_version[] = version
```

### Module Parameters

```c
/**
 * module_param - Module parameter
 * @name: Parameter name
 * @type: Parameter type
 * @perm: Permission bits
 * 
 * Usage: module_param(my_param, int, 0644);
 */
#define module_param(name, type, perm) \
    module_param_named(name, name, type, perm)

/**
 * module_param_named - Named module parameter
 * @name: Parameter name in sysfs
 * @var: Variable name
 * @type: Parameter type
 * @perm: Permission bits
 */
#define module_param_named(name, var, type, perm) \
    /* Implementation defined */

/* Parameter types */
/* int, uint, long, ulong, charp, bool, invbool */
```

---

*Last Updated: March 2026*
*NEXUS-OS Version 1.0.0-alpha*
