/*
 * NEXUS OS - Completely Fair Scheduler (CFS)
 * kernel/sched/cfs.c
 */

#include "sched.h"
#include "../include/types.h"
#include "../include/kernel.h"

/* Forward declarations */
bool vruntime_before(u64 a, u64 b);
void rb_erase(struct rb_node *node, struct rb_root *root);
static inline void rb_insert_color_cached(struct rb_node *node,
                                           struct rb_root *root,
                                           struct rb_node **leftmost);

/* RB-tree link node */
void rb_link_node(struct rb_node *node, struct rb_node *parent,
                  struct rb_node **link)
{
    node->rb_parent_color = (unsigned long)parent;
    node->rb_left = NULL;
    node->rb_right = NULL;
    *link = node;
}

/*===========================================================================*/
/*                         CFS CONFIGURATION                                 */
/*===========================================================================*/

/* CFS tunables */
static struct {
    /* Scheduling latency target (6ms default) */
    u64 sched_latency_ns;

    /* Minimum granularity (0.75ms default) */
    u64 sched_min_granularity_ns;

    /* Wakeup granularity (0.5ms default) */
    u64 sched_wakeup_granularity_ns;

    /* Migration cost (0.5ms default) */
    u64 sched_migration_cost_ns;

    /* Number of tasks for latency target */
    u64 sched_nr_latency;

    /* Runtime ratio for throttling */
    u64 sched_rt_runtime_us;
    u64 sched_rt_period_us;
} cfs_tunables = {
    .sched_latency_ns = 6000000ULL,           /* 6ms */
    .sched_min_granularity_ns = 750000ULL,    /* 0.75ms */
    .sched_wakeup_granularity_ns = 500000ULL, /* 0.5ms */
    .sched_migration_cost_ns = 500000ULL,     /* 0.5ms */
    .sched_nr_latency = 8,
    .sched_rt_runtime_us = 950000,
    .sched_rt_period_us = 1000000
};

/* Nice to weight conversion table */
static const unsigned long nice_to_weight[40] = {
    /*  -20 */     88761,
    /*  -19 */     71755,
    /*  -18 */     56483,
    /*  -17 */     46273,
    /*  -16 */     36291,
    /*  -15 */     29154,
    /*  -14 */     23254,
    /*  -13 */     18705,
    /*  -12 */     14949,
    /*  -11 */     11916,
    /*  -10 */      9548,
    /*   -9 */      7620,
    /*   -8 */      6157,
    /*   -7 */      4936,
    /*   -6 */      3903,
    /*   -5 */      3121,
    /*   -4 */      2501,
    /*   -3 */      1991,
    /*   -2 */      1586,
    /*   -1 */      1277,
    /*    0 */      1024,
    /*    1 */       820,
    /*    2 */       655,
    /*    3 */       526,
    /*    4 */       423,
    /*    5 */       335,
    /*    6 */       272,
    /*    7 */       215,
    /*    8 */       172,
    /*    9 */       139,
    /*   10 */       110,
    /*   11 */        87,
    /*   12 */        70,
    /*   13 */        56,
    /*   14 */        45,
    /*   15 */        36,
    /*   16 */        29,
    /*   17 */        23,
    /*   18 */        18,
    /*   19 */        15
};

/* Priority to nice conversion */
static const int prio_to_nice_table[40] = {
    /*  0 */  -20,
    /*  1 */  -19,
    /*  2 */  -18,
    /*  3 */  -17,
    /*  4 */  -16,
    /*  5 */  -15,
    /*  6 */  -14,
    /*  7 */  -13,
    /*  8 */  -12,
    /*  9 */  -11,
    /* 10 */  -10,
    /* 11 */   -9,
    /* 12 */   -8,
    /* 13 */   -7,
    /* 14 */   -6,
    /* 15 */   -5,
    /* 16 */   -4,
    /* 17 */   -3,
    /* 18 */   -2,
    /* 19 */   -1,
    /* 20 */    0,
    /* 21 */    1,
    /* 22 */    2,
    /* 23 */    3,
    /* 24 */    4,
    /* 25 */    5,
    /* 26 */    6,
    /* 27 */    7,
    /* 28 */    8,
    /* 29 */    9,
    /* 30 */   10,
    /* 31 */   11,
    /* 32 */   12,
    /* 33 */   13,
    /* 34 */   14,
    /* 35 */   15,
    /* 36 */   16,
    /* 37 */   17,
    /* 38 */   18,
    /* 39 */   19
};

/*===========================================================================*/
/*                         RB-TREE OPERATIONS                                */
/*===========================================================================*/

/**
 * rb_first - Get first (leftmost) node in RB-tree
 * @root: RB-tree root
 */
static inline struct rb_node *rb_first(struct rb_root *root)
{
    struct rb_node *node = root->rb_node;

    if (!node) {
        return NULL;
    }

    while (node->rb_left) {
        node = node->rb_left;
    }

    return node;
}

/**
 * rb_next - Get next node in RB-tree (in-order successor)
 * @node: Current node
 */
static inline struct rb_node *rb_next(struct rb_node *node)
{
    struct rb_node *parent;

    if (!node) {
        return NULL;
    }

    /* If we have a right child, go right then left */
    if (node->rb_right) {
        node = node->rb_right;
        while (node->rb_left) {
            node = node->rb_left;
        }
        return node;
    }

    /* Otherwise, go up until we're a left child */
    while ((parent = node->rb_parent) && node == parent->rb_right) {
        node = parent;
    }

    return parent;
}

/**
 * rb_erase_cached - Remove node from RB-tree with caching
 * @node: Node to remove
 * @root: RB-tree root
 * @leftmost: Pointer to leftmost node cache
 */
static inline void rb_erase_cached(struct rb_node *node, struct rb_root *root,
                                    struct rb_node **leftmost)
{
    bool is_leftmost = (node == *leftmost);

    rb_erase(node, root);

    if (is_leftmost) {
        *leftmost = rb_first(root);
    }
}

/**
 * rb_insert_color_cached - Insert node into RB-tree with caching
 * @node: Node to insert
 * @root: RB-tree root
 * @leftmost: Pointer to leftmost node cache
 */
static inline void rb_insert_color_cached(struct rb_node *node,
                                           struct rb_root *root,
                                           struct rb_node **leftmost)
{
    bool first = false;

    if (!*leftmost) {
        first = true;
    }

    rb_insert_color(node, root);

    if (first) {
        *leftmost = node;
    } else if (*leftmost && (*leftmost)->rb_parent == node) {
        *leftmost = node;
    }
}

/*===========================================================================*/
/*                         CFS RUN QUEUE MANAGEMENT                          */
/*===========================================================================*/

/**
 * cfs_rq_init - Initialize CFS run queue
 * @cfs_rq: CFS run queue to initialize
 * @rq: Parent run queue
 */
static void cfs_rq_init(struct cfs_rq *cfs_rq, struct rq *rq)
{
    cfs_rq->tasks_timeline = RB_ROOT;
    cfs_rq->rb_leftmost = NULL;
    cfs_rq->nr_running = 0;
    cfs_rq->nr_spread_over = 0;
    cfs_rq->min_vruntime = 0;
    cfs_rq->min_granularity = cfs_tunables.sched_min_granularity_ns;
    cfs_rq->granularity = cfs_tunables.sched_min_granularity_ns;
    cfs_rq->latency = cfs_tunables.sched_latency_ns;
    cfs_rq->runtime = 0;
    cfs_rq->runtime_total = 0;
    cfs_rq->load = 0;
    cfs_rq->load_avg = 0;
    cfs_rq->exec_clock = 0;
    cfs_rq->min_vruntime_runnable = 0;
    cfs_rq->rq = rq;
}

/**
 * cfs_rq_load - Calculate CFS run queue load
 * @cfs_rq: CFS run queue
 */
static inline unsigned long cfs_rq_load(struct cfs_rq *cfs_rq)
{
    return cfs_rq->load;
}

/**
 * cfs_rq_add_load - Add load to CFS run queue
 * @cfs_rq: CFS run queue
 * @load: Load to add
 */
static inline void cfs_rq_add_load(struct cfs_rq *cfs_rq, unsigned long load)
{
    cfs_rq->load += load;
}

/**
 * cfs_rq_sub_load - Subtract load from CFS run queue
 * @cfs_rq: CFS run queue
 * @load: Load to subtract
 */
static inline void cfs_rq_sub_load(struct cfs_rq *cfs_rq, unsigned long load)
{
    if (cfs_rq->load >= load) {
        cfs_rq->load -= load;
    } else {
        cfs_rq->load = 0;
    }
}

/*===========================================================================*/
/*                         VRUNTIME CALCULATION                              */
/*===========================================================================*/

/**
 * sched_slice - Calculate time slice for task
 * @cfs_rq: CFS run queue
 * @task: Task to calculate slice for
 *
 * Returns the time slice for a task based on its weight and
 * the number of runnable tasks.
 */
static inline u64 sched_slice(struct cfs_rq *cfs_rq, struct task_struct *task)
{
    u64 slice;
    unsigned int nr_running = cfs_rq->nr_running;

    /* Base slice from latency target */
    if (nr_running <= cfs_tunables.sched_nr_latency) {
        slice = cfs_tunables.sched_latency_ns / cfs_tunables.sched_nr_latency;
    } else {
        slice = cfs_tunables.sched_latency_ns / nr_running;
    }

    /* Ensure minimum granularity */
    if (slice < cfs_tunables.sched_min_granularity_ns) {
        slice = cfs_tunables.sched_min_granularity_ns;
    }

    /* Scale by weight */
    slice *= task->se.load_weight;
    slice /= cfs_rq->load;

    return slice;
}

/**
 * sched_vruntime_add - Add execution time to vruntime
 * @vruntime: Current vruntime
 * @delta: Execution time delta
 * @weight: Task weight
 *
 * Calculates new vruntime by adding weighted execution time.
 */
static inline u64 sched_vruntime_add(u64 vruntime, u64 delta, unsigned long weight)
{
    u64 vruntime_delta;

    /* Scale delta by weight (inverse relationship) */
    vruntime_delta = (delta << NICE_0_LOAD_SHIFT) / weight;

    return vruntime + vruntime_delta;
}

/**
 * update_curr - Update current task's vruntime
 * @cfs_rq: CFS run queue
 * @task: Current task
 *
 * Updates the vruntime of the currently running task based on
 * how long it has executed.
 */
void update_curr_fair(struct rq *rq, struct task_struct *task)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    u64 now = rq->clock;
    u64 delta_exec;
    u64 delta_vruntime;

    if (!task || task->policy != SCHED_NORMAL) {
        return;
    }

    /* Calculate execution delta */
    delta_exec = now - task->se.prev_sum_exec_runtime;
    if ((s64)delta_exec < 0) {
        delta_exec = 0;
    }

    task->se.prev_sum_exec_runtime = now;
    task->se.sum_exec_runtime += delta_exec;

    /* Update vruntime */
    delta_vruntime = (delta_exec << NICE_0_LOAD_SHIFT) / task->se.load_weight;
    task->se.vruntime += delta_vruntime;

    /* Update minimum vruntime */
    if (task->se.vruntime > cfs_rq->min_vruntime) {
        cfs_rq->min_vruntime = task->se.vruntime;
    }

    /* Update CFS exec clock */
    cfs_rq->exec_clock += delta_exec;
}

/**
 * place_entity - Place task at appropriate vruntime
 * @cfs_rq: CFS run queue
 * @task: Task to place
 * @initial: Is this an initial placement
 *
 * Places a task at an appropriate vruntime when it's being
 * added to the run queue. This ensures fair scheduling for
 * newly woken tasks.
 */
static void place_entity(struct cfs_rq *cfs_rq, struct task_struct *task, int initial)
{
    u64 vruntime = cfs_rq->min_vruntime;

    /* New tasks start at min_vruntime */
    if (initial) {
        task->se.vruntime = vruntime;
        return;
    }

    /* Woken tasks get a vruntime boost */
    vruntime -= cfs_tunables.sched_wakeup_granularity_ns;

    /* Don't go below minimum */
    if ((s64)vruntime < 0) {
        vruntime = 0;
    }

    task->se.vruntime = vruntime;
}

/*===========================================================================*/
/*                         TASK ENQUEUE/DEQUEUE                              */
/*===========================================================================*/

/**
 * enqueue_task_fair - Enqueue task to CFS run queue
 * @rq: Run queue
 * @task: Task to enqueue
 * @flags: Enqueue flags
 *
 * Adds a task to the CFS run queue's RB-tree, ordered by vruntime.
 */
void enqueue_task_fair(struct rq *rq, struct task_struct *task, int flags)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se = &task->se;
    struct rb_node **link = &cfs_rq->tasks_timeline.rb_node;
    struct rb_node *parent = NULL;
    struct sched_entity *entry;
    int leftmost = 1;

    /* Update task vruntime if needed */
    if (task->state == TASK_NEW) {
        place_entity(cfs_rq, task, 1);
        task->state = TASK_RUNNING;
    }

    /* Update load */
    cfs_rq_add_load(cfs_rq, se->load_weight);

    /* Find insertion point in RB-tree */
    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct sched_entity, rb_node);

        /* Compare vruntime to find position */
        if (vruntime_before(se->vruntime, entry->vruntime)) {
            link = &(*link)->rb_left;
        } else {
            link = &(*link)->rb_right;
            leftmost = 0;
        }
    }

    /* Insert node */
    rb_link_node(&se->rb_node, parent, link);
    rb_insert_color_cached(&se->rb_node, &cfs_rq->tasks_timeline,
                           &cfs_rq->rb_leftmost);

    /* Update statistics */
    cfs_rq->nr_running++;

    pr_debug("CFS: Enqueued %s (vruntime: %llu)\n",
             task->comm, (unsigned long long)se->vruntime);
}

/**
 * dequeue_task_fair - Dequeue task from CFS run queue
 * @rq: Run queue
 * @task: Task to dequeue
 * @flags: Dequeue flags
 *
 * Removes a task from the CFS run queue's RB-tree.
 */
void dequeue_task_fair(struct rq *rq, struct task_struct *task, int flags)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se = &task->se;

    /* Update task vruntime before removing */
    update_curr_fair(rq, task);

    /* Remove from RB-tree */
    rb_erase_cached(&se->rb_node, &cfs_rq->tasks_timeline, &cfs_rq->rb_leftmost);

    /* Update load */
    cfs_rq_sub_load(cfs_rq, se->load_weight);

    /* Update statistics */
    cfs_rq->nr_running--;

    pr_debug("CFS: Dequeued %s (vruntime: %llu)\n",
             task->comm, (unsigned long long)se->vruntime);
}

/**
 * put_prev_task_fair - Put previous task back on CFS run queue
 * @rq: Run queue
 * @prev: Previous task
 *
 * Called when a task is being switched out. Updates its vruntime
 * and re-enqueues it.
 */
void put_prev_task_fair(struct rq *rq, struct task_struct *prev)
{
    struct sched_entity *se = &prev->se;

    /* Update vruntime */
    update_curr_fair(rq, prev);

    /* Re-enqueue if still runnable */
    if (prev->state == TASK_RUNNING) {
        enqueue_task_fair(rq, prev, 0);
    }
}

/*===========================================================================*/
/*                         TASK SELECTION                                    */
/*===========================================================================*/

/**
 * pick_next_task_fair - Pick next task from CFS run queue
 * @rq: Run queue
 *
 * Selects the task with the smallest vruntime (leftmost node
 * in the RB-tree) to run next.
 *
 * Returns: Next task to run, or NULL if no runnable tasks
 */
struct task_struct *pick_next_task_fair(struct rq *rq)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se;
    struct task_struct *task;

    /* Check if there are runnable tasks */
    if (!cfs_rq->nr_running) {
        return NULL;
    }

    /* Get leftmost (earliest vruntime) entity */
    se = rb_entry(cfs_rq->rb_leftmost, struct sched_entity, rb_node);
    if (!se) {
        return NULL;
    }

    task = se->task;
    if (!task) {
        return NULL;
    }

    /* Update minimum vruntime */
    if (cfs_rq->min_vruntime < se->vruntime) {
        cfs_rq->min_vruntime = se->vruntime;
    }

    pr_debug("CFS: Picked %s (vruntime: %llu)\n",
             task->comm, (unsigned long long)se->vruntime);

    return task;
}

/**
 * check_preempt_curr_fair - Check if new task should preempt current
 * @rq: Run queue
 * @task: New task
 * @flags: Preempt flags
 *
 * Checks if a newly woken task should preempt the currently
 * running task based on vruntime comparison.
 */
void check_preempt_curr_fair(struct rq *rq, struct task_struct *task, int flags)
{
    struct task_struct *curr = rq->curr;
    u64 vruntime_diff;

    if (!curr || curr->policy != SCHED_NORMAL) {
        return;
    }

    /* Update current task's vruntime */
    update_curr_fair(rq, curr);

    /* Compare vruntime */
    vruntime_diff = curr->se.vruntime - task->se.vruntime;

    /* Preempt if new task has significantly smaller vruntime */
    if (vruntime_diff > cfs_tunables.sched_wakeup_granularity_ns) {
        resched_cpu(rq->cpu);
    }
}

/*===========================================================================*/
/*                         PRIORITY/NICE HANDLING                            */
/*===========================================================================*/

/**
 * nice_to_prio - Convert nice value to priority
 * @nice: Nice value (-20 to 19)
 *
 * Converts a nice value to internal priority representation.
 */
static int nice_to_prio(int nice)
{
    if (nice < -20) {
        nice = -20;
    }
    if (nice > 19) {
        nice = 19;
    }

    return MAX_RT_PRIO + nice;
}

/**
 * prio_to_nice - Convert priority to nice value
 * @prio: Priority value
 */
static int prio_to_nice(int prio)
{
    if (prio < MAX_RT_PRIO) {
        return -20;
    }
    if (prio >= MAX_PRIO) {
        return 19;
    }

    return prio - MAX_RT_PRIO;
}

/**
 * task_nice - Get task's nice value
 * @task: Task to query
 */
long task_nice(struct task_struct *task)
{
    return prio_to_nice(task->static_prio);
}

/**
 * set_user_nice - Set task's nice value
 * @task: Task to modify
 * @nice: New nice value
 *
 * Changes a task's nice value, which affects its scheduling weight.
 */
void set_user_nice(struct task_struct *task, long nice)
{
    struct rq *rq;
    u64 flags;
    int new_prio;
    unsigned long new_weight;
    bool queued;

    /* Clamp nice value */
    if (nice < -20) {
        nice = -20;
    }
    if (nice > 19) {
        nice = 19;
    }

    rq = get_rq(task->cpu);
    rq_lock_irqsave(rq, &flags);

    /* Check if task is on run queue */
    queued = (task->state == TASK_RUNNING);

    /* Dequeue if queued */
    if (queued) {
        dequeue_task_fair(rq, task, 0);
    }

    /* Calculate new priority and weight */
    new_prio = nice_to_prio((int)nice);
    new_weight = nice_to_weight[nice + 20];

    /* Update task properties */
    task->static_prio = new_prio;
    task->normal_prio = new_prio;
    task->prio = new_prio;
    task->se.load_weight = new_weight;

    /* Re-enqueue if was queued */
    if (queued) {
        enqueue_task_fair(rq, task, 0);
    }

    rq_unlock_irqrestore(rq, flags);

    pr_debug("Nice changed for %s: %ld\n", task->comm, nice);
}

/**
 * set_scheduler_class - Set task's scheduler class
 * @task: Task to modify
 * @policy: New scheduler policy
 *
 * Changes a task's scheduling policy (SCHED_NORMAL, SCHED_FIFO, etc.)
 */
int set_scheduler_class(struct task_struct *task, int policy)
{
    struct rq *rq;
    u64 flags;
    int old_policy;

    if (!task) {
        return -EINVAL;
    }

    /* Validate policy */
    if (policy < SCHED_NORMAL || policy > SCHED_DEADLINE) {
        return -EINVAL;
    }

    rq = get_rq(task->cpu);
    rq_lock_irqsave(rq, &flags);

    old_policy = task->policy;

    /* Dequeue from current class */
    if (task->state == TASK_RUNNING) {
        if (old_policy == SCHED_NORMAL) {
            dequeue_task_fair(rq, task, 0);
        } else if (old_policy == SCHED_FIFO || old_policy == SCHED_RR) {
            dequeue_task_rt(rq, task, 0);
        }
    }

    /* Update policy */
    task->policy = policy;

    /* Set appropriate priority */
    if (policy == SCHED_NORMAL) {
        task->prio = DEFAULT_PRIO;
        task->static_prio = DEFAULT_PRIO;
        task->normal_prio = DEFAULT_PRIO;
        task->se.load_weight = NICE_0_LOAD;
    } else if (policy == SCHED_FIFO || policy == SCHED_RR) {
        task->prio = MAX_RT_PRIO - 1 - task->rt_priority;
        task->static_prio = task->prio;
        task->normal_prio = task->prio;
    }

    /* Enqueue to new class */
    if (task->state == TASK_RUNNING) {
        if (policy == SCHED_NORMAL) {
            enqueue_task_fair(rq, task, 0);
        } else if (policy == SCHED_FIFO || policy == SCHED_RR) {
            enqueue_task_rt(rq, task, 0);
        }
    }

    rq_unlock_irqrestore(rq, flags);

    return 0;
}

/**
 * get_scheduler_class - Get task's scheduler class
 * @task: Task to query
 */
int get_scheduler_class(struct task_struct *task)
{
    if (!task) {
        return -EINVAL;
    }

    return task->policy;
}

/*===========================================================================*/
/*                         CPU AFFINITY                                      */
/*===========================================================================*/

/**
 * set_cpus_allowed - Set task's CPU affinity mask
 * @task: Task to modify
 * @cpus_allowed: New CPU mask
 *
 * Sets which CPUs a task is allowed to run on.
 */
int set_cpus_allowed(struct task_struct *task, u64 cpus_allowed)
{
    struct rq *rq;
    u64 flags;

    if (!task) {
        return -EINVAL;
    }

    /* Check for valid mask */
    if (cpus_allowed == 0) {
        return -EINVAL;
    }

    rq = get_rq(task->cpu);
    rq_lock_irqsave(rq, &flags);

    task->cpus_allowed = cpus_allowed;

    /* Migrate if current CPU not in new mask */
    if (!(cpus_allowed & (1ULL << task->cpu))) {
        u32 new_cpu = __builtin_ctzll(cpus_allowed);
        if (new_cpu < MAX_CPUS) {
            task->cpu = new_cpu;
        }
    }

    rq_unlock_irqrestore(rq, flags);

    return 0;
}

/**
 * get_cpus_allowed - Get task's CPU affinity mask
 * @task: Task to query
 */
u64 get_cpus_allowed(struct task_struct *task)
{
    if (!task) {
        return 0;
    }

    return task->cpus_allowed;
}

/*===========================================================================*/
/*                         CFS STATISTICS                                    */
/*===========================================================================*/

/**
 * cfs_stats - Print CFS statistics
 */
void cfs_stats(void)
{
    u32 cpu;
    u32 num_cpus = get_num_cpus();

    printk("\n=== CFS Statistics ===\n");
    printk("\nTunables:\n");
    printk("  Latency Target: %llu ms\n",
           (unsigned long long)cfs_tunables.sched_latency_ns / 1000000);
    printk("  Min Granularity: %llu us\n",
           (unsigned long long)cfs_tunables.sched_min_granularity_ns / 1000);
    printk("  Wakeup Granularity: %llu us\n",
           (unsigned long long)cfs_tunables.sched_wakeup_granularity_ns / 1000);
    printk("  Nr Latency: %llu\n",
           (unsigned long long)cfs_tunables.sched_nr_latency);
    printk("\n");

    for (cpu = 0; cpu < num_cpus; cpu++) {
        struct rq *rq = get_rq(cpu);
        struct cfs_rq *cfs_rq = &rq->cfs;

        printk("CPU %u:\n", cpu);
        printk("  Running Tasks: %u\n", cfs_rq->nr_running);
        printk("  Load: %lu\n", cfs_rq->load);
        printk("  Min Vruntime: %llu ns\n",
               (unsigned long long)cfs_rq->min_vruntime);
        printk("  Exec Clock: %llu ns\n",
               (unsigned long long)cfs_rq->exec_clock);
        printk("\n");
    }
}

/*===========================================================================*/
/*                         HELPER FUNCTIONS                                  */
/*===========================================================================*/

/**
 * vruntime_before - Compare two vruntime values
 * @a: First vruntime
 * @b: Second vruntime
 *
 * Compares two vruntime values, handling wraparound correctly.
 * Returns true if a is before b.
 */
bool vruntime_before(u64 a, u64 b)
{
    return (s64)(a - b) < 0;
}

/**
 * calc_delta_fair - Calculate fair delta for load
 * @delta: Time delta
 * @task: Task
 *
 * Scales a time delta by the task's weight.
 */
static inline u64 calc_delta_fair(u64 delta, struct task_struct *task)
{
    if (task->se.load_weight != NICE_0_LOAD) {
        delta = (delta << NICE_0_LOAD_SHIFT) / task->se.load_weight;
    }

    return delta;
}

/*===========================================================================*/
/*                         RB-TREE IMPLEMENTATION                            */
/*===========================================================================*/

/**
 * rb_erase - Remove node from RB-tree
 * @node: Node to remove
 * @root: RB-tree root
 *
 * Standard RB-tree node removal implementation.
 */
void rb_erase(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *child, *parent, *old = node;
    int color;

    /* Find replacement node */
    if (!node->rb_left) {
        child = node->rb_right;
    } else if (!node->rb_right) {
        child = node->rb_left;
    } else {
        /* Node has two children - find successor */
        old = node->rb_right;
        while (old->rb_left) {
            old = old->rb_left;
        }
        child = old->rb_right;
        parent = old->rb_parent;
        color = old->rb_color;

        if (child) {
            child->rb_parent = parent;
        }

        if (parent) {
            if (parent->rb_left == old) {
                parent->rb_left = child;
            } else {
                parent->rb_right = child;
            }
        } else {
            root->rb_node = child;
        }

        if (old->rb_parent == node) {
            parent = old;
        }

        /* Copy data from successor to original node */
        old->rb_parent = node->rb_parent;
        old->rb_color = node->rb_color;
        old->rb_left = node->rb_left;
        old->rb_right = node->rb_right;

        if (node->rb_parent) {
            parent = node->rb_parent;
            if (parent->rb_left == node) {
                parent->rb_left = old;
            } else {
                parent->rb_right = old;
            }
        } else {
            root->rb_node = old;
        }

        node->rb_left->rb_parent = old;
        if (node->rb_right) {
            node->rb_right->rb_parent = old;
        }

        goto fix_color;
    }

    /* Node has 0 or 1 child */
    parent = node->rb_parent;
    color = node->rb_color;

    if (child) {
        child->rb_parent = parent;
    }

    if (parent) {
        if (parent->rb_left == node) {
            parent->rb_left = child;
        } else {
            parent->rb_right = child;
        }
    } else {
        root->rb_node = child;
    }

fix_color:
    /* Fix RB-tree properties if needed */
    if (color == RB_BLACK && child && child->rb_color == RB_BLACK) {
        rb_erase_fixup(root, child, parent);
    }
}

/**
 * rb_erase_fixup - Fix RB-tree after removal
 * @root: RB-tree root
 * @node: Current node
 * @parent: Parent node
 *
 * Restores RB-tree properties after node removal.
 */
void rb_erase_fixup(struct rb_root *root, struct rb_node *node,
                    struct rb_node *parent)
{
    struct rb_node *sibling;

    while ((!node || node->rb_color == RB_BLACK) && node != root->rb_node) {
        if (parent->rb_left == node) {
            sibling = parent->rb_right;

            if (sibling && sibling->rb_color == RB_RED) {
                sibling->rb_color = RB_BLACK;
                parent->rb_color = RB_RED;
                rb_rotate_left(root, parent);
                sibling = parent->rb_right;
            }

            if ((!sibling->rb_left || sibling->rb_left->rb_color == RB_BLACK) &&
                (!sibling->rb_right || sibling->rb_right->rb_color == RB_BLACK)) {
                sibling->rb_color = RB_RED;
                node = parent;
                parent = parent->rb_parent;
            } else {
                if (!sibling->rb_right || sibling->rb_right->rb_color == RB_BLACK) {
                    if (sibling->rb_left) {
                        sibling->rb_left->rb_color = RB_BLACK;
                    }
                    sibling->rb_color = RB_RED;
                    rb_rotate_right(root, sibling);
                    sibling = parent->rb_right;
                }

                sibling->rb_color = parent->rb_color;
                parent->rb_color = RB_BLACK;
                if (sibling->rb_right) {
                    sibling->rb_right->rb_color = RB_BLACK;
                }
                rb_rotate_left(root, parent);
                node = root->rb_node;
                break;
            }
        } else {
            /* Mirror image of above */
            sibling = parent->rb_left;

            if (sibling && sibling->rb_color == RB_RED) {
                sibling->rb_color = RB_BLACK;
                parent->rb_color = RB_RED;
                rb_rotate_right(root, parent);
                sibling = parent->rb_left;
            }

            if ((!sibling->rb_right || sibling->rb_right->rb_color == RB_BLACK) &&
                (!sibling->rb_left || sibling->rb_left->rb_color == RB_BLACK)) {
                sibling->rb_color = RB_RED;
                node = parent;
                parent = parent->rb_parent;
            } else {
                if (!sibling->rb_left || sibling->rb_left->rb_color == RB_BLACK) {
                    if (sibling->rb_right) {
                        sibling->rb_right->rb_color = RB_BLACK;
                    }
                    sibling->rb_color = RB_RED;
                    rb_rotate_left(root, sibling);
                    sibling = parent->rb_left;
                }

                sibling->rb_color = parent->rb_color;
                parent->rb_color = RB_BLACK;
                if (sibling->rb_left) {
                    sibling->rb_left->rb_color = RB_BLACK;
                }
                rb_rotate_right(root, parent);
                node = root->rb_node;
                break;
            }
        }
    }

    if (node) {
        node->rb_color = RB_BLACK;
    }
}

/**
 * rb_rotate_left - Left rotation in RB-tree
 * @root: RB-tree root
 * @node: Node to rotate
 */
void rb_rotate_left(struct rb_root *root, struct rb_node *node)
{
    struct rb_node *right = node->rb_right;
    struct rb_node *parent = node->rb_parent;

    node->rb_right = right->rb_left;
    if (right->rb_left) {
        right->rb_left->rb_parent = node;
    }

    right->rb_parent = parent;
    right->rb_left = node;
    node->rb_parent = right;

    if (parent) {
        if (parent->rb_left == node) {
            parent->rb_left = right;
        } else {
            parent->rb_right = right;
        }
    } else {
        root->rb_node = right;
    }
}

/**
 * rb_rotate_right - Right rotation in RB-tree
 * @root: RB-tree root
 * @node: Node to rotate
 */
void rb_rotate_right(struct rb_root *root, struct rb_node *node)
{
    struct rb_node *left = node->rb_left;
    struct rb_node *parent = node->rb_parent;

    node->rb_left = left->rb_right;
    if (left->rb_right) {
        left->rb_right->rb_parent = node;
    }

    left->rb_parent = parent;
    left->rb_right = node;
    node->rb_parent = left;

    if (parent) {
        if (parent->rb_left == node) {
            parent->rb_left = left;
        } else {
            parent->rb_right = left;
        }
    } else {
        root->rb_node = left;
    }
}

/**
 * rb_insert_color - Fix RB-tree after insertion
 * @node: Inserted node
 * @root: RB-tree root
 *
 * Restores RB-tree properties after node insertion.
 */
void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *parent, *grandparent, *uncle;

    while ((parent = node->rb_parent) && parent->rb_color == RB_RED) {
        grandparent = parent->rb_parent;

        if (parent == grandparent->rb_left) {
            uncle = grandparent->rb_right;

            if (uncle && uncle->rb_color == RB_RED) {
                parent->rb_color = RB_BLACK;
                uncle->rb_color = RB_BLACK;
                grandparent->rb_color = RB_RED;
                node = grandparent;
            } else {
                if (node == parent->rb_right) {
                    rb_rotate_left(root, parent);
                    node = parent;
                    parent = node->rb_parent;
                }

                parent->rb_color = RB_BLACK;
                grandparent->rb_color = RB_RED;
                rb_rotate_right(root, grandparent);
            }
        } else {
            uncle = grandparent->rb_left;

            if (uncle && uncle->rb_color == RB_RED) {
                parent->rb_color = RB_BLACK;
                uncle->rb_color = RB_BLACK;
                grandparent->rb_color = RB_RED;
                node = grandparent;
            } else {
                if (node == parent->rb_left) {
                    rb_rotate_right(root, parent);
                    node = parent;
                    parent = node->rb_parent;
                }

                parent->rb_color = RB_BLACK;
                grandparent->rb_color = RB_RED;
                rb_rotate_left(root, grandparent);
            }
        }
    }

    root->rb_node->rb_color = RB_BLACK;
}
