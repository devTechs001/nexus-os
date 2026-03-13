/*
 * NEXUS OS - Real-Time Scheduler
 * kernel/sched/realtime.c
 */

#include "sched.h"
#include "../include/types.h"
#include "../include/kernel.h"

/* Forward declarations */
struct rt_mutex;

/*===========================================================================*/
/*                         RT SCHEDULER CONFIGURATION                        */
/*===========================================================================*/

/* RT scheduler tunables */
static struct {
    /* RT runtime in microseconds (950ms default) */
    u64 rt_runtime_us;

    /* RT period in microseconds (1000ms default) */
    u64 rt_period_us;

    /* RT throttling enabled */
    bool throttling_enabled;

    /* Priority inheritance enabled */
    bool pi_enabled;

    /* Maximum RT priority */
    int max_rt_prio;

    /* Default RT priority */
    int default_rt_prio;
} rt_tunables = {
    .rt_runtime_us = 950000,
    .rt_period_us = 1000000,
    .throttling_enabled = true,
    .pi_enabled = true,
    .max_rt_prio = MAX_RT_PRIO - 1,
    .default_rt_prio = 50
};

/* Priority inheritance boost */
#define PI_BOOST_MAX        10
#define PI_FUTEX_BIT        0x80000000

/*===========================================================================*/
/*                         RT RUN QUEUE MANAGEMENT                           */
/*===========================================================================*/

/**
 * rt_rq_init - Initialize RT run queue
 * @rt_rq: RT run queue to initialize
 * @rq: Parent run queue
 */
static void rt_rq_init(struct rt_rq *rt_rq, struct rq *rq)
{
    int i;

    rt_rq->nr_running = 0;
    rt_rq->time = 0;
    rt_rq->rt_runtime = rt_tunables.rt_runtime_us * NS_PER_US;
    rt_rq->rt_period = rt_tunables.rt_period_us * NS_PER_US;
    rt_rq->throttled = 0;
    rt_rq->throttled_clock = 0;
    rt_rq->pushable_tasks = 0;
    rt_rq->rq = rq;

    /* Initialize priority array */
    for (i = 0; i < MAX_RT_PRIO; i++) {
        INIT_LIST_HEAD(&rt_rq->queue[i]);
    }
}

/**
 * rt_rq_empty - Check if RT run queue is empty
 * @rt_rq: RT run queue
 */
static inline bool rt_rq_empty(struct rt_rq *rt_rq)
{
    return rt_rq->nr_running == 0;
}

/**
 * rt_rq_has_tasks - Check if RT run queue has tasks
 * @rt_rq: RT run queue
 */
static inline bool rt_rq_has_tasks(struct rt_rq *rt_rq)
{
    return rt_rq->nr_running > 0;
}

/*===========================================================================*/
/*                         RT THROTTLING                                     */
/*===========================================================================*/

/**
 * update_rt_runtime - Update RT runtime accounting
 * @rq: Run queue
 *
 * Updates the RT runtime consumption and handles throttling
 * when RT tasks exceed their allocated runtime.
 */
void update_rt_runtime(struct rq *rq)
{
    struct rt_rq *rt_rq = &rq->rt;
    u64 now = rq->clock;
    u64 delta;

    if (!rt_tunables.throttling_enabled) {
        return;
    }

    /* Calculate time delta since last update */
    delta = now - rt_rq->time;
    rt_rq->time = now;

    /* Replenish runtime at period boundary */
    if (rt_rq->rt_runtime >= rt_tunables.rt_runtime_us * NS_PER_US) {
        /* Period expired, replenish */
        rt_rq->rt_runtime = rt_tunables.rt_runtime_us * NS_PER_US;
        rt_rq->throttled = 0;
    }

    /* Consume runtime if RT tasks are running */
    if (rt_rq->nr_running > 0 && !rt_rq->throttled) {
        if (rt_rq->rt_runtime >= delta) {
            rt_rq->rt_runtime -= delta;
        } else {
            /* Runtime exhausted, throttle */
            rt_rq->rt_runtime = 0;
            rt_rq->throttled = 1;
            rt_rq->throttled_clock = now;

            pr_debug("RT throttling activated on CPU %u\n", rq->cpu);
        }
    }
}

/**
 * rt_throttled - Check if RT run queue is throttled
 * @rt_rq: RT run queue
 */
static inline bool rt_throttled(struct rt_rq *rt_rq)
{
    return rt_rq->throttled;
}

/**
 * do_sched_rt_period_timer - Handle RT period timer
 * @rt_rq: RT run queue
 *
 * Called at each RT period boundary to replenish runtime.
 */
static void do_sched_rt_period_timer(struct rt_rq *rt_rq)
{
    /* Replenish runtime */
    rt_rq->rt_runtime = rt_tunables.rt_runtime_us * NS_PER_US;
    rt_rq->throttled = 0;

    pr_debug("RT period timer fired, runtime replenished\n");
}

/**
 * sched_rt_runtime - Get/set RT runtime
 * @runtime: New runtime in microseconds (-1 for query)
 * @period: Period in microseconds
 *
 * Configures RT throttling parameters.
 *
 * Returns: Current runtime, or negative error code
 */
s64 sched_rt_runtime(s64 runtime, u64 period)
{
    s64 old_runtime;

    if (runtime < -1 || runtime > 1000000) {
        return -EINVAL;
    }

    old_runtime = rt_tunables.rt_runtime_us;

    if (runtime >= 0) {
        rt_tunables.rt_runtime_us = runtime;
    }

    if (period > 0) {
        rt_tunables.rt_period_us = period;
    }

    return old_runtime;
}

/*===========================================================================*/
/*                         TASK ENQUEUE/DEQUEUE                              */
/*===========================================================================*/

/**
 * enqueue_task_rt - Enqueue task to RT run queue
 * @rq: Run queue
 * @task: Task to enqueue
 * @flags: Enqueue flags
 *
 * Adds an RT task to the priority array based on its priority.
 * RT tasks are stored in FIFO order within each priority level.
 */
void enqueue_task_rt(struct rq *rq, struct task_struct *task, int flags)
{
    struct rt_rq *rt_rq = &rq->rt;
    struct list_head *queue;

    /* Check throttling */
    if (rt_throttled(rt_rq) && !(flags & ENQUEUE_REPLENISH)) {
        return;
    }

    /* Get priority queue */
    queue = &rt_rq->queue[task->rt_priority];

    /* Add to tail of priority queue (FIFO) */
    list_add_tail(&task->run_list, queue);

    /* Update statistics */
    rt_rq->nr_running++;

    /* Update pushable tasks */
    if (task->rt_priority < rt_rq->pushable_tasks) {
        rt_rq->pushable_tasks = task->rt_priority;
    }

    pr_debug("RT: Enqueued %s (priority: %u)\n",
             task->comm, task->rt_priority);
}

/**
 * dequeue_task_rt - Dequeue task from RT run queue
 * @rq: Run queue
 * @task: Task to dequeue
 * @flags: Dequeue flags
 *
 * Removes an RT task from the priority array.
 */
void dequeue_task_rt(struct rq *rq, struct task_struct *task, int flags)
{
    struct rt_rq *rt_rq = &rq->rt;

    /* Remove from priority queue */
    list_del(&task->run_list);

    /* Update statistics */
    rt_rq->nr_running--;

    /* Update pushable tasks */
    if (rt_rq->nr_running > 0) {
        /* Find highest priority task */
        int prio;
        for (prio = 0; prio < MAX_RT_PRIO; prio++) {
            if (!list_empty(&rt_rq->queue[prio])) {
                rt_rq->pushable_tasks = prio;
                break;
            }
        }
    } else {
        rt_rq->pushable_tasks = MAX_RT_PRIO;
    }

    pr_debug("RT: Dequeued %s (priority: %u)\n",
             task->comm, task->rt_priority);
}

/**
 * put_prev_task_rt - Put previous RT task back on run queue
 * @rq: Run queue
 * @prev: Previous task
 *
 * Called when an RT task is being switched out.
 */
void put_prev_task_rt(struct rq *rq, struct task_struct *prev)
{
    /* Re-enqueue if still runnable */
    if (prev->state == TASK_RUNNING) {
        enqueue_task_rt(rq, prev, 0);
    }
}

/*===========================================================================*/
/*                         TASK SELECTION                                    */
/*===========================================================================*/

/**
 * pick_next_task_rt - Pick next RT task to run
 * @rq: Run queue
 *
 * Selects the highest priority RT task to run next.
 * RT tasks are selected in priority order, with FIFO within
 * each priority level.
 *
 * Returns: Next RT task to run, or NULL if no runnable RT tasks
 */
struct task_struct *pick_next_task_rt(struct rq *rq)
{
    struct rt_rq *rt_rq = &rq->rt;
    struct task_struct *task;
    int prio;

    /* Check throttling */
    if (rt_throttled(rt_rq)) {
        return NULL;
    }

    /* Find highest priority task */
    for (prio = 0; prio < MAX_RT_PRIO; prio++) {
        if (!list_empty(&rt_rq->queue[prio])) {
            task = list_entry(rt_rq->queue[prio].next,
                             struct task_struct, run_list);

            pr_debug("RT: Picked %s (priority: %u)\n",
                     task->comm, task->rt_priority);

            return task;
        }
    }

    return NULL;
}

/**
 * check_preempt_curr_rt - Check if RT task should preempt current
 * @rq: Run queue
 * @task: New task
 * @flags: Preempt flags
 *
 * Checks if a newly woken RT task should preempt the currently
 * running task.
 */
void check_preempt_curr_rt(struct rq *rq, struct task_struct *task, int flags)
{
    struct task_struct *curr = rq->curr;

    if (!curr) {
        resched_cpu(rq->cpu);
        return;
    }

    /* RT tasks always preempt non-RT tasks */
    if (curr->policy != SCHED_FIFO && curr->policy != SCHED_RR) {
        resched_cpu(rq->cpu);
        return;
    }

    /* Higher priority RT task preempts lower */
    if (task->rt_priority > curr->rt_priority) {
        resched_cpu(rq->cpu);
        return;
    }

    /* Same priority FIFO task does not preempt */
    if (task->policy == SCHED_FIFO && task->rt_priority == curr->rt_priority) {
        return;
    }

    /* Round-robin at same priority triggers reschedule */
    if (task->policy == SCHED_RR && task->rt_priority == curr->rt_priority) {
        resched_cpu(rq->cpu);
    }
}

/*===========================================================================*/
/*                         FIFO SCHEDULING                                   */
/*===========================================================================*/

/**
 * sched_fifo_run - Run FIFO scheduled task
 * @rq: Run queue
 * @task: Task to run
 *
 * Handles execution of a FIFO-scheduled RT task.
 * FIFO tasks run until they block, yield, or are preempted
 * by a higher priority task.
 */
static void sched_fifo_run(struct rq *rq, struct task_struct *task)
{
    /* FIFO tasks have no time slice - run until blocked */
    task->rt.time_slice = 0;

    pr_debug("FIFO: Running %s (priority: %u)\n",
             task->comm, task->rt_priority);
}

/**
 * fifo_wakeup - Handle FIFO task wakeup
 * @rq: Run queue
 * @task: Woken task
 *
 * Handles wakeup of a FIFO-scheduled task.
 */
static void fifo_wakeup(struct rq *rq, struct task_struct *task)
{
    /* FIFO tasks go to end of their priority queue */
    enqueue_task_rt(rq, task, 0);

    /* Check for preemption */
    check_preempt_curr_rt(rq, task, 0);
}

/*===========================================================================*/
/*                         ROUND-ROBIN SCHEDULING                            */
/*===========================================================================*/

/**
 * sched_rr_get_interval - Get RR task's time slice
 * @task: RR task
 *
 * Returns the time slice for a round-robin task.
 */
static u64 sched_rr_get_interval(struct task_struct *task)
{
    /* Default RR time slice: 100ms */
    return 100 * NS_PER_MS;
}

/**
 * sched_rr_run - Run RR scheduled task
 * @rq: Run queue
 * @task: Task to run
 *
 * Handles execution of a round-robin scheduled RT task.
 * RR tasks run for their time slice, then yield to other
 * tasks at the same priority.
 */
static void sched_rr_run(struct rq *rq, struct task_struct *task)
{
    /* Set time slice */
    task->rt.time_slice = sched_rr_get_interval(task);

    pr_debug("RR: Running %s (priority: %u, slice: %llu ms)\n",
             task->comm, task->rt_priority,
             (unsigned long long)task->rt.time_slice / NS_PER_MS);
}

/**
 * sched_rr_tick - Handle RR task tick
 * @rq: Run queue
 * @task: Current RR task
 *
 * Called at each timer tick for the current RR task.
 * Checks if time slice has expired.
 */
static void sched_rr_tick(struct rq *rq, struct task_struct *task)
{
    if (task->policy != SCHED_RR) {
        return;
    }

    /* Decrement time slice */
    if (task->rt.time_slice > NS_PER_TICK) {
        task->rt.time_slice -= NS_PER_TICK;
    } else {
        task->rt.time_slice = 0;
    }

    /* Check if time slice expired */
    if (task->rt.time_slice == 0) {
        /* Dequeue and re-enqueue at end of priority queue */
        dequeue_task_rt(rq, task, 0);
        enqueue_task_rt(rq, task, 0);

        /* Trigger reschedule */
        resched_cpu(rq->cpu);

        pr_debug("RR: Time slice expired for %s\n", task->comm);
    }
}

/**
 * rr_wakeup - Handle RR task wakeup
 * @rq: Run queue
 * @task: Woken task
 *
 * Handles wakeup of a round-robin scheduled task.
 */
static void rr_wakeup(struct rq *rq, struct task_struct *task)
{
    /* Reset time slice on wakeup */
    task->rt.time_slice = sched_rr_get_interval(task);

    /* Add to end of priority queue */
    enqueue_task_rt(rq, task, 0);

    /* Check for preemption */
    check_preempt_curr_rt(rq, task, 0);
}

/*===========================================================================*/
/*                         PRIORITY HANDLING                                 */
/*===========================================================================*/

/**
 * rt_set_priority - Set RT task priority
 * @task: Task to modify
 * @prio: New RT priority (0-99)
 *
 * Sets the real-time priority of a task. Higher values
 * indicate higher priority.
 *
 * Returns: 0 on success, negative error code on failure
 */
int rt_set_priority(struct task_struct *task, int prio)
{
    struct rq *rq;
    u64 flags;
    int old_prio;
    bool queued;

    if (!task) {
        return -EINVAL;
    }

    /* Validate priority range */
    if (prio < 0 || prio >= MAX_RT_PRIO) {
        return -EINVAL;
    }

    rq = get_rq(task->cpu);
    rq_lock_irqsave(rq, &flags);

    old_prio = task->rt_priority;
    queued = (task->state == TASK_RUNNING);

    /* Dequeue if queued */
    if (queued) {
        dequeue_task_rt(rq, task, 0);
    }

    /* Update priority */
    task->rt_priority = prio;

    /* Update internal priority (inverted - lower is higher) */
    task->prio = MAX_RT_PRIO - 1 - prio;
    task->static_prio = task->prio;
    task->normal_prio = task->prio;

    /* Re-enqueue if was queued */
    if (queued) {
        enqueue_task_rt(rq, task, 0);
    }

    rq_unlock_irqrestore(rq, flags);

    pr_debug("RT priority changed for %s: %d -> %d\n",
             task->comm, old_prio, prio);

    return 0;
}

/**
 * rt_get_priority - Get RT task priority
 * @task: Task to query
 *
 * Returns the real-time priority of a task.
 */
int rt_get_priority(struct task_struct *task)
{
    if (!task) {
        return -EINVAL;
    }

    return task->rt_priority;
}

/**
 * rt_task - Check if task is RT task
 * @task: Task to check
 */
bool rt_task(struct task_struct *task)
{
    if (!task) {
        return false;
    }

    return (task->policy == SCHED_FIFO || task->policy == SCHED_RR);
}

/*===========================================================================*/
/*                         PRIORITY INHERITANCE                              */
/*===========================================================================*/

/**
 * rt_mutex_init - Initialize RT mutex
 * @lock: RT mutex to initialize
 *
 * Initializes an RT mutex with priority inheritance support.
 */
void rt_mutex_init(rt_mutex_t *lock)
{
    if (!lock) {
        return;
    }

    spin_lock_init(&lock->wait_lock);
    lock->owner = NULL;
    lock->pi_boosted_prio = -1;
    INIT_LIST_HEAD(&lock->wait_list);
}

/**
 * rt_mutex_lock - Acquire RT mutex with priority inheritance
 * @lock: RT mutex to acquire
 *
 * Acquires an RT mutex, implementing priority inheritance
 * to prevent priority inversion.
 *
 * Returns: 0 on success, negative error code on failure
 */
int rt_mutex_lock(rt_mutex_t *lock)
{
    struct task_struct *task = current;
    struct task_struct *owner;
    unsigned long flags;
    int ret = 0;

    if (!lock) {
        return -EINVAL;
    }

    spin_lock_irqsave(&lock->wait_lock, flags);

    /* Check if lock is free */
    if (!lock->owner) {
        lock->owner = task;
        spin_unlock_irqrestore(&lock->wait_lock, flags);
        return 0;
    }

    /* Lock is held by another task */
    owner = lock->owner;

    /* Add to wait list */
    list_add_tail(&task->run_list, &lock->wait_list);

    /* Priority inheritance: boost owner's priority if needed */
    if (rt_tunables.pi_enabled && task->rt_priority > owner->rt_priority) {
        int boost = task->rt_priority - owner->rt_priority;

        if (boost > PI_BOOST_MAX) {
            boost = PI_BOOST_MAX;
        }

        owner->pi_boosted_prio = owner->rt_priority + boost;

        /* Update owner's effective priority */
        if (owner->pi_boosted_prio > owner->rt_priority) {
            rt_set_priority(owner, owner->pi_boosted_prio);
        }

        pr_debug("PI: Boosted %s from %d to %d\n",
                 owner->comm, owner->rt_priority, owner->pi_boosted_prio);
    }

    spin_unlock_irqrestore(&lock->wait_lock, flags);

    /* Wait for lock */
    while (1) {
        task->state = TASK_UNINTERRUPTIBLE;

        if (lock->owner == NULL) {
            break;
        }

        schedule();
    }

    task->state = TASK_RUNNING;

    return ret;
}

/**
 * rt_mutex_unlock - Release RT mutex
 * @lock: RT mutex to release
 *
 * Releases an RT mutex and restores any priority boosts.
 *
 * Returns: 0 on success, negative error code on failure
 */
int rt_mutex_unlock(rt_mutex_t *lock)
{
    struct task_struct *task = current;
    struct task_struct *next;
    unsigned long flags;

    if (!lock) {
        return -EINVAL;
    }

    spin_lock_irqsave(&lock->wait_lock, flags);

    /* Check ownership */
    if (lock->owner != task) {
        spin_unlock_irqrestore(&lock->wait_lock, flags);
        return -EPERM;
    }

    /* Restore priority if boosted */
    if (task->pi_boosted_prio >= 0) {
        rt_set_priority(task, task->rt_priority);
        task->pi_boosted_prio = -1;
    }

    /* Wake next waiter */
    if (!list_empty(&lock->wait_list)) {
        next = list_entry(lock->wait_list.next, struct task_struct, run_list);
        list_del(&next->run_list);
        lock->owner = next;
        task_wake_up(next);
    } else {
        lock->owner = NULL;
    }

    spin_unlock_irqrestore(&lock->wait_lock, flags);

    return 0;
}

/**
 * rt_mutex_trylock - Try to acquire RT mutex without blocking
 * @lock: RT mutex to acquire
 *
 * Attempts to acquire an RT mutex without blocking.
 *
 * Returns: 0 on success, -EBUSY if lock is held
 */
int rt_mutex_trylock(rt_mutex_t *lock)
{
    unsigned long flags;
    int ret = 0;

    if (!lock) {
        return -EINVAL;
    }

    spin_lock_irqsave(&lock->wait_lock, flags);

    if (!lock->owner) {
        lock->owner = current;
        ret = 0;
    } else {
        ret = -EBUSY;
    }

    spin_unlock_irqrestore(&lock->wait_lock, flags);

    return ret;
}

/*===========================================================================*/
/*                         RT TASK CREATION                                  */
/*===========================================================================*/

/**
 * create_rt_task - Create a real-time task
 * @name: Task name
 * @fn: Entry function
 * @arg: Argument
 * @policy: RT policy (SCHED_FIFO or SCHED_RR)
 * @priority: RT priority (0-99)
 *
 * Creates a new real-time task with the specified policy
 * and priority.
 *
 * Returns: Pointer to new task, or NULL on failure
 */
struct task_struct *create_rt_task(const char *name, void (*fn)(void *),
                                    void *arg, int policy, int priority)
{
    struct task_struct *task;
    int ret;

    /* Validate policy */
    if (policy != SCHED_FIFO && policy != SCHED_RR) {
        pr_err("Invalid RT policy: %d\n", policy);
        return NULL;
    }

    /* Validate priority */
    if (priority < 0 || priority >= MAX_RT_PRIO) {
        pr_err("Invalid RT priority: %d\n", priority);
        return NULL;
    }

    /* Create task */
    task = task_create(name, fn, arg);
    if (!task) {
        return NULL;
    }

    /* Set RT policy and priority */
    task->policy = policy;
    task->rt_priority = priority;
    task->prio = MAX_RT_PRIO - 1 - priority;
    task->static_prio = task->prio;
    task->normal_prio = task->prio;

    /* Initialize RT-specific fields */
    task->rt.timeout = 0;
    task->rt.time_slice = (policy == SCHED_RR) ?
                          sched_rr_get_interval(task) : 0;

    pr_debug("RT task created: %s (policy: %s, priority: %d)\n",
             task->comm,
             policy == SCHED_FIFO ? "FIFO" : "RR",
             priority);

    return task;
}

/*===========================================================================*/
/*                         RT STATISTICS                                     */
/*===========================================================================*/

/**
 * rt_stats - Print RT scheduler statistics
 */
void rt_stats(void)
{
    u32 cpu;
    u32 num_cpus = get_num_cpus();

    printk("\n=== RT Scheduler Statistics ===\n");
    printk("\nTunables:\n");
    printk("  RT Runtime: %llu us\n",
           (unsigned long long)rt_tunables.rt_runtime_us);
    printk("  RT Period: %llu us\n",
           (unsigned long long)rt_tunables.rt_period_us);
    printk("  Throttling: %s\n",
           rt_tunables.throttling_enabled ? "Enabled" : "Disabled");
    printk("  Priority Inheritance: %s\n",
           rt_tunables.pi_enabled ? "Enabled" : "Disabled");
    printk("\n");

    for (cpu = 0; cpu < num_cpus; cpu++) {
        struct rq *rq = get_rq(cpu);
        struct rt_rq *rt_rq = &rq->rt;
        int prio;

        printk("CPU %u:\n", cpu);
        printk("  RT Tasks: %u\n", rt_rq->nr_running);
        printk("  Runtime Remaining: %llu us\n",
               (unsigned long long)rt_rq->rt_runtime / NS_PER_US);
        printk("  Throttled: %s\n", rt_rq->throttled ? "Yes" : "No");
        printk("  Pushable Tasks Priority: %d\n", rt_rq->pushable_tasks);

        /* Print tasks by priority */
        if (rt_rq->nr_running > 0) {
            printk("  Tasks by Priority:\n");
            for (prio = 0; prio < MAX_RT_PRIO; prio++) {
                if (!list_empty(&rt_rq->queue[prio])) {
                    struct task_struct *t;
                    list_for_each_entry(t, &rt_rq->queue[prio], run_list) {
                        printk("    Priority %d: %s (PID: %u)\n",
                               prio, t->comm, t->pid);
                    }
                }
            }
        }
        printk("\n");
    }
}

/*===========================================================================*/
/*                         RT TASK ITERATION                                 */
/*===========================================================================*/

/**
 * for_each_rt_task - Iterate over RT tasks on run queue
 * @task: Task pointer variable
 * @rq: Run queue
 *
 * Macro-like iteration over all RT tasks on a run queue.
 */
void for_each_rt_task(struct task_struct *task, struct rq *rq)
{
    /* This is implemented as a macro in the header */
    (void)task;
    (void)rq;
}

/*===========================================================================*/
/*                         SCHEDULER CLASS OPERATIONS                        */
/*===========================================================================*/

/* RT scheduler class operations */
const struct sched_class rt_sched_class = {
    .enqueue_task = enqueue_task_rt,
    .dequeue_task = dequeue_task_rt,
    .put_prev_task = put_prev_task_rt,
    .pick_next_task = pick_next_task_rt,
    .check_preempt_curr = check_preempt_curr_rt,
    .task_tick = sched_rr_tick,
    .switched_from = NULL,
    .switched_to = NULL,
    .prio_changed = NULL,
};

/*===========================================================================*/
/*                         ARCHITECTURE INTERFACES                           */
/*===========================================================================*/

/* These functions may need architecture-specific implementations */

extern void resched_cpu(int cpu);

/* Enqueue flags */
#define ENQUEUE_REPLENISH     0x01
#define ENQUEUE_HEAD          0x02

/* Scheduler class structure */
struct sched_class {
    void (*enqueue_task)(struct rq *rq, struct task_struct *task, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *task, int flags);
    void (*put_prev_task)(struct rq *rq, struct task_struct *task);
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *task, int flags);
    void (*task_tick)(struct rq *rq, struct task_struct *task);
    void (*switched_from)(struct rq *rq, struct task_struct *task);
    void (*switched_to)(struct rq *rq, struct task_struct *task);
    void (*prio_changed)(struct rq *rq, struct task_struct *task);
};
