/*
 * NEXUS OS - Thread Management
 * kernel/sched/thread.c
 */

#include "../include/kernel.h"
#include "sched.h"

/*===========================================================================*/
/*                         THREAD MANAGEMENT DATA                            */
/*===========================================================================*/

/* Thread-local storage key management */
#define TLS_MAX_KEYS        1024
#define TLS_KEY_FREE        0xFFFFFFFF

typedef struct tls_key {
    void (*destructor)(void *);
    bool in_use;
} tls_key_t;

static struct {
    tls_key_t keys[TLS_MAX_KEYS];
    spinlock_t key_lock;
    u32 next_key;
} tls_manager = {
    .key_lock = { .lock = 0 },
    .next_key = 1
};

/* Thread group management */
static spinlock_t thread_group_lock = { .lock = 0 };

/* User thread support */
static struct {
    bool enabled;
    u32 max_user_threads;
    u32 active_user_threads;
    spinlock_t lock;
} user_thread_manager = {
    .enabled = true,
    .max_user_threads = 10000,
    .active_user_threads = 0,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         THREAD CREATION                                   */
/*===========================================================================*/

/**
 * thread_create - Create a new thread
 * @name: Thread name
 * @fn: Thread entry function
 * @arg: Argument to pass to thread function
 * @stack_size: Stack size (0 for default)
 *
 * Creates a new kernel thread with the specified entry point.
 * The thread shares the address space of the calling process.
 *
 * Returns: Pointer to new task_struct (thread), or NULL on failure
 */
struct task_struct *thread_create(const char *name, void (*fn)(void *),
                                   void *arg, size_t stack_size)
{
    struct task_struct *thread;
    struct task_struct *parent = current;
    int ret;

    if (!fn) {
        pr_err("Thread creation failed: NULL entry function\n");
        return NULL;
    }

    /* Default stack size */
    if (stack_size == 0) {
        stack_size = KERNEL_STACK_SIZE;
    }

    /* Create new task */
    thread = alloc_task_struct();
    if (!thread) {
        return NULL;
    }

    /* Allocate PID and TID */
    thread->pid = parent->tgid;  /* Same process ID as parent */
    thread->tgid = parent->tgid;
    thread->tid = allocate_tid();

    /* Set thread properties */
    thread->flags = PF_KTHREAD;
    thread->uid = parent->uid;
    thread->gid = parent->gid;

    /* Set name */
    if (name) {
        strncpy(thread->comm, name, sizeof(thread->comm) - 1);
        thread->comm[sizeof(thread->comm) - 1] = '\0';
    } else {
        snprintf(thread->comm, sizeof(thread->comm), "thread-%u", thread->tid);
    }

    /* Inherit priority from parent */
    thread->prio = parent->prio;
    thread->static_prio = parent->static_prio;
    thread->normal_prio = parent->normal_prio;
    thread->rt_priority = parent->rt_priority;
    thread->policy = parent->policy;

    /* Set CPU affinity */
    thread->cpu = parent->cpu;
    thread->last_cpu = parent->cpu;
    thread->cpus_allowed = parent->cpus_allowed;

    /* Allocate stack */
    thread->stack = kzalloc(stack_size);
    if (!thread->stack) {
        pr_err("Failed to allocate thread stack\n");
        free_tid(thread->tid);
        kfree(thread);
        return NULL;
    }
    thread->thread_info = thread->stack;

    /* Share memory space with parent */
    thread->mm = parent->mm;
    thread->active_mm = parent->active_mm;

    /* Initialize architecture-specific thread data */
    ret = arch_init_thread(thread, parent, fn, arg);
    if (ret != 0) {
        pr_err("Failed to initialize thread arch data\n");
        kfree(thread->stack);
        free_tid(thread->tid);
        kfree(thread);
        return NULL;
    }

    /* Set parent */
    thread->parent = parent;

    /* Add to parent's children list */
    spin_lock(&thread_group_lock);
    list_add_tail(&thread->sibling, &parent->children);
    spin_unlock(&thread_group_lock);

    /* Insert into hash table */
    task_hash_insert(thread);

    /* Add to global task list */
    spin_lock(&task_list_lock);
    list_add_tail(&thread->run_list, &task_list);
    spin_unlock(&task_list_lock);

    /* Update thread count */
    atomic_inc(&thread_count);

    /* Set initial state */
    thread->state = TASK_NEW;

    pr_debug("Thread created: %s (PID: %u, TID: %u)\n",
             thread->comm, thread->pid, thread->tid);

    return thread;
}

/**
 * thread_destroy - Destroy a thread
 * @thread: Thread to destroy
 *
 * Destroys a thread and releases its resources.
 * The thread must be in TASK_DEAD or TASK_EXITING state.
 */
void thread_destroy(struct task_struct *thread)
{
    if (!thread) {
        return;
    }

    /* Must be exiting or dead */
    if (!(thread->state & (TASK_EXITING | TASK_DEAD))) {
        pr_warn("Attempting to destroy non-exiting thread %u\n", thread->tid);
        return;
    }

    /* Remove from parent's children list */
    spin_lock(&thread_group_lock);
    if (!list_empty(&thread->sibling)) {
        list_del(&thread->sibling);
    }
    spin_unlock(&thread_group_lock);

    /* Remove from hash table */
    task_hash_remove(thread);

    /* Remove from global task list */
    spin_lock(&task_list_lock);
    list_del(&thread->run_list);
    spin_unlock(&task_list_lock);

    /* Free TID */
    free_tid(thread->tid);

    /* Update thread count */
    atomic_dec(&thread_count);

    /* Free resources */
    free_task_struct(thread);

    pr_debug("Thread destroyed: TID %u\n", thread->tid);
}

/**
 * thread_exit - Exit current thread
 * @ret: Return value
 *
 * Exits the current thread with the specified return value.
 */
void thread_exit(void *ret)
{
    struct task_struct *thread = current;

    pr_debug("Thread exiting: %s (TID: %u)\n", thread->comm, thread->tid);

    /* Store return value in thread-specific area */
    thread->exit_code = (int)(uintptr_t)ret;

    /* Mark as exiting */
    do_exit(thread, (int)(uintptr_t)ret);
}

/*===========================================================================*/
/*                         THREAD-LOCAL STORAGE                              */
/*===========================================================================*/

/**
 * tls_key_create - Create a new TLS key
 * @destructor: Destructor function called when thread exits
 *
 * Creates a new thread-local storage key that can be used to
 * store thread-specific data.
 *
 * Returns: Key ID, or TLS_KEY_FREE on failure
 */
u32 tls_key_create(void (*destructor)(void *))
{
    u32 key;

    spin_lock(&tls_manager.key_lock);

    /* Find free key */
    key = tls_manager.next_key;

    /* Check if key is available */
    if (key >= TLS_MAX_KEYS || tls_manager.keys[key].in_use) {
        /* Search for free key */
        for (key = 1; key < TLS_MAX_KEYS; key++) {
            if (!tls_manager.keys[key].in_use) {
                break;
            }
        }

        if (key >= TLS_MAX_KEYS) {
            spin_unlock(&tls_manager.key_lock);
            pr_err("TLS: No free keys available\n");
            return TLS_KEY_FREE;
        }
    }

    /* Mark key as in use */
    tls_manager.keys[key].in_use = true;
    tls_manager.keys[key].destructor = destructor;

    /* Update next key hint */
    tls_manager.next_key = (key + 1) % TLS_MAX_KEYS;
    if (tls_manager.next_key == 0) {
        tls_manager.next_key = 1;
    }

    spin_unlock(&tls_manager.key_lock);

    pr_debug("TLS key created: %u\n", key);

    return key;
}

/**
 * tls_key_delete - Delete a TLS key
 * @key: Key to delete
 *
 * Deletes a previously created TLS key. All thread-local data
 * associated with this key is freed using the destructor.
 *
 * Returns: 0 on success, -EINVAL on failure
 */
int tls_key_delete(u32 key)
{
    if (key == 0 || key >= TLS_MAX_KEYS) {
        return -EINVAL;
    }

    spin_lock(&tls_manager.key_lock);

    if (!tls_manager.keys[key].in_use) {
        spin_unlock(&tls_manager.key_lock);
        return -EINVAL;
    }

    /* Mark key as free */
    tls_manager.keys[key].in_use = false;
    tls_manager.keys[key].destructor = NULL;

    spin_unlock(&tls_manager.key_lock);

    pr_debug("TLS key deleted: %u\n", key);

    return 0;
}

/**
 * tls_set_value - Set thread-local value for a key
 * @key: TLS key
 * @value: Value to set
 *
 * Sets the thread-local value for the specified key.
 * Each thread has its own independent value for each key.
 *
 * Returns: 0 on success, -EINVAL on failure
 */
int tls_set_value(u32 key, void *value)
{
    struct task_struct *task = current;

    if (key == 0 || key >= TLS_MAX_KEYS) {
        return -EINVAL;
    }

    if (!tls_manager.keys[key].in_use) {
        return -EINVAL;
    }

    /* Store value in task's TLS array */
    if (!task->tls) {
        /* Allocate TLS array if not present */
        task->tls = kzalloc(TLS_MAX_KEYS * sizeof(void *));
        if (!task->tls) {
            return -ENOMEM;
        }
    }

    ((void **)task->tls)[key] = value;

    return 0;
}

/**
 * tls_get_value - Get thread-local value for a key
 * @key: TLS key
 *
 * Gets the thread-local value for the specified key.
 *
 * Returns: Value associated with key, or NULL if not set
 */
void *tls_get_value(u32 key)
{
    struct task_struct *task = current;

    if (key == 0 || key >= TLS_MAX_KEYS) {
        return NULL;
    }

    if (!tls_manager.keys[key].in_use) {
        return NULL;
    }

    if (!task->tls) {
        return NULL;
    }

    return ((void **)task->tls)[key];
}

/**
 * tls_cleanup - Clean up TLS for exiting thread
 * @task: Task exiting
 *
 * Calls destructors for all TLS keys that have values set.
 */
static void tls_cleanup(struct task_struct *task)
{
    u32 key;

    if (!task || !task->tls) {
        return;
    }

    for (key = 1; key < TLS_MAX_KEYS; key++) {
        if (tls_manager.keys[key].in_use) {
            void *value = ((void **)task->tls)[key];

            if (value && tls_manager.keys[key].destructor) {
                tls_manager.keys[key].destructor(value);
            }
        }
    }

    /* Free TLS array */
    kfree(task->tls);
    task->tls = NULL;
}

/*===========================================================================*/
/*                         USER THREAD SUPPORT                               */
/*===========================================================================*/

/**
 * user_thread_create - Create a user-space thread
 * @name: Thread name
 * @fn: Thread entry function
 * @arg: Argument to pass to thread function
 * @stack_size: Stack size
 *
 * Creates a user-space thread that is scheduled by the kernel
 * but runs in user mode.
 *
 * Returns: Pointer to new task_struct, or NULL on failure
 */
struct task_struct *user_thread_create(const char *name, void (*fn)(void *),
                                        void *arg, size_t stack_size)
{
    struct task_struct *thread;

    if (!user_thread_manager.enabled) {
        pr_err("User threads are disabled\n");
        return NULL;
    }

    /* Check thread limit */
    spin_lock(&user_thread_manager.lock);
    if (user_thread_manager.active_user_threads >= user_thread_manager.max_user_threads) {
        spin_unlock(&user_thread_manager.lock);
        pr_err("User thread limit reached\n");
        return NULL;
    }
    user_thread_manager.active_user_threads++;
    spin_unlock(&user_thread_manager.lock);

    /* Create kernel thread */
    thread = thread_create(name, fn, arg, stack_size);
    if (!thread) {
        spin_lock(&user_thread_manager.lock);
        user_thread_manager.active_user_threads--;
        spin_unlock(&user_thread_manager.lock);
        return NULL;
    }

    /* Mark as user thread */
    thread->flags &= ~PF_KTHREAD;
    thread->flags |= PF_VCPU;

    /* Set user mode stack */
    arch_setup_user_stack(thread, stack_size);

    pr_debug("User thread created: %s (TID: %u)\n",
             thread->comm, thread->tid);

    return thread;
}

/**
 * user_thread_destroy - Destroy a user-space thread
 * @thread: Thread to destroy
 */
void user_thread_destroy(struct task_struct *thread)
{
    if (!thread) {
        return;
    }

    if (!(thread->flags & PF_VCPU)) {
        pr_warn("Not a user thread: %u\n", thread->tid);
        return;
    }

    thread_destroy(thread);

    spin_lock(&user_thread_manager.lock);
    if (user_thread_manager.active_user_threads > 0) {
        user_thread_manager.active_user_threads--;
    }
    spin_unlock(&user_thread_manager.lock);
}

/**
 * user_thread_yield - Yield current user thread
 *
 * Voluntarily yields the CPU to allow other threads to run.
 */
void user_thread_yield(void)
{
    schedule();
}

/**
 * user_thread_sleep - Put user thread to sleep
 * @ns: Nanoseconds to sleep
 */
void user_thread_sleep(u64 ns)
{
    struct task_struct *task = current;
    u64 wake_time;

    wake_time = get_ticks() * NS_PER_TICK + ns;

    task->state = TASK_INTERRUPTIBLE;

    while (get_ticks() * NS_PER_TICK < wake_time) {
        if (signal_pending(task)) {
            task->state = TASK_RUNNING;
            return;
        }
        schedule();
        task->state = TASK_INTERRUPTIBLE;
    }

    task->state = TASK_RUNNING;
}

/*===========================================================================*/
/*                         THREAD SYNCHRONIZATION                            */
/*===========================================================================*/

/**
 * thread_join - Wait for thread to exit
 * @thread: Thread to join
 * @ret: Pointer to store return value
 *
 * Waits for the specified thread to exit and optionally retrieves
 * its return value.
 *
 * Returns: 0 on success, negative error code on failure
 */
int thread_join(struct task_struct *thread, void **ret)
{
    int exit_code;
    pid_t pid;

    if (!thread) {
        return -EINVAL;
    }

    /* Wait for thread to exit */
    pid = wait_for_task(thread, &exit_code, 0);
    if (pid < 0) {
        return pid;
    }

    /* Get return value */
    if (ret) {
        *ret = (void *)(uintptr_t)exit_code;
    }

    /* Clean up thread resources */
    thread_destroy(thread);

    return 0;
}

/**
 * thread_detach - Detach a thread
 * @thread: Thread to detach
 *
 * Detaches a thread, allowing its resources to be automatically
 * reclaimed when it exits.
 *
 * Returns: 0 on success, negative error code on failure
 */
int thread_detach(struct task_struct *thread)
{
    if (!thread) {
        return -EINVAL;
    }

    /* Mark thread as detached */
    thread->flags |= PF_THREAD_BOUND;

    /* Parent will not wait for this thread */
    spin_lock(&thread_group_lock);
    if (!list_empty(&thread->sibling)) {
        list_del(&thread->sibling);
    }
    spin_unlock(&thread_group_lock);

    return 0;
}

/**
 * thread_cancel - Cancel a thread
 * @thread: Thread to cancel
 *
 * Requests cancellation of a thread. The thread will exit
 * at the next cancellation point.
 *
 * Returns: 0 on success, negative error code on failure
 */
int thread_cancel(struct task_struct *thread)
{
    if (!thread) {
        return -EINVAL;
    }

    /* Send cancellation signal */
    return task_kill(thread, SIGKILL);
}

/**
 * thread_set_priority - Set thread priority
 * @thread: Thread to modify
 * @priority: New priority
 *
 * Sets the scheduling priority of a thread.
 *
 * Returns: 0 on success, negative error code on failure
 */
int thread_set_priority(struct task_struct *thread, int priority)
{
    struct rq *rq;
    u64 flags;

    if (!thread) {
        return -EINVAL;
    }

    if (priority < MIN_PRIO || priority > MAX_PRIO) {
        return -EINVAL;
    }

    rq = get_rq(thread->cpu);
    rq_lock_irqsave(rq, &flags);

    thread->prio = priority;
    thread->static_prio = priority;
    thread->normal_prio = priority;

    /* Requeue thread with new priority */
    if (thread->state == TASK_RUNNING) {
        dequeue_task(rq, thread, 0);
        enqueue_task(rq, thread, 0);
    }

    rq_unlock_irqrestore(rq, flags);

    return 0;
}

/**
 * thread_get_priority - Get thread priority
 * @thread: Thread to query
 *
 * Returns the current priority of a thread.
 */
int thread_get_priority(struct task_struct *thread)
{
    if (!thread) {
        return -EINVAL;
    }

    return thread->prio;
}

/*===========================================================================*/
/*                         THREAD AFFINITY                                   */
/*===========================================================================*/

/**
 * thread_setaffinity - Set thread CPU affinity
 * @thread: Thread to modify
 * @cpu_mask: CPU mask
 *
 * Sets the CPUs on which a thread is allowed to run.
 *
 * Returns: 0 on success, negative error code on failure
 */
int thread_setaffinity(struct task_struct *thread, u64 cpu_mask)
{
    struct rq *rq;
    u64 flags;
    u32 new_cpu;

    if (!thread) {
        return -EINVAL;
    }

    /* Check if mask has at least one valid CPU */
    if (cpu_mask == 0) {
        return -EINVAL;
    }

    rq = get_rq(thread->cpu);
    rq_lock_irqsave(rq, &flags);

    thread->cpus_allowed = cpu_mask;

    /* If thread is not allowed on current CPU, migrate it */
    if (!(cpu_mask & (1ULL << thread->cpu))) {
        /* Find first allowed CPU */
        new_cpu = __builtin_ctzll(cpu_mask);
        if (new_cpu < MAX_CPUS) {
            thread->cpu = new_cpu;
        }
    }

    rq_unlock_irqrestore(rq, flags);

    return 0;
}

/**
 * thread_getaffinity - Get thread CPU affinity
 * @thread: Thread to query
 *
 * Returns the CPU mask for a thread.
 */
u64 thread_getaffinity(struct task_struct *thread)
{
    if (!thread) {
        return 0;
    }

    return thread->cpus_allowed;
}

/*===========================================================================*/
/*                         THREAD INFO                                       */
/*===========================================================================*/

/**
 * thread_info - Print thread information
 * @thread: Thread to query
 */
void thread_info(struct task_struct *thread)
{
    if (!thread) {
        return;
    }

    printk("\n=== Thread Information ===\n");
    printk("Name: %s\n", thread->comm);
    printk("PID: %u, TID: %u, TGID: %u\n",
           thread->pid, thread->tid, thread->tgid);
    printk("State: 0x%lx\n", thread->state);
    printk("Flags: 0x%x\n", thread->flags);
    printk("Priority: %d\n", thread->prio);
    printk("Policy: %u\n", thread->policy);
    printk("CPU: %u (Last: %u)\n", thread->cpu, thread->last_cpu);
    printk("CPUs Allowed: 0x%llx\n", (unsigned long long)thread->cpus_allowed);
    printk("Stack: 0x%p\n", thread->stack);
    printk("TLS: 0x%p\n", thread->tls);
    printk("Parent: %s (PID: %u)\n",
           thread->parent ? thread->parent->comm : "NULL",
           thread->parent ? thread->parent->pid : 0);
    printk("Children: %s\n", list_empty(&thread->children) ? "None" : "Yes");
    printk("\n");
}

/**
 * thread_list - List all threads
 */
void thread_list(void)
{
    struct task_struct *task;

    printk("\n=== Thread List ===\n");
    printk("%-8s %-8s %-16s %-8s %-8s %s\n",
           "PID", "TID", "Name", "State", "CPU", "Flags");
    printk("--------------------------------------------------------\n");

    spin_lock(&task_list_lock);
    list_for_each_entry(task, &task_list, run_list) {
        const char *state_str;

        switch (task->state & 0xff) {
            case TASK_RUNNING:
                state_str = "R";
                break;
            case TASK_INTERRUPTIBLE:
                state_str = "S";
                break;
            case TASK_UNINTERRUPTIBLE:
                state_str = "D";
                break;
            case TASK_STOPPED:
                state_str = "T";
                break;
            case TASK_EXITING:
                state_str = "X";
                break;
            case TASK_DEAD:
                state_str = "Z";
                break;
            default:
                state_str = "?";
                break;
        }

        printk("%-8u %-8u %-16s %-8s %-8u 0x%x\n",
               task->pid, task->tid, task->comm, state_str,
               task->cpu, task->flags);
    }
    spin_unlock(&task_list_lock);

    printk("\nTotal: %d threads\n", atomic_read(&thread_count));
    printk("\n");
}

/*===========================================================================*/
/*                         THREAD STATISTICS                                 */
/*===========================================================================*/

/**
 * thread_stats - Print thread statistics
 */
void thread_stats(void)
{
    printk("\n=== Thread Statistics ===\n");
    printk("Total Threads: %d\n", atomic_read(&thread_count));
    printk("Total Processes: %d\n", atomic_read(&process_count));
    printk("User Threads Enabled: %s\n",
           user_thread_manager.enabled ? "Yes" : "No");
    printk("Active User Threads: %u / %u\n",
           user_thread_manager.active_user_threads,
           user_thread_manager.max_user_threads);
    printk("\n");
}

/*===========================================================================*/
/*                         ARCHITECTURE INTERFACES                           */
/*===========================================================================*/

/* These functions must be implemented by architecture-specific code */

extern int arch_init_thread(struct task_struct *thread, struct task_struct *parent,
                            void (*fn)(void *), void *arg);
extern void arch_setup_user_stack(struct task_struct *thread, size_t stack_size);
extern bool signal_pending(struct task_struct *task);

/* Signal constants */
#define SIGKILL     9
