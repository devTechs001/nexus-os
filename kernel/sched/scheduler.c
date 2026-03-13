/*
 * NEXUS OS - Main Scheduler Core
 * kernel/sched/scheduler.c
 */

#include "sched.h"
#include "../include/types.h"
#include "../include/kernel.h"

/* Forward declarations for scheduler classes */
struct task_struct *pick_next_task_rt(struct rq *rq);
struct task_struct *pick_next_task_fair(struct rq *rq);
static struct task_struct *pull_task(struct rq *src_rq, struct rq *dst_rq);

/* Preemption and reschedule */
void set_need_resched(void);
bool need_resched(void);
void clear_need_resched(void);

/* Architecture stubs */
void arch_prepare_switch(struct task_struct *prev, struct task_struct *next)
{
    (void)prev;
    (void)next;
}

void arch_finish_switch(struct task_struct *prev)
{
    (void)prev;
}

void arch_switch_mm(struct address_space *prev_mm, struct address_space *next_mm)
{
    (void)prev_mm;
    (void)next_mm;
}

void arch_switch_to(struct task_struct *prev, struct task_struct *next)
{
    (void)prev;
    (void)next;
}

int arch_task_kill(struct task_struct *task, int signal)
{
    (void)task;
    (void)signal;
    return 0;
}

void arch_idle_cpu(void)
{
    /* Halt CPU until interrupt */
}

/*===========================================================================*/
/*                         SCHEDULER GLOBAL DATA                             */
/*===========================================================================*/

/* Per-CPU Run Queues */
static struct rq run_queues[MAX_CPUS];

/* Scheduler State */
static struct {
    bool enabled;
    bool started;
    u64 tick_count;
    u64 schedule_count;
    u64 context_switch_count;
    spinlock_t global_lock;
} scheduler_state = {
    .enabled = false,
    .started = false,
    .tick_count = 0,
    .schedule_count = 0,
    .context_switch_count = 0,
    .global_lock = { .lock = 0 }
};

/* Idle tasks per CPU */
static struct task_struct *idle_tasks[MAX_CPUS];

/* Preemption count per CPU */
DEFINE_PER_CPU(int, per_cpu_preempt_count) = 0;

/* Scheduler tick timer */
static u64 scheduler_tick_ns = NS_PER_TICK;

/*===========================================================================*/
/*                         RUN QUEUE MANAGEMENT                              */
/*===========================================================================*/

/**
 * rq_lock - Acquire run queue spinlock
 * @rq: Run queue to lock
 */
void rq_lock(struct rq *rq)
{
    spin_lock(&rq->lock);
}

/**
 * rq_unlock - Release run queue spinlock
 * @rq: Run queue to unlock
 */
void rq_unlock(struct rq *rq)
{
    spin_unlock(&rq->lock);
}

/**
 * rq_lock_irqsave - Acquire run queue spinlock with IRQ save
 * @rq: Run queue to lock
 * @flags: IRQ flags storage
 */
void rq_lock_irqsave(struct rq *rq, u64 *flags)
{
    spin_lock_irqsave(&rq->lock, *flags);
}

/**
 * rq_unlock_irqrestore - Release run queue spinlock with IRQ restore
 * @rq: Run queue to unlock
 * @flags: IRQ flags to restore
 */
void rq_unlock_irqrestore(struct rq *rq, u64 flags)
{
    spin_unlock_irqrestore(&rq->lock, flags);
}

/**
 * get_cpu_rq - Get run queue for current CPU
 */
struct rq *get_cpu_rq(void)
{
    return &run_queues[get_cpu_id()];
}

/**
 * get_rq - Get run queue for specific CPU
 * @cpu: CPU number
 */
struct rq *get_rq(u32 cpu)
{
    if (cpu >= MAX_CPUS) {
        cpu = 0;
    }
    return &run_queues[cpu];
}

/**
 * init_rq - Initialize a run queue
 * @rq: Run queue to initialize
 * @cpu: CPU number
 */
static void init_rq(struct rq *rq, u32 cpu)
{
    rq->cpu = cpu;
    spin_lock_init(&rq->lock);
    rq->nr_running = 0;
    rq->curr = NULL;
    rq->idle = NULL;
    rq->prev = NULL;
    rq->next = NULL;
    rq->clock = 0;
    rq->clock_task = 0;
    rq->nr_switches = 0;
    rq->nr_sched_events = 0;

    /* Initialize CFS run queue */
    rq->cfs.tasks_timeline = RB_ROOT;
    rq->cfs.rb_leftmost = NULL;
    rq->cfs.nr_running = 0;
    rq->cfs.nr_spread_over = 0;
    rq->cfs.min_vruntime = 0;
    rq->cfs.min_granularity = DEF_TIMESLICE;
    rq->cfs.granularity = DEF_TIMESLICE;
    rq->cfs.latency = 6 * DEF_TIMESLICE;
    rq->cfs.runtime = 0;
    rq->cfs.runtime_total = 0;
    rq->cfs.load = 0;
    rq->cfs.load_avg = 0;
    rq->cfs.exec_clock = 0;
    rq->cfs.min_vruntime_runnable = 0;
    rq->cfs.rq = rq;

    /* Initialize RT run queue */
    rq->rt.nr_running = 0;
    rq->rt.time = 0;
    rq->rt.rt_runtime = 950 * NS_PER_MS;
    rq->rt.rt_period = 1000 * NS_PER_MS;
    rq->rt.throttled = 0;
    rq->rt.throttled_clock = 0;
    rq->rt.pushable_tasks = 0;
    rq->rt.rq = rq;

    /* Initialize priority array */
    for (int i = 0; i < MAX_RT_PRIO; i++) {
        INIT_LIST_HEAD(&rq->rt.queue[i]);
    }
}

/**
 * update_rq_clock - Update run queue clock
 * @rq: Run queue to update
 */
static inline void update_rq_clock(struct rq *rq)
{
    rq->clock = get_ticks() * NS_PER_TICK;
    rq->clock_task = rq->clock;
}

/*===========================================================================*/
/*                         IDLE TASK MANAGEMENT                              */
/*===========================================================================*/

/**
 * idle_thread - Idle task main function
 * @arg: Unused argument
 */
static void idle_thread(void *arg)
{
    struct task_struct *task = (struct task_struct *)arg;

    /* Mark as idle */
    task->flags |= PF_IDLE;

    pr_debug("Idle task started on CPU %u (PID: %u)\n", task->cpu, task->pid);

    /* Infinite idle loop */
    for (;;) {
        /* Enter idle state */
        local_irq_enable();

        /* Architecture-specific idle */
        arch_idle_cpu();

        /* Check for reschedule */
        local_irq_disable();

        if (need_resched()) {
            schedule();
        }
    }
}

/**
 * create_idle_task - Create idle task for CPU
 * @cpu: CPU number
 */
static int create_idle_task(u32 cpu)
{
    struct task_struct *task;
    struct rq *rq = get_rq(cpu);

    task = kzalloc(sizeof(struct task_struct));
    if (!task) {
        pr_err("Failed to allocate idle task for CPU %u\n", cpu);
        return -ENOMEM;
    }

    /* Initialize task structure */
    task->pid = cpu;
    task->tgid = cpu;
    task->tid = cpu;
    task->state = TASK_RUNNING;
    task->flags = PF_KTHREAD | PF_IDLE;
    task->prio = MAX_PRIO - 1;
    task->static_prio = MAX_PRIO - 1;
    task->normal_prio = MAX_PRIO - 1;
    task->rt_priority = 0;
    task->policy = SCHED_NORMAL;
    task->cpu = cpu;
    task->last_cpu = cpu;
    task->cpus_allowed = (1ULL << cpu);

    /* Initialize scheduler entity */
    task->se.vruntime = 0;
    task->se.sum_exec_runtime = 0;
    task->se.prev_sum_exec_runtime = 0;
    task->se.rb_node.rb_parent_color = 0;
    task->se.rb_node.rb_right = NULL;
    task->se.rb_node.rb_left = NULL;
    task->se.rb_node.rb_parent = NULL;
    task->se.rb_node.rb_color = RB_BLACK;
    INIT_LIST_HEAD(&task->run_list);
    task->se.task = task;

    /* Initialize lists */
    INIT_LIST_HEAD(&task->children);
    INIT_LIST_HEAD(&task->sibling);
    task->parent = NULL;

    /* Initialize wait queue */
    wait_queue_init(&task->wait_queue);

    /* Set comm */
    snprintf(task->comm, sizeof(task->comm), "swapper/%u", cpu);

    /* Initialize reference count */
    atomic_set(&task->refcount, 1);

    /* Set as idle task */
    rq->idle = task;
    idle_tasks[cpu] = task;
    rq->curr = task;

    pr_debug("Idle task created for CPU %u\n", cpu);

    return 0;
}

/**
 * idle_task - Get idle task for CPU
 * @cpu: CPU number
 */
struct task_struct *idle_task(int cpu)
{
    if (cpu >= MAX_CPUS) {
        return NULL;
    }
    return idle_tasks[cpu];
}

/**
 * is_idle_task - Check if task is idle task
 * @task: Task to check
 */
bool is_idle_task(struct task_struct *task)
{
    if (!task) {
        return false;
    }
    return !!(task->flags & PF_IDLE);
}

/*===========================================================================*/
/*                         SCHEDULER INITIALIZATION                          */
/*===========================================================================*/

/**
 * scheduler_init - Initialize the scheduler subsystem
 *
 * This function initializes all scheduler data structures including:
 * - Per-CPU run queues
 * - Idle tasks for each CPU
 * - Scheduler state variables
 * - CFS and RT run queues
 */
void scheduler_init(void)
{
    u32 cpu;
    u32 num_cpus = get_num_cpus();

    pr_info("Initializing scheduler...\n");

    /* Initialize global scheduler lock */
    spin_lock_init(&scheduler_state.global_lock);

    /* Initialize per-CPU run queues */
    for (cpu = 0; cpu < num_cpus; cpu++) {
        init_rq(&run_queues[cpu], cpu);
    }

    /* Create idle tasks for each CPU */
    for (cpu = 0; cpu < num_cpus; cpu++) {
        if (create_idle_task(cpu) != 0) {
            pr_err("Failed to create idle task for CPU %u\n", cpu);
            /* Continue anyway - scheduler can work with partial idle tasks */
        }
    }

    /* Initialize scheduler tick */
    scheduler_tick_ns = NS_PER_TICK;

    scheduler_state.enabled = true;
    scheduler_state.started = false;

    pr_info("Scheduler initialized for %u CPU(s)\n", num_cpus);
}

/**
 * scheduler_start - Start the scheduler
 *
 * This function marks the scheduler as started and enables scheduling
 * on all online CPUs. After this call, the system is fully operational.
 */
void scheduler_start(void)
{
    u32 cpu;
    struct rq *rq;

    pr_info("Starting scheduler...\n");

    spin_lock(&scheduler_state.global_lock);

    if (scheduler_state.started) {
        pr_warn("Scheduler already started\n");
        spin_unlock(&scheduler_state.global_lock);
        return;
    }

    /* Set scheduler as started */
    scheduler_state.started = true;

    /* Initialize first CPU's run queue with idle task */
    rq = get_cpu_rq();
    rq->curr = idle_task(get_cpu_id());
    rq->prev = rq->curr;

    /* Enable interrupts */
    local_irq_enable();

    spin_unlock(&scheduler_state.global_lock);

    pr_info("Scheduler started successfully\n");

    /* Print scheduler statistics */
    scheduler_stats();
}

/**
 * scheduler_enable_all - Enable scheduler on all CPUs
 *
 * Called during SMP initialization to enable scheduling on all
 * secondary CPUs after they have been brought online.
 */
void scheduler_enable_all(void)
{
    u32 cpu;
    u32 num_cpus = get_num_cpus();

    pr_info("Enabling scheduler on all CPUs...\n");

    for (cpu = 0; cpu < num_cpus; cpu++) {
        struct rq *rq = get_rq(cpu);

        rq_lock(rq);
        rq->curr = idle_task(cpu);
        rq_unlock(rq);

        pr_debug("Scheduler enabled on CPU %u\n", cpu);
    }

    pr_info("All CPUs scheduler-enabled\n");
}

/*===========================================================================*/
/*                         CONTEXT SWITCH                                    */
/*===========================================================================*/

/**
 * prepare_task_switch - Prepare for task switch
 * @rq: Run queue
 * @prev: Previous task
 * @next: Next task
 */
static inline void prepare_task_switch(struct rq *rq,
                                       struct task_struct *prev,
                                       struct task_struct *next)
{
    /* Architecture-specific preparation */
    arch_prepare_switch(prev, next);
}

/**
 * finish_task_switch - Finish task switch
 * @rq: Run queue
 * @prev: Previous task
 */
static inline void finish_task_switch(struct rq *rq, struct task_struct *prev)
{
    /* Update statistics */
    rq->nr_switches++;
    scheduler_state.context_switch_count++;

    /* Architecture-specific finish */
    arch_finish_switch(prev);

    /* Put previous task if it's exiting */
    if (unlikely(prev->state & TASK_EXITING)) {
        put_task(prev);
    }
}

/**
 * context_switch - Perform context switch between tasks
 * @rq: Run queue
 * @prev: Previous (currently running) task
 * @next: Next task to run
 *
 * This is the core context switch function that saves the state of the
 * previous task and restores the state of the next task. It handles:
 * - Register state save/restore
 * - Stack switching
 * - Memory management context switch
 * - FPU state save/restore
 */
void context_switch(struct rq *rq, struct task_struct *prev,
                    struct task_struct *next)
{
    /* Update run queue clock */
    update_rq_clock(rq);

    /* Prepare for switch */
    prepare_task_switch(rq, prev, next);

    /* Switch memory context if needed */
    if (next->mm && next->mm != prev->mm) {
        arch_switch_mm(prev->mm, next->mm);
    }

    /* Perform the actual context switch */
    switch_to(prev, next);

    /* Finish the switch */
    finish_task_switch(rq, prev);
}

/**
 * switch_to - Architecture-specific context switch
 * @prev: Previous task
 * @next: Next task
 *
 * This function is typically implemented in architecture-specific code.
 * It performs the low-level register save/restore and stack switching.
 */
void switch_to(struct task_struct *prev, struct task_struct *next)
{
    /* Architecture-specific implementation required */
    arch_switch_to(prev, next);
}

/**
 * schedule_tail - Complete context switch after returning to new task
 * @prev: Previous task (from perspective of new task)
 *
 * Called after a context switch completes, from the perspective of
 * the newly scheduled task.
 */
void schedule_tail(struct task_struct *prev)
{
    struct rq *rq = get_cpu_rq();

    /* Update statistics */
    rq->nr_sched_events++;

    /* Check if we need to rebalance */
    if (unlikely(rq->nr_running > 1)) {
        rebalance_domains();
    }
}

/*===========================================================================*/
/*                         MAIN SCHEDULING FUNCTION                          */
/*===========================================================================*/

/**
 * pick_next_task - Pick the next task to run
 * @rq: Run queue
 *
 * This function selects the next task to run based on:
 * 1. Real-time tasks (FIFO, Round-Robin)
 * 2. CFS tasks (normal priority)
 * 3. Idle task (if nothing else is runnable)
 */
static struct task_struct *pick_next_task(struct rq *rq)
{
    struct task_struct *next = NULL;

    /* Try to pick RT task first */
    if (rq->rt.nr_running > 0) {
        next = pick_next_task_rt(rq);
        if (next) {
            return next;
        }
    }

    /* Try CFS task */
    if (rq->cfs.nr_running > 0) {
        next = pick_next_task_fair(rq);
        if (next) {
            return next;
        }
    }

    /* Fall back to idle task */
    return rq->idle;
}

/**
 * put_prev_task - Put previous task back on run queue
 * @rq: Run queue
 * @prev: Previous task
 */
static void put_prev_task(struct rq *rq, struct task_struct *prev)
{
    if (prev->policy >= SCHED_FIFO && prev->policy <= SCHED_RR) {
        put_prev_task_rt(rq, prev);
    } else {
        put_prev_task_fair(rq, prev);
    }
}

/**
 * __schedule - Core scheduling function (internal)
 *
 * This is the main scheduling function that:
 * 1. Selects the next task to run
 * 2. Performs context switch
 * 3. Updates scheduling statistics
 */
static void __schedule(void)
{
    struct task_struct *prev, *next;
    struct rq *rq;
    u64 flags;

    /* Get current run queue */
    rq = get_cpu_rq();

    /* Disable interrupts */
    local_irq_disable();

    /* Lock run queue */
    rq_lock_irqsave(rq, &flags);

    /* Get current task */
    prev = rq->curr;

    /* Update clock */
    update_rq_clock(rq);

    /* Update task statistics */
    if (prev->state == TASK_RUNNING) {
        u64 delta = rq->clock - prev->se.sum_exec_runtime;
        prev->se.sum_exec_runtime += delta;
        prev->utime += delta;
    }

    /* Check if previous task is still runnable */
    if (prev->state == TASK_RUNNING) {
        /* Put back on run queue */
        put_prev_task(rq, prev);
    }

    /* Pick next task */
    next = pick_next_task(rq);

    /* Check if we need to switch */
    if (likely(prev != next)) {
        /* Update run queue state */
        rq->prev = prev;
        rq->curr = next;

        /* Perform context switch */
        context_switch(rq, prev, next);

        /* Update statistics */
        scheduler_state.schedule_count++;
    }

    /* Unlock run queue */
    rq_unlock_irqrestore(rq, flags);

    /* Re-enable interrupts */
    local_irq_enable();
}

/**
 * schedule - Main scheduling entry point
 *
 * This is the primary function called to invoke the scheduler.
 * It should be called when:
 * - Current task blocks (sleep, wait for I/O)
 * - Time slice expires
 * - Higher priority task becomes runnable
 * - Explicit yield
 */
void schedule(void)
{
    /* Check if scheduler is enabled */
    if (unlikely(!scheduler_state.enabled)) {
        return;
    }

    /* Check preemption count */
    if (preempt_count() > 0) {
        return;
    }

    /* Call core scheduling function */
    __schedule();
}

/**
 * preempt_schedule - Schedule due to preemption
 *
 * Called when a task needs to be preempted by a higher priority task.
 */
void preempt_schedule(void)
{
    if (preempt_count() == 0) {
        __schedule();
    }
}

/*===========================================================================*/
/*                         PREEMPTION CONTROL                                */
/*===========================================================================*/

/**
 * preempt_disable - Disable preemption
 *
 * Disables voluntary preemption by incrementing the preemption count.
 * Must be balanced with preempt_enable().
 */
void preempt_disable(void)
{
    int *count = &per_cpu_preempt_count;
    (*count)++;
    barrier();
}

/**
 * preempt_enable - Enable preemption
 *
 * Decrements the preemption count and schedules if needed.
 * Must be balanced with preempt_disable().
 */
void preempt_enable(void)
{
    int *count = &per_cpu_preempt_count;

    barrier();
    (*count)--;

    if (unlikely(*count == 0) && need_resched()) {
        preempt_schedule();
    }
}

/**
 * preempt_count - Get current preemption count
 */
int preempt_count(void)
{
    int *count = &per_cpu_preempt_count;
    return *count;
}

/**
 * set_preempt_count - Set preemption count
 * @val: Value to set
 */
void set_preempt_count(int val)
{
    int *count = &per_cpu_preempt_count;
    *count = val;
}

/*===========================================================================*/
/*                         RUN QUEUE MANAGEMENT                              */
/*===========================================================================*/

/**
 * enqueue_task - Add task to run queue
 * @rq: Run queue
 * @task: Task to enqueue
 * @flags: Enqueue flags
 */
void enqueue_task(struct rq *rq, struct task_struct *task, int flags)
{
    if (task->policy >= SCHED_FIFO && task->policy <= SCHED_RR) {
        enqueue_task_rt(rq, task, flags);
    } else {
        enqueue_task_fair(rq, task, flags);
    }

    rq->nr_running++;
}

/**
 * dequeue_task - Remove task from run queue
 * @rq: Run queue
 * @task: Task to dequeue
 * @flags: Dequeue flags
 */
void dequeue_task(struct rq *rq, struct task_struct *task, int flags)
{
    if (task->policy >= SCHED_FIFO && task->policy <= SCHED_RR) {
        dequeue_task_rt(rq, task, flags);
    } else {
        dequeue_task_fair(rq, task, flags);
    }

    rq->nr_running--;
}

/**
 * activate_task - Activate a task (make runnable)
 * @rq: Run queue
 * @task: Task to activate
 */
void activate_task(struct rq *rq, struct task_struct *task)
{
    if (task->state == TASK_NEW) {
        task->state = TASK_RUNNING;
    }

    enqueue_task(rq, task, 0);
}

/**
 * deactivate_task - Deactivate a task (make non-runnable)
 * @rq: Run queue
 * @task: Task to deactivate
 */
void deactivate_task(struct rq *rq, struct task_struct *task)
{
    dequeue_task(rq, task, 0);
}

/*===========================================================================*/
/*                         TASK WAKEUP/SLEEP                                 */
/*===========================================================================*/

/**
 * try_to_wake_up - Try to wake up a task
 * @task: Task to wake up
 * @state: State mask to match
 * @flags: Wakeup flags
 */
int try_to_wake_up(struct task_struct *task, unsigned int state, int flags)
{
    struct rq *rq;
    u64 flags_irq;
    int success = 0;

    rq_lock_irqsave(get_rq(task->cpu), &flags_irq);

    if (task->state & state) {
        /* Task is sleeping, wake it up */
        task->state = TASK_RUNNING;

        /* Add to run queue */
        activate_task(get_rq(task->cpu), task);

        /* Check if we need to reschedule */
        if (task->prio < get_rq(task->cpu)->curr->prio) {
            resched_cpu(task->cpu);
        }

        success = 1;
    }

    rq_unlock_irqrestore(get_rq(task->cpu), flags_irq);

    return success;
}

/**
 * task_wake_up - Wake up a task
 * @task: Task to wake up
 */
int task_wake_up(struct task_struct *task)
{
    if (!task) {
        return -EINVAL;
    }
    return try_to_wake_up(task, TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE, 0);
}

/**
 * task_sleep - Put task to sleep
 * @task: Task to sleep
 */
int task_sleep(struct task_struct *task)
{
    struct rq *rq;

    if (!task) {
        return -EINVAL;
    }

    rq = get_rq(task->cpu);

    rq_lock(rq);

    if (task->state == TASK_RUNNING) {
        deactivate_task(rq, task);
        task->state = TASK_INTERRUPTIBLE;

        /* Schedule away */
        if (task == rq->curr) {
            rq_unlock(rq);
            schedule();
        } else {
            rq_unlock(rq);
        }
    } else {
        rq_unlock(rq);
    }

    return 0;
}

/**
 * task_stop - Stop a task
 * @task: Task to stop
 */
int task_stop(struct task_struct *task)
{
    if (!task) {
        return -EINVAL;
    }

    task->state = TASK_STOPPED;
    return 0;
}

/**
 * task_continue - Continue a stopped task
 * @task: Task to continue
 */
int task_continue(struct task_struct *task)
{
    if (!task) {
        return -EINVAL;
    }

    if (task->state == TASK_STOPPED) {
        task->state = TASK_RUNNING;
        task_wake_up(task);
    }

    return 0;
}

/**
 * task_kill - Send signal to task
 * @task: Target task
 * @signal: Signal number
 */
int task_kill(struct task_struct *task, int signal)
{
    if (!task) {
        return -EINVAL;
    }

    /* Architecture-specific signal delivery */
    return arch_task_kill(task, signal);
}

/*===========================================================================*/
/*                         LOAD BALANCING                                    */
/*===========================================================================*/

/**
 * rebalance_domains - Rebalance load across CPUs
 *
 * This function is called periodically to balance the load across
 * all CPUs in the system. It migrates tasks from overloaded CPUs
 * to underutilized CPUs.
 */
void rebalance_domains(void)
{
    u32 cpu;
    u32 num_cpus = get_num_cpus();
    struct rq *rq;
    unsigned long total_load = 0;
    unsigned long avg_load;

    /* Calculate total load */
    for (cpu = 0; cpu < num_cpus; cpu++) {
        rq = get_rq(cpu);
        total_load += rq->cfs.load;
    }

    /* Calculate average load */
    avg_load = total_load / num_cpus;

    /* Check each CPU for imbalance */
    for (cpu = 0; cpu < num_cpus; cpu++) {
        rq = get_rq(cpu);

        if (rq->cfs.load > avg_load * 11 / 10) {
            /* CPU is overloaded, try to migrate tasks */
            idle_balance(cpu);
        }
    }
}

/**
 * idle_balance - Balance load when CPU goes idle
 * @cpu: CPU that is going idle
 *
 * Called when a CPU's run queue becomes empty. Attempts to pull
 * tasks from other CPUs to keep this CPU busy.
 */
void idle_balance(int cpu)
{
    struct rq *rq = get_rq(cpu);
    u32 src_cpu;
    u32 num_cpus = get_num_cpus();
    struct task_struct *task = NULL;

    /* Try to pull task from other CPUs */
    for (src_cpu = 0; src_cpu < num_cpus; src_cpu++) {
        struct rq *src_rq;

        if (src_cpu == cpu) {
            continue;
        }

        src_rq = get_rq(src_cpu);

        /* Check if source CPU has tasks */
        if (src_rq->cfs.nr_running > 1) {
            /* Try to pull a task */
            task = pull_task(src_rq, rq);
            if (task) {
                pr_debug("Pulled task %s from CPU %u to CPU %u\n",
                         task->comm, src_cpu, cpu);
                break;
            }
        }
    }
}

/**
 * pull_task - Pull a task from one run queue to another
 * @src_rq: Source run queue
 * @dst_rq: Destination run queue
 */
static struct task_struct *pull_task(struct rq *src_rq, struct rq *dst_rq)
{
    struct task_struct *task = NULL;
    struct rb_node *node;

    /* Find leftmost (earliest vruntime) task in CFS */
    node = src_rq->cfs.rb_leftmost;
    if (node) {
        task = rb_entry(node, struct task_struct, se.rb_node);

        /* Check if task can run on destination CPU */
        if (task->cpus_allowed & (1ULL << dst_rq->cpu)) {
            /* Dequeue from source */
            dequeue_task_fair(src_rq, task, 0);
            src_rq->nr_running--;

            /* Enqueue to destination */
            enqueue_task_fair(dst_rq, task, 0);
            dst_rq->nr_running++;

            /* Wake up destination CPU if needed */
            if (dst_rq->curr == dst_rq->idle) {
                resched_cpu(dst_rq->cpu);
            }
        } else {
            task = NULL;
        }
    }

    return task;
}

/**
 * resched_cpu - Force reschedule on CPU
 * @cpu: CPU to reschedule
 */
void resched_cpu(int cpu)
{
    struct rq *rq = get_rq(cpu);

    if (cpu == get_cpu_id()) {
        /* Local CPU - set reschedule flag */
        set_need_resched();
    } else {
        /* Remote CPU - send IPI */
        send_ipi(cpu, IPI_RESCHEDULE);
    }
}

/*===========================================================================*/
/*                         CURRENT TASK ACCESS                               */
/*===========================================================================*/

/**
 * get_current - Get current running task
 *
 * Returns the task_struct of the currently running task on this CPU.
 */
struct task_struct *get_current(void)
{
    struct rq *rq = get_cpu_rq();
    return rq->curr;
}

/**
 * get_task - Increment task reference count
 * @task: Task to reference
 */
void get_task(struct task_struct *task)
{
    if (task) {
        atomic_inc(&task->refcount);
    }
}

/**
 * put_task - Decrement task reference count
 * @task: Task to dereference
 */
void put_task(struct task_struct *task)
{
    if (task && atomic_dec_and_test(&task->refcount)) {
        /* Reference count reached zero, free task */
        kfree(task);
    }
}

/*===========================================================================*/
/*                         SCHEDULER STATISTICS                              */
/*===========================================================================*/

/**
 * scheduler_stats - Print scheduler statistics
 */
void scheduler_stats(void)
{
    u32 cpu;
    u32 num_cpus = get_num_cpus();

    printk("\n=== Scheduler Statistics ===\n");
    printk("Scheduler Enabled: %s\n", scheduler_state.enabled ? "Yes" : "No");
    printk("Scheduler Started: %s\n", scheduler_state.started ? "Yes" : "No");
    printk("Total Schedule Calls: %llu\n", (unsigned long long)scheduler_state.schedule_count);
    printk("Total Context Switches: %llu\n", (unsigned long long)scheduler_state.context_switch_count);
    printk("\n");

    for (cpu = 0; cpu < num_cpus; cpu++) {
        struct rq *rq = get_rq(cpu);

        printk("CPU %u:\n", cpu);
        printk("  Running Tasks: %u\n", rq->nr_running);
        printk("  CFS Tasks: %u\n", rq->cfs.nr_running);
        printk("  RT Tasks: %u\n", rq->rt.nr_running);
        printk("  Context Switches: %llu\n", (unsigned long long)rq->nr_switches);
        printk("  Current Task: %s (PID: %u)\n",
               rq->curr ? rq->curr->comm : "NULL",
               rq->curr ? rq->curr->pid : 0);
        printk("\n");
    }
}

/**
 * task_stats - Print task statistics
 * @task: Task to show stats for
 */
void task_stats(struct task_struct *task)
{
    if (!task) {
        return;
    }

    printk("\n=== Task Statistics: %s (PID: %u) ===\n", task->comm, task->pid);
    printk("State: 0x%lx\n", task->state);
    printk("Flags: 0x%x\n", task->flags);
    printk("Priority: %d (Static: %d, Normal: %d)\n",
           task->prio, task->static_prio, task->normal_prio);
    printk("RT Priority: %u\n", task->rt_priority);
    printk("Policy: %u\n", task->policy);
    printk("CPU: %u (Last: %u)\n", task->cpu, task->last_cpu);
    printk("CPUs Allowed: 0x%llx\n", (unsigned long long)task->cpus_allowed);
    printk("Virtual Runtime: %llu ns\n", (unsigned long long)task->se.vruntime);
    printk("Sum Exec Runtime: %llu ns\n", (unsigned long long)task->se.sum_exec_runtime);
    printk("Voluntary Context Switches: %llu\n", (unsigned long long)task->nvcsw);
    printk("Involuntary Context Switches: %llu\n", (unsigned long long)task->nivcsw);
    printk("User Time: %llu ns\n", (unsigned long long)task->utime);
    printk("System Time: %llu ns\n", (unsigned long long)task->stime);
    printk("\n");
}

/*===========================================================================*/
/*                         SCHEDULER TICK                                    */
/*===========================================================================*/

/**
 * scheduler_tick - Handle scheduler tick
 *
 * Called at each timer tick to update scheduling state and trigger
 * rescheduling if needed.
 */
void scheduler_tick(void)
{
    struct rq *rq = get_cpu_rq();
    struct task_struct *curr;
    u64 flags;

    rq_lock_irqsave(rq, &flags);

    /* Update clock */
    update_rq_clock(rq);

    /* Get current task */
    curr = rq->curr;

    /* Update task runtime */
    if (curr && curr != rq->idle) {
        u64 delta = NS_PER_TICK;

        curr->se.sum_exec_runtime += delta;

        /* Update CFS vruntime */
        if (curr->policy == SCHED_NORMAL) {
            update_curr_fair(rq, curr);
        }

        /* Check if time slice expired */
        if (need_resched()) {
            set_need_resched();
        }
    }

    /* Update RT runtime */
    update_rt_runtime(rq);

    /* Trigger load balancing periodically */
    if (scheduler_state.tick_count % HZ == 0) {
        rebalance_domains();
    }

    scheduler_state.tick_count++;

    rq_unlock_irqrestore(rq, flags);
}

/*===========================================================================*/
/*                         ARCHITECTURE INTERFACES                           */
/*===========================================================================*/

/* These functions must be implemented by architecture-specific code */

extern void arch_idle_cpu(void);
extern void arch_prepare_switch(struct task_struct *prev, struct task_struct *next);
extern void arch_finish_switch(struct task_struct *prev);
extern void arch_switch_mm(struct address_space *prev_mm, struct address_space *next_mm);
extern void arch_switch_to(struct task_struct *prev, struct task_struct *next);
extern int arch_task_kill(struct task_struct *task, int signal);

/* Per-CPU need reschedule flag */
static DEFINE_PER_CPU(bool, need_resched_flag) = false;

void set_need_resched(void)
{
    bool *flag = &need_resched_flag;
    *flag = true;
}

bool need_resched(void)
{
    bool *flag = &need_resched_flag;
    return *flag;
}

void clear_need_resched(void)
{
    bool *flag = &need_resched_flag;
    *flag = false;
}
