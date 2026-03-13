/*
 * NEXUS OS - Scheduler Header
 * kernel/sched/sched.h
 */

#ifndef _NEXUS_SCHED_H
#define _NEXUS_SCHED_H

#include "../include/kernel.h"
#include "../sync/sync.h"

/*===========================================================================*/
/*                         SCHEDULER CONSTANTS                               */
/*===========================================================================*/

/* Scheduler Classes */
#define SCHED_NORMAL          0
#define SCHED_FIFO            1
#define SCHED_RR              2
#define SCHED_BATCH           3
#define SCHED_IDLE            4
#define SCHED_DEADLINE        5

/* Task States */
#define TASK_RUNNING          0x00000000
#define TASK_INTERRUPTIBLE    0x00000001
#define TASK_UNINTERRUPTIBLE  0x00000002
#define TASK_STOPPED          0x00000004
#define TASK_TRACED           0x00000008
#define TASK_EXITING          0x00000010
#define TASK_DEAD             0x00000020
#define TASK_WAKEKILL         0x00000040
#define TASK_WAKING           0x00000080
#define TASK_NOLOAD           0x00000100
#define TASK_NEW              0x00000200

/* Task Flags */
#define PF_KTHREAD            0x00000001
#define PF_IO_WORKER          0x00000002
#define PF_IDLE               0x00000004
#define PF_EXITING            0x00000008
#define PF_VCPU               0x00000010
#define PF_FROZEN             0x00000020
#define PF_NOFREEZE           0x00000040
#define PF_KSWAPD             0x00000080
#define PF_MEMALLOC           0x00000100
#define PF_NPROC_EXCEEDED     0x00000200
#define PF_USED_MATH          0x00000400
#define PF_USED_ASYNC         0x00000800
#define PF_THREAD_BOUND       0x00001000

/* Enqueue Flags */
#define ENQUEUE_REPLENISH     0x0001
#define ENQUEUE_HEAD          0x0002
#define ENQUEUE_WAKING        0x0004

/* Priority Range */
#define MAX_RT_PRIO           100
#define MAX_PRIO              (MAX_RT_PRIO + 10)
#define MIN_PRIO              0
#define DEFAULT_PRIO          (MAX_RT_PRIO / 2)

/* Time Constants */
#define HZ                    1000
#define TICK_PER_SEC          HZ
#define MS_PER_SEC            1000
#define US_PER_SEC            1000000
#define NS_PER_SEC            1000000000

#define NS_PER_MS             (NS_PER_SEC / MS_PER_SEC)
#define NS_PER_US             (NS_PER_SEC / US_PER_SEC)
#define NS_PER_TICK           (NS_PER_SEC / HZ)

/* CFS Constants */
#define DEF_TIMESLICE         (5 * NS_PER_MS)
#define MAX_TIMESLICE         (800 * NS_PER_MS)
#define MIN_TIMESLICE         (1 * NS_PER_MS)

#define NICE_0_LOAD           1024
#define NICE_0_LOAD_SHIFT     10

/*===========================================================================*/
/*                         SCHEDULER STRUCTURES                              */
/*===========================================================================*/

/* Forward declarations */
struct task_struct;

/**
 * sched_entity - Scheduler entity for CFS
 */
struct sched_entity {
    /* RB-Tree Node */
    struct rb_node rb_node;

    /* Virtual Runtime */
    u64 vruntime;
    u64 prev_vruntime;

    /* Run Queue Link */
    struct list_head run_list;

    /* Parent/Children */
    struct sched_entity *parent;
    struct cfs_rq *cfs_rq;
    struct cfs_rq *my_q;

    /* Load Weight */
    unsigned long load_weight;

    /* Execution Time */
    u64 sum_exec_runtime;
    u64 prev_sum_exec_runtime;

    /* Statistics */
    u64 nr_migrations;
    u64 wait_start;
    u64 wait_max;
    u64 wait_count;
    u64 wait_sum;

    /* Deadline */
    u64 deadline;
    u64 runtime;
    u64 period;

    /* Task Backlink */
    struct task_struct *task;
};

/**
 * task_struct - Process/Task control block
 */
struct task_struct {
    /* Thread Info */
    volatile long state;
    u32 flags;
    u32 ptrace;

    /* Identification */
    pid_t pid;
    pid_t tgid;
    tid_t tid;
    uid_t uid;
    gid_t gid;

    /* Names */
    char comm[16];

    /* Priority */
    int prio;
    int static_prio;
    int normal_prio;
    unsigned int rt_priority;

    /* Scheduler Class */
    u32 policy;

    /* CPU Affinity */
    u64 cpus_allowed;
    u32 cpu;
    u32 last_cpu;

    /* Run Queue */
    struct list_head run_list;
    struct rb_node rb_node;

    /* Scheduler Entity (CFS) */
    struct sched_entity se;

    /* Context Switches */
    u64 nvcsw;
    u64 nivcsw;

    /* Real-time */
    struct {
        u64 timeout;
        u64 time_slice;
    } rt;

    /* Priority Inheritance */
    s32 pi_boosted_prio;

    /* Sleep/Wakeup */
    wait_queue_head_t wait_queue;
    void *sleeping_on;
    
    /* Stack */
    void *stack;
    void *thread_info;
    
    /* Memory */
    struct address_space *mm;
    struct address_space *active_mm;
    
    /* Children/Parent */
    struct task_struct *parent;
    struct list_head children;
    struct list_head sibling;
    
    /* Thread Local Storage */
    void *tls;
    
    /* Architecture Specific */
    void *arch;
    
    /* Reference Count */
    atomic_t refcount;
    
    /* Exit Code */
    int exit_code;
    
    /* Start Time */
    u64 start_time;
    
    /* CPU Statistics */
    u64 utime;
    u64 stime;
    u64 cutime;
    u64 cstime;
};

/**
 * process - Process structure
 */
struct process {
    pid_t pid;                      /* Process ID */
    pid_t ppid;                     /* Parent process ID */
    pid_t tgid;                     /* Thread group ID */
    uid_t uid;                      /* User ID */
    gid_t gid;                      /* Group ID */

    char name[64];                  /* Process name */
    char cmdline[256];              /* Command line */

    struct task_struct *leader;     /* Thread group leader */
    struct list_head threads;       /* Thread list */
    spinlock_t thread_lock;         /* Thread list lock */

    struct mm_struct *mm;           /* Memory descriptor */
    struct files_struct *files;     /* File descriptor table */
    struct fs_struct *fs;           /* Filesystem info */
    struct signal_struct *signal;   /* Signal handlers */

    u32 state;                      /* Process state */
    u32 flags;                      /* Process flags */

    u64 start_time;                 /* Start time */
    u64 exit_time;                  /* Exit time */
    int exit_code;                  /* Exit code */

    atomic_t refcount;              /* Reference count */
    struct list_head list;          /* Global process list */
};

/**
 * files_struct - File descriptor table
 */
struct files_struct {
    atomic_t count;
    spinlock_t file_lock;
    int max_fds;
    int next_fd;
    struct file **fd;
    fd_set *open_fds;
    fd_set *close_on_exec;
};

/**
 * path - Filesystem path
 */
struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};

/**
 * fs_struct - Filesystem info
 */
struct fs_struct {
    atomic_t count;
    spinlock_t lock;
    struct path root;
    struct path pwd;
};

/**
 * k_sigaction - Kernel signal action
 */
struct k_sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t sa_mask;
    unsigned long sa_flags;
};

/**
 * signal_struct - Signal handlers
 */
struct signal_struct {
    atomic_t count;
    spinlock_t siglock;
    struct k_sigaction action[64];
    sigset_t signal;
    sigset_t blocked;
    sigset_t ignored;
};

/**
 * cfs_rq - CFS Run Queue
 */
struct cfs_rq {
    /* RB-Tree Root */
    struct rb_root tasks_timeline;
    struct rb_node *rb_leftmost;
    
    /* Number of Tasks */
    unsigned int nr_running;
    u64 nr_spread_over;
    
    /* Minimum Virtual Runtime */
    u64 min_vruntime;
    
    /* Requested Scheduling Granularity */
    u64 min_granularity;
    u64 granularity;
    
    /* Latency */
    u64 latency;
    
    /* Runtime */
    u64 runtime;
    u64 runtime_total;
    
    /* Load */
    unsigned long load;
    unsigned long load_avg;
    
    /* Statistics */
    u64 exec_clock;
    u64 min_vruntime_runnable;
    
    /* Per-CPU */
    struct rq *rq;
};

/**
 * rt_rq - Real-Time Run Queue
 */
struct rt_rq {
    /* Priority Array */
    struct list_head queue[MAX_RT_PRIO];
    unsigned int nr_running;

    /* Time */
    u64 time;
    u64 rt_runtime;
    u64 rt_period;

    /* Throttling */
    int throttled;
    int throttled_clock;

    /* Push/Pull */
    int pushable_tasks;
    struct list_head pushable_tasks_list;

    /* Per-CPU */
    struct rq *rq;
};

/**
 * rq - Run Queue (per-CPU)
 */
struct rq {
    /* CPU */
    u32 cpu;
    
    /* Lock */
    spinlock_t lock;
    
    /* Number Running */
    unsigned int nr_running;
    
    /* CFS */
    struct cfs_rq cfs;
    
    /* Real-Time */
    struct rt_rq rt;
    
    /* Current Task */
    struct task_struct *curr;
    struct task_struct *idle;
    struct task_struct *prev;
    
    /* Next Task */
    struct task_struct *next;
    
    /* Clock */
    u64 clock;
    u64 clock_task;
    
    /* Statistics */
    u64 nr_switches;
    u64 nr_sched_events;

    /* Load Balancing */
    unsigned long cpu_load[5];

    /* Per-CPU Data */
    percpu_data_t *percpu;
};

/**
 * sched_class - Scheduler class operations
 */
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

/*===========================================================================*/
/*                         SCHEDULER API                                     */
/*===========================================================================*/

/* Scheduler Initialization */
void scheduler_init(void);
void scheduler_start(void);
void scheduler_enable_all(void);

/* Task Creation/Destruction */
struct task_struct *alloc_task_struct(void);
void free_task_struct(struct task_struct *task);
struct task_struct *task_create(const char *name, void (*fn)(void *), void *arg);
void task_destroy(struct task_struct *task);
void do_exit(struct task_struct *task, int code);

/* PID/TID Management */
tid_t allocate_tid(void);
void free_tid(tid_t tid);
pid_t allocate_pid(void);
void free_pid(pid_t pid);

/* Task Hash Table */
void task_hash_insert(struct task_struct *task);
void task_hash_remove(struct task_struct *task);

/* Task Queue Management */
void enqueue_task(struct rq *rq, struct task_struct *task, int flags);
void dequeue_task(struct rq *rq, struct task_struct *task, int flags);

/* Task Control */
int task_wake_up(struct task_struct *task);
int task_sleep(struct task_struct *task);
int task_stop(struct task_struct *task);
int task_continue(struct task_struct *task);
int task_kill(struct task_struct *task, int signal);

/* Scheduling */
void schedule(void);
void schedule_tail(struct task_struct *prev);
void preempt_schedule(void);
void preempt_disable(void);
void preempt_enable(void);

/* Context Switch */
void context_switch(struct rq *rq, struct task_struct *prev, struct task_struct *next);
void switch_to(struct task_struct *prev, struct task_struct *next);

/* Priority */
void set_user_nice(struct task_struct *task, long nice);
long task_nice(struct task_struct *task);
int set_scheduler_class(struct task_struct *task, int policy);
int get_scheduler_class(struct task_struct *task);

/* CPU Affinity */
int set_cpus_allowed(struct task_struct *task, u64 cpus_allowed);
u64 get_cpus_allowed(struct task_struct *task);

/* Idle Task */
struct task_struct *idle_task(int cpu);
bool is_idle_task(struct task_struct *task);

/* Current Task */
struct task_struct *get_current(void);
#define current get_current()

/* Run Queue Access */
struct rq *get_rq(u32 cpu);
struct rq *get_cpu_rq(void);
void rq_lock(struct rq *rq);
void rq_unlock(struct rq *rq);

/* Task Reference */
void get_task(struct task_struct *task);
void put_task(struct task_struct *task);

/* Task Iteration */
struct task_struct *task_by_pid(pid_t pid);
struct task_struct *task_by_tid(tid_t tid);
void for_each_task(struct task_struct *task);

/* Wait Queue Operations */
void wait_event(wait_queue_t *wq, int condition);
int wait_event_interruptible(wait_queue_t *wq, int condition);
void wake_up(wait_queue_t *wq);
void wake_up_interruptible(wait_queue_t *wq);

/* Scheduler Statistics */
void scheduler_stats(void);
void task_stats(struct task_struct *task);

/* Load Balancing */
void rebalance_domains(void);
void idle_balance(int cpu);

/* High-Resolution Timer Support */
void hrtimer_init(void);
void hrtimer_start(u64 expires_ns);
void hrtimer_cancel(void);

#endif /* _NEXUS_SCHED_H */
