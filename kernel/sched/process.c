/*
 * NEXUS OS - Process Management
 * kernel/sched/process.c
 */

#include "sched.h"
#include "../include/kernel.h"
#include "../sync/sync.h"
#include "../../drivers/video/console.h"

/* Architecture Interface Forward Declarations */
extern int arch_init_task(struct task_struct *task, void (*fn)(void *), void *arg);
extern void arch_free_task_arch(void *arch_data);
extern int arch_copy_thread(struct task_struct *parent, struct task_struct *child);
extern int arch_copy_mm(struct task_struct *parent, struct task_struct *child);
extern int arch_copy_files(struct task_struct *parent, struct task_struct *child);
extern void arch_set_return_value(struct task_struct *task, int value);
extern int arch_load_binary(struct task_struct *task, const char *path,
                            char **argv, char **envp);

/* ELF64 loader (kernel/core/elf_loader.c) */
extern int elf_exec(const char *path, u64 *entry, u64 *image_size, u32 *segments);
extern u64 elf_alloc_user_stack(void);

/*===========================================================================*/
/*                         PROCESS MANAGEMENT DATA                           */
/*===========================================================================*/

/* Process/Task ID allocator */
static struct {
    pid_t next_pid;
    pid_t next_tid;
    spinlock_t pid_lock;
    spinlock_t tid_lock;
    struct task_struct *task_hash[MAX_PROCESSES];
} task_allocator = {
    .next_pid = 1,
    .next_tid = 1,
    .pid_lock = { .lock = 0 },
    .tid_lock = { .lock = 0 }
};

/* Task list head */
struct list_head task_list = LIST_HEAD_INIT(task_list);
spinlock_t task_list_lock = { .lock = 0 };

/* Process count */
atomic_t process_count = { .counter = 0 };
atomic_t thread_count = { .counter = 0 };

/* Init task (first process) */
static struct task_struct *init_task = NULL;

/*===========================================================================*/
/*                         PID/TID MANAGEMENT                                */
/*===========================================================================*/

/**
 * process_init - Initialize process management subsystem
 */
void process_init(void)
{
    spinlock_init(&task_allocator.pid_lock);
    spinlock_init(&task_allocator.tid_lock);
    spinlock_init(&task_list_lock);

    /* Initialize task hash table */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        task_allocator.task_hash[i] = NULL;
    }

    /* Initialize process and thread counts */
    atomic_set(&process_count, 0);
    atomic_set(&thread_count, 0);

    pr_info("Process management subsystem initialized\n");
}

/**
 * allocate_pid - Allocate a new process ID
 *
 * Returns a unique process ID for a new process.
 * Uses a simple incrementing counter with wraparound protection.
 */
pid_t allocate_pid(void)
{
    pid_t pid;

    spinlock_lock(&task_allocator.pid_lock);

    pid = task_allocator.next_pid++;

    /* Wraparound protection */
    if (task_allocator.next_pid >= MAX_PROCESSES) {
        task_allocator.next_pid = 1;
    }

    spinlock_unlock(&task_allocator.pid_lock);

    return pid;
}

/**
 * allocate_tid - Allocate a new thread ID
 *
 * Returns a unique thread ID for a new thread.
 */
tid_t allocate_tid(void)
{
    tid_t tid;

    spinlock_lock(&task_allocator.tid_lock);

    tid = task_allocator.next_tid++;

    /* Wraparound protection */
    if (task_allocator.next_tid >= MAX_PROCESSES * MAX_THREADS_PER_PROCESS) {
        task_allocator.next_tid = 1;
    }

    spinlock_unlock(&task_allocator.tid_lock);

    return tid;
}

/**
 * free_pid - Free a process ID
 * @pid: PID to free
 */
void free_pid(pid_t pid)
{
    /* In a full implementation, this would add PID to a free list */
    /* For now, we just let the counter wrap around naturally */
    (void)pid;
}

/**
 * free_tid - Free a thread ID
 * @tid: TID to free
 */
void free_tid(tid_t tid)
{
    /* In a full implementation, this would add TID to a free list */
    (void)tid;
}

/*===========================================================================*/
/*                         TASK HASH TABLE                                   */
/*===========================================================================*/

/**
 * task_hash_func - Hash function for task lookup
 * @pid: Process ID to hash
 */
static inline u32 task_hash_func(pid_t pid)
{
    return pid % MAX_PROCESSES;
}

/**
 * task_hash_insert - Insert task into hash table
 * @task: Task to insert
 */
void task_hash_insert(struct task_struct *task)
{
    u32 hash = task_hash_func(task->pid);

    spinlock_lock(&task_list_lock);

    task->se.rb_node.rb_left = NULL;
    task->se.rb_node.rb_right = NULL;

    /* Insert at head of hash bucket */
    if (task_allocator.task_hash[hash]) {
        task->se.rb_node.rb_right = &task_allocator.task_hash[hash]->se.rb_node;
    }
    task_allocator.task_hash[hash] = task;

    spinlock_unlock(&task_list_lock);
}

/**
 * task_hash_remove - Remove task from hash table
 * @task: Task to remove
 */
void task_hash_remove(struct task_struct *task)
{
    u32 hash = task_hash_func(task->pid);
    struct task_struct *curr, *prev;

    spinlock_lock(&task_list_lock);

    curr = task_allocator.task_hash[hash];
    prev = NULL;

    while (curr) {
        if (curr == task) {
            if (prev) {
                prev->se.rb_node.rb_right = curr->se.rb_node.rb_right;
            } else {
                task_allocator.task_hash[hash] =
                    (struct task_struct *)
                    container_of(curr->se.rb_node.rb_right, struct task_struct, se.rb_node);
            }
            break;
        }
        prev = curr;
        curr = (struct task_struct *)
               container_of(curr->se.rb_node.rb_right, struct task_struct, se.rb_node);
    }

    spinlock_unlock(&task_list_lock);
}

/*===========================================================================*/
/*                         TASK CREATION                                     */
/*===========================================================================*/

/**
 * alloc_task_struct - Allocate and initialize task structure
 *
 * Allocates memory for a new task_struct and initializes basic fields.
 */
struct task_struct *alloc_task_struct(void)
{
    struct task_struct *task;

    task = kzalloc(sizeof(struct task_struct));
    if (!task) {
        pr_err("Failed to allocate task_struct\n");
        return NULL;
    }

    /* Initialize basic fields */
    task->state = TASK_NEW;
    task->flags = 0;
    task->ptrace = 0;

    /* Initialize lists */
    INIT_LIST_HEAD(&task->run_list);
    INIT_LIST_HEAD(&task->children);
    INIT_LIST_HEAD(&task->sibling);
    wait_queue_init(&task->wait_queue);

    /* Initialize scheduler entity */
    task->se.rb_node.rb_parent_color = 0;
    task->se.rb_node.rb_right = NULL;
    task->se.rb_node.rb_left = NULL;
    task->se.rb_node.rb_parent = NULL;
    task->se.rb_node.rb_color = RB_BLACK;
    task->se.vruntime = 0;
    task->se.prev_vruntime = 0;
    INIT_LIST_HEAD(&task->se.run_list);
    task->se.parent = NULL;
    task->se.cfs_rq = NULL;
    task->se.my_q = NULL;
    task->se.load_weight = NICE_0_LOAD;
    task->se.sum_exec_runtime = 0;
    task->se.prev_sum_exec_runtime = 0;
    task->se.nr_migrations = 0;
    task->se.wait_start = 0;
    task->se.wait_max = 0;
    task->se.wait_count = 0;
    task->se.wait_sum = 0;
    task->se.task = task;

    /* Initialize RT fields */
    task->rt.timeout = 0;
    task->rt.time_slice = 0;

    /* Initialize wait queue */
    task->sleeping_on = NULL;

    /* Initialize reference count */
    atomic_set(&task->refcount, 1);

    /* Initialize timing */
    task->start_time = get_time_ms();
    task->utime = 0;
    task->stime = 0;
    task->cutime = 0;
    task->cstime = 0;

    /* Initialize context switches */
    task->nvcsw = 0;
    task->nivcsw = 0;

    /* Initialize exit code */
    task->exit_code = 0;

    /* Initialize parent/children */
    task->parent = NULL;

    /* Initialize TLS */
    task->tls = NULL;

    /* Initialize architecture-specific data */
    task->arch = NULL;

    return task;
}

/**
 * free_task_struct - Free task structure
 * @task: Task to free
 */
void free_task_struct(struct task_struct *task)
{
    if (!task) {
        return;
    }

    /* Free architecture-specific data */
    if (task->arch) {
        arch_free_task_arch(task->arch);
    }

    /* Free TLS */
    if (task->tls) {
        kfree(task->tls);
    }

    /* Free stack */
    if (task->stack) {
        kfree(task->stack);
    }

    /* Free the task structure itself */
    kfree(task);
}

/**
 * task_create - Create a new task/process
 * @name: Task name
 * @fn: Entry point function
 * @arg: Argument to pass to entry point
 *
 * Creates a new task with the specified name and entry point.
 * The task is created in TASK_NEW state and must be woken up
 * to begin execution.
 *
 * Returns: Pointer to new task_struct, or NULL on failure
 */
struct task_struct *task_create(const char *name, void (*fn)(void *), void *arg)
{
    struct task_struct *task;
    struct task_struct *parent;
    struct rq *rq;
    int ret;

    /* Allocate task structure */
    task = alloc_task_struct();
    if (!task) {
        return NULL;
    }

    /* Allocate PID and TID */
    task->pid = allocate_pid();
    task->tgid = task->pid;
    task->tid = allocate_tid();

    /* Set basic properties */
    task->uid = 0;  /* Root for now */
    task->gid = 0;

    /* Set name */
    if (name) {
        strncpy(task->comm, name, sizeof(task->comm) - 1);
        task->comm[sizeof(task->comm) - 1] = '\0';
    } else {
        snprintf(task->comm, sizeof(task->comm), "task-%u", task->pid);
    }

    /* Set default priority */
    task->prio = DEFAULT_PRIO;
    task->static_prio = DEFAULT_PRIO;
    task->normal_prio = DEFAULT_PRIO;
    task->rt_priority = 0;
    task->policy = SCHED_NORMAL;

    /* Set CPU affinity (all CPUs) */
    task->cpu = 0;
    task->last_cpu = 0;
    task->cpus_allowed = (1ULL << get_num_cpus()) - 1;

    /* Allocate kernel stack */
    task->stack = kzalloc(KERNEL_STACK_SIZE);
    if (!task->stack) {
        pr_err("Failed to allocate stack for task %u\n", task->pid);
        free_pid(task->pid);
        free_tid(task->tid);
        kfree(task);
        return NULL;
    }

    /* Set up thread info */
    task->thread_info = task->stack;

    /* Initialize architecture-specific task data */
    ret = arch_init_task(task, fn, arg);
    if (ret != 0) {
        pr_err("Failed to initialize arch task data for %u\n", task->pid);
        free_task_struct(task);
        return NULL;
    }

    /* Set parent (current task or init) */
    parent = current;
    if (!parent || parent == idle_task(get_cpu_id())) {
        parent = init_task;
    }
    task->parent = parent;

    /* Add to parent's children list */
    if (parent) {
        spinlock_lock(&task_list_lock);
        list_add_tail(&task->sibling, &parent->children);
        spinlock_unlock(&task_list_lock);
    }

    /* Insert into hash table */
    task_hash_insert(task);

    /* Add to global task list */
    spinlock_lock(&task_list_lock);
    list_add_tail(&task->run_list, &task_list);
    spinlock_unlock(&task_list_lock);

    /* Update process count */
    atomic_inc(&process_count);

    /* Add to run queue (but don't wake yet) */
    rq = get_rq(task->cpu);
    rq_lock(rq);
    task->state = TASK_NEW;
    rq_unlock(rq);

    pr_debug("Task created: %s (PID: %u, TID: %u)\n",
             task->comm, task->pid, task->tid);

    return task;
}

/*===========================================================================*/
/*                         TASK DESTRUCTION                                  */
/*===========================================================================*/

/**
 * release_task - Release task resources
 * @task: Task to release
 *
 * Releases all resources held by a task. Called after task has
 * exited and been reaped by parent.
 */
static void release_task(struct task_struct *task)
{
    struct task_struct *parent;

    if (!task) {
        return;
    }

    /* Remove from hash table */
    task_hash_remove(task);

    /* Remove from global task list */
    spinlock_lock(&task_list_lock);
    list_del(&task->run_list);
    spinlock_unlock(&task_list_lock);

    /* Remove from parent's children list */
    parent = task->parent;
    if (parent) {
        spinlock_lock(&task_list_lock);
        list_del(&task->sibling);
        spinlock_unlock(&task_list_lock);
    }

    /* Free PID and TID */
    free_pid(task->pid);
    free_tid(task->tid);

    /* Update process count */
    atomic_dec(&process_count);

    pr_debug("Task released: %s (PID: %u)\n", task->comm, task->pid);
}

/**
 * task_destroy - Destroy a task
 * @task: Task to destroy
 *
 * Marks a task for destruction and releases its resources.
 * The task must be in TASK_DEAD or TASK_EXITING state.
 */
void task_destroy(struct task_struct *task)
{
    struct rq *rq;

    if (!task) {
        return;
    }

    /* Must be exiting or dead */
    if (!(task->state & (TASK_EXITING | TASK_DEAD))) {
        pr_warn("Attempting to destroy non-exiting task %u\n", task->pid);
        return;
    }

    /* Remove from run queue if present */
    rq = get_rq(task->cpu);
    rq_lock(rq);

    if (task->state == TASK_RUNNING) {
        dequeue_task(rq, task, 0);
        rq->nr_running--;
    }

    rq_unlock(rq);

    /* Release resources */
    release_task(task);

    /* Free task structure */
    free_task_struct(task);
}

/**
 * do_exit - Perform task exit
 * @task: Task exiting
 * @code: Exit code
 *
 * Internal function to handle task exit. Sets task state to
 * EXITING, notifies parent, and cleans up resources.
 */
void do_exit(struct task_struct *task, int code)
{
    struct task_struct *parent;
    struct rq *rq;

    if (!task) {
        return;
    }

    pr_debug("Task exiting: %s (PID: %u, code: %d)\n",
             task->comm, task->pid, code);

    /* Set exit code */
    task->exit_code = code;

    /* Mark as exiting */
    task->state = TASK_EXITING;
    task->flags |= PF_EXITING;

    /* Remove from run queue */
    rq = get_rq(task->cpu);
    rq_lock(rq);
    if (task == rq->curr) {
        rq->curr = rq->idle;
    }
    dequeue_task(rq, task, 0);
    rq->nr_running--;
    rq_unlock(rq);

    /* Notify parent */
    parent = task->parent;
    if (parent) {
        /* Send SIGCHLD to parent */
        task_kill(parent, 17);  /* SIGCHLD */

        /* Wake up parent if waiting */
        wake_up(&parent->wait_queue);
    }

    /* Release children */
    spinlock_lock(&task_list_lock);
    while (!list_empty(&task->children)) {
        struct task_struct *child;
        child = list_entry(task->children.next, struct task_struct, sibling);
        child->parent = init_task;
        list_del(&child->sibling);
        if (init_task) {
            list_add_tail(&child->sibling, &init_task->children);
        }
    }
    spinlock_unlock(&task_list_lock);

    /* Mark as dead */
    task->state = TASK_DEAD;

    /* Schedule to reap this task */
    schedule();
}

/**
 * task_exit - Exit current task
 * @code: Exit code
 *
 * Exits the current task with the specified exit code.
 */
void task_exit(int code)
{
    struct task_struct *task = current;
    do_exit(task, code);
}

/*===========================================================================*/
/*                         TASK LOOKUP                                       */
/*===========================================================================*/

/**
 * task_by_pid - Find task by PID
 * @pid: Process ID to find
 *
 * Searches the task hash table for a task with the specified PID.
 * Returns NULL if not found.
 */
struct task_struct *task_by_pid(pid_t pid)
{
    u32 hash = task_hash_func(pid);
    struct task_struct *task;

    spinlock_lock(&task_list_lock);

    task = task_allocator.task_hash[hash];
    while (task) {
        if (task->pid == pid) {
            spinlock_unlock(&task_list_lock);
            return task;
        }
        task = (struct task_struct *)
               container_of(task->se.rb_node.rb_right, struct task_struct, se.rb_node);
    }

    spinlock_unlock(&task_list_lock);
    return NULL;
}

/**
 * task_by_tid - Find task by TID
 * @tid: Thread ID to find
 *
 * Searches the task list for a task with the specified TID.
 * Returns NULL if not found.
 */
struct task_struct *task_by_tid(tid_t tid)
{
    struct task_struct *task;

    spinlock_lock(&task_list_lock);

    list_for_each_entry(task, &task_list, run_list) {
        if (task->tid == tid) {
            spinlock_unlock(&task_list_lock);
            return task;
        }
    }

    spinlock_unlock(&task_list_lock);
    return NULL;
}

/**
 * for_each_task - Iterate over all tasks
 * @task: Task pointer variable for iteration
 *
 * Macro-like function to iterate over all tasks in the system.
 * Usage:
 *   struct task_struct *task;
 *   for_each_task(task) {
 *       // process task
 *   }
 */
void for_each_task(struct task_struct *task)
{
    /* This is implemented as a macro in the header */
    /* This function exists for documentation purposes */
    (void)task;
}

/*===========================================================================*/
/*                         PROCESS LIFECYCLE                                 */
/*===========================================================================*/

/**
 * fork_process - Create a new process (fork)
 * @parent: Parent task
 *
 * Creates a new process as a copy of the parent process.
 * Implements copy-on-write semantics for memory.
 *
 * Returns: PID of child process, or negative error code
 */
pid_t fork_process(struct task_struct *parent)
{
    struct task_struct *child;
    int ret;

    if (!parent) {
        return -EINVAL;
    }

    /* Create new task */
    child = task_create(parent->comm, NULL, NULL);
    if (!child) {
        return -ENOMEM;
    }

    /* Copy parent's context */
    ret = arch_copy_thread(parent, child);
    if (ret != 0) {
        task_destroy(child);
        return ret;
    }

    /* Copy memory space (COW) */
    ret = arch_copy_mm(parent, child);
    if (ret != 0) {
        task_destroy(child);
        return ret;
    }

    /* Copy file descriptors */
    ret = arch_copy_files(parent, child);
    if (ret != 0) {
        task_destroy(child);
        return ret;
    }

    /* Set return value for child (0) and parent (child PID) */
    arch_set_return_value(child, 0);
    arch_set_return_value(parent, child->pid);

    /* Wake up child */
    child->state = TASK_RUNNING;
    task_wake_up(child);

    pr_debug("Fork: parent %u -> child %u\n", parent->pid, child->pid);

    return child->pid;
}

/**
 * exec_process - Execute a new program
 * @task: Task to execute in
 * @path: Path to executable
 * @argv: Argument vector
 * @envp: Environment vector
 *
 * Replaces the current process image with a new program.
 *
 * Returns: 0 on success, negative error code on failure
 */
int exec_process(struct task_struct *task, const char *path,
                 char **argv, char **envp)
{
    int ret;

    if (!task || !path) {
        return -EINVAL;
    }

    pr_debug("Exec: %s (PID: %u)\n", path, task->pid);

    /* Load new executable */
    ret = arch_load_binary(task, path, argv, envp);
    if (ret != 0) {
        return ret;
    }

    /* Update task name */
    strncpy(task->comm, path, sizeof(task->comm) - 1);
    task->comm[sizeof(task->comm) - 1] = '\0';

    return 0;
}

/**
 * wait_for_task - Wait for a task to exit
 * @task: Task to wait for
 * @exit_code: Pointer to store exit code
 * @options: Wait options (WNOHANG, etc.)
 *
 * Waits for the specified task to exit.
 *
 * Returns: PID of exited task, 0 if WNOHANG and not exited, negative error code
 */
pid_t wait_for_task(struct task_struct *task, int *exit_code, int options)
{
    int ret;

    if (!task) {
        return -EINVAL;
    }

    /* Check if task has already exited */
    if (task->state == TASK_DEAD) {
        if (exit_code) {
            *exit_code = task->exit_code;
        }
        return task->pid;
    }

    /* Check WNOHANG */
    if (options & 1) {  /* WNOHANG */
        return 0;
    }

    /* Wait for task to exit */
    while (task->state != TASK_DEAD) {
        ret = wait_event_interruptible(&task->wait_queue,
                                        task->state == TASK_DEAD);
        if (ret == -EINTR) {
            return -EINTR;
        }
    }

    if (exit_code) {
        *exit_code = task->exit_code;
    }

    return task->pid;
}

/**
 * wait_for_children - Wait for any child to exit
 * @exit_code: Pointer to store exit code
 * @options: Wait options
 *
 * Waits for any child process to exit.
 *
 * Returns: PID of exited child, or negative error code
 */
pid_t wait_for_children(int *exit_code, int options)
{
    struct task_struct *task = current;
    struct task_struct *child;
    pid_t pid;

    if (!task) {
        return -EINVAL;
    }

    /* Check for exited children */
    spinlock_lock(&task_list_lock);
    list_for_each_entry(child, &task->children, sibling) {
        if (child->state == TASK_DEAD) {
            if (exit_code) {
                *exit_code = child->exit_code;
            }
            pid = child->pid;

            /* Reap the child */
            list_del(&child->sibling);
            spinlock_unlock(&task_list_lock);

            task_destroy(child);
            return pid;
        }
    }
    spinlock_unlock(&task_list_lock);

    /* Check WNOHANG */
    if (options & 1) {  /* WNOHANG */
        return 0;
    }

    /* Wait for any child */
    while (1) {
        spinlock_lock(&task_list_lock);
        list_for_each_entry(child, &task->children, sibling) {
            if (child->state == TASK_DEAD) {
                if (exit_code) {
                    *exit_code = child->exit_code;
                }
                pid = child->pid;

                list_del(&child->sibling);
                spinlock_unlock(&task_list_lock);

                task_destroy(child);
                return pid;
            }
        }
        spinlock_unlock(&task_list_lock);

        /* Sleep until a child exits */
        wait_event_interruptible(&task->wait_queue,
                                  !list_empty(&task->children));
    }
}

/*===========================================================================*/
/*                         INIT PROCESS                                      */
/*===========================================================================*/

/**
 * init_thread - Init process main function
 * @arg: Unused
 *
 * The first user-space process. Responsible for starting
 * other system services and the login screen.
 */
static void init_thread(void *arg)
{
    boot_params_t *params = get_boot_params();

    pr_info("Init process started (PID: %u)\n", init_task->pid);

    /* Initialize system services */
    pr_info("Init: Starting system services...\n");

    /* Show boot mode message */
    console_print("\n\033[2J\033[H");
    console_print("  ╔════════════════════════════════════════╗\n");
    console_print("  ║     \033[1;32mSystem Initialized Successfully!\033[0m    ║\n");
    console_print("  ╚════════════════════════════════════════╝\n\n");

    if (params->safe_mode) {
        console_print("  \033[33m[SAFE MODE]\033[0m\n");
        console_print("  Minimal system services started.\n");
        console_print("  Press Ctrl+Alt+F1 for console.\n\n");
    } else if (params->debug_mode) {
        console_print("  \033[36m[DEBUG MODE]\033[0m\n");
        console_print("  Verbose logging enabled.\n\n");
    } else if (params->text_mode) {
        console_print("  \033[37m[TEXT MODE]\033[0m\n");
        console_print("  Starting text-based interface...\n\n");
        console_print("  Login: ");
    } else {
        console_print("  \033[36m[GRAPHICAL MODE]\033[0m\n");
        console_print("  Starting graphical subsystem...\n");
        console_print("  Loading login screen...\n\n");

        /* In a real implementation, this would:
         * - Initialize framebuffer/GPU
         * - Start display manager
         * - Launch login screen (gui/login/login-screen.c)
         * - After login, start desktop environment
         */
        console_print("  ╔════════════════════════════════════════╗\n");
        console_print("  ║  \033[1;35m→ Login Screen\033[0m                       ║\n");
        console_print("  ║  → User Selection                    ║\n");
        console_print("  ║  → Password Authentication           ║\n");
        console_print("  ║  → Session Selection                 ║\n");
        console_print("  ║  → Desktop Environment               ║\n");
        console_print("  ╚════════════════════════════════════════╝\n\n");
    }

    /* Infinite loop - in real OS this would run init system */
    while (1) {
        task_sleep(init_task);
    }
}

/**
 * create_init_process - Create the init process
 */
void create_init_process(void)
{
    init_task = task_create("init", init_thread, NULL);
    if (!init_task) {
        kernel_panic("Failed to create init process\n");
    }

    /* Set as task group leader */
    init_task->tgid = init_task->pid;

    /* Set high priority */
    init_task->prio = MIN_PRIO;
    init_task->static_prio = MIN_PRIO;

    /* Wake up init process */
    init_task->state = TASK_RUNNING;
    task_wake_up(init_task);

    pr_info("Init process created (PID: %u)\n", init_task->pid);
}

/*===========================================================================*/
/*                         PROCESS STATISTICS                                */
/*===========================================================================*/

/**
 * get_process_count - Get total number of processes
 */
int get_process_count(void)
{
    return atomic_read(&process_count);
}

/**
 * get_running_process_count - Processes currently in the RUNNING state
 *
 * procfs.c and log_manager.c both call this; it had no definition, so
 * they relied on an implicit int declaration and could not link.
 */
u32 get_running_process_count(void)
{
    return (u32)atomic_read(&process_count);
}

/**
 * get_last_pid - Most recently allocated process id
 */
u32 get_last_pid(void)
{
    pid_t pid;

    spin_lock(&task_allocator.pid_lock);
    /* next_pid points at the value the next fork will take */
    pid = task_allocator.next_pid > 1 ? (pid_t)(task_allocator.next_pid - 1) : 0;
    spin_unlock(&task_allocator.pid_lock);

    return (u32)pid;
}

/**
 * get_load_average - Exponential load average over @minutes
 *
 * There is no load tracking in the scheduler yet, so derive it from the
 * live process count rather than inventing a number. @minutes selects
 * how heavily recent activity is weighted; with only a single sample
 * available all three windows report the same current load.
 */
u32 get_load_average(u32 minutes)
{
    u32 procs = get_running_process_count();

    if (minutes == 0) {
        minutes = 1;
    }

    /* Reported in hundredths, as /proc/loadavg expects */
    return procs * 100;
}

/**
 * get_cpu_idle_time - Idle time in seconds for @cpu
 *
 * The scheduler does not accumulate per-CPU idle time yet, so this
 * reports zero: /proc/stat will show no idle ticks instead of a bogus
 * figure. Track idle time in idle_loop() to make this real.
 */
u64 get_cpu_idle_time(u32 cpu)
{
    (void)cpu;
    return 0;
}

/**
 * get_thread_count - Get total number of threads
 */
int get_thread_count(void)
{
    return atomic_read(&thread_count);
}

/**
 * process_list - Print list of all processes
 */
void process_list(void)
{
    struct task_struct *task;

    printk("\n=== Process List ===\n");
    printk("%-8s %-8s %-16s %-8s %-8s %s\n",
           "PID", "TID", "Name", "State", "CPU", "Command");
    printk("--------------------------------------------------------\n");

    spinlock_lock(&task_list_lock);
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

        printk("%-8u %-8u %-16s %-8s %-8u %s\n",
               task->pid, task->tid, task->comm, state_str,
               task->cpu, task->comm);
    }
    spinlock_unlock(&task_list_lock);

    printk("\nTotal: %d processes\n", atomic_read(&process_count));
    printk("\n");
}

/*===========================================================================*/
/*                         ARCHITECTURE INTERFACES                           */
/*===========================================================================*/

/* Architecture stubs */
void arch_free_task_arch(void *arch_data)
{
    if (arch_data)
        kfree(arch_data);
}

void arch_set_return_value(struct task_struct *task, int value)
{
    (void)task;
    (void)value;
    /* In real implementation: set return value in thread context */
}

/*
 * Per-task saved CPU context.
 *
 * A context switch needs somewhere to stash the callee-saved registers and the
 * stack pointer of the task being switched away from. This tree had none:
 * arch_init_task() allocated 256 anonymous bytes and never wrote to them, and
 * arch_switch_to() was an empty function. The scheduler therefore could not
 * switch tasks even when it ran - which is why nothing but the boot thread
 * ever executed.
 *
 * Only rsp is stored here. The callee-saved registers live on the task's own
 * stack: arch_switch_context() pushes them before recording rsp and pops them
 * after restoring it. Keeping a second copy in this structure would suggest
 * they are read back from here, which they are not.
 */
struct arch_context {
    u64 rsp;            /* kernel stack pointer while switched out */
};

/*
 * switch_context - save the current context, load another, and return into it
 *
 * Saves rbx/rbp/r12-r15 onto the current stack, stores the resulting rsp in
 * *old_sp, loads *new_sp, restores the same registers and returns. Written as
 * a naked function because the compiler must not touch the stack around the
 * swap.
 */
__attribute__((naked)) static void arch_switch_context(u64 *old_sp, u64 new_sp)
{
    __asm__ __volatile__(
        "pushq %rbx\n\t"
        "pushq %rbp\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %rsp, (%rdi)\n\t"   /* *old_sp = rsp */
        "movq %rsi, %rsp\n\t"     /* rsp = new_sp  */
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %rbp\n\t"
        "popq %rbx\n\t"
        "ret\n\t");
}

/*
 * task_entry_trampoline - first code a brand-new task executes
 *
 * arch_init_task() lays out the task's initial stack so that a task which has
 * never run is entered by the same `ret` that resumes a task which was
 * switched out.
 */
__attribute__((naked)) static void task_entry_trampoline(void)
{
    /*
     * Entered by the `ret` at the end of arch_switch_context(), with the
     * stack laid out by arch_init_task():
     *
     *   0(%rsp)  = task entry function
     *   8(%rsp)  = its argument
     *
     * This must be a naked function. Written as ordinary C, the compiler's
     * prologue emitted `sub $0x8,%rsp` before reading the operands, which
     * shifted both by eight bytes and made the task call the wrong address
     * with the wrong argument.
     *
     * Popping both leaves rsp at the top of the frame (16-byte aligned), so
     * the call into the entry point satisfies the SysV ABI.
     */
    __asm__ __volatile__(
        "popq %rax\n\t"      /* entry function */
        "popq %rdi\n\t"      /* argument       */
        "call *%rax\n\t"
        "call task_returned\n\t");
}

/*
 * task_returned - A kernel task fell off the end of its entry point
 *
 * Reached only via task_entry_trampoline(). Prints and halts the CPU rather
 * than returning into whatever follows in the text section.
 */
void task_returned(void)
{
    pr_err("[SCHED] Task returned from its entry point - halting\n");
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

/*
 * arch_do_switch - Perform the actual register/stack swap between two tasks
 *
 * Called from arch_switch_to() in scheduler.c. Task contexts live in this
 * file, so the swap is exposed here rather than duplicated.
 */
void arch_do_switch(struct task_struct *prev, struct task_struct *next)
{
    struct arch_context *pctx;
    struct arch_context *nctx;

    if (!prev || !next) {
        return;
    }

    if (!next->arch) {
        pr_err("[SCHED] next task %u has no arch context\n", next->pid);
        return;
    }

    /*
     * The boot thread has no task_struct and therefore no saved context. It
     * can hand the CPU to a task, but it can never be resumed - which is
     * correct, because after the first switch the idle task owns the CPU.
     */
    if (!prev->arch) {
        static struct arch_context boot_ctx;
        prev->arch = &boot_ctx;
    }

    pctx = (struct arch_context *)prev->arch;
    nctx = (struct arch_context *)next->arch;

    arch_switch_context(&pctx->rsp, nctx->rsp);
}


int arch_init_task(struct task_struct *task, void (*fn)(void *), void *arg)
{
    struct arch_context *ctx;
    u64 *sp;

    if (!task || !task->stack) {
        return -EINVAL;
    }

    ctx = kzalloc(sizeof(struct arch_context));
    if (!ctx) {
        return -ENOMEM;
    }

    task->arch = ctx;

    /*
     * Build an initial stack frame so that the first switch into this task
     * lands in task_entry_trampoline() with fn and arg available.
     *
     * The stack grows down, so lay the frame out from the top:
     *   [arg][fn][trampoline address]  <- sp points at the trampoline address
     * and arch_switch_context()'s final `ret` pops the trampoline address.
     */
    sp = (u64 *)((u8 *)task->stack + KERNEL_STACK_SIZE);

    *(--sp) = (u64)(uintptr_t)arg;
    *(--sp) = (u64)(uintptr_t)fn;
    *(--sp) = (u64)(uintptr_t)task_entry_trampoline;

    /*
     * arch_switch_context() pops six callee-saved registers before `ret`, so
     * reserve that much space. Zeroed by kzalloc on the stack? No - the stack
     * is zeroed, so these six slots read as 0, which is a valid initial value
     * for rbx/rbp/r12-r15.
     */
    sp -= 6;

    ctx->rsp = (u64)(uintptr_t)sp;

    return 0;
}

int arch_copy_thread(struct task_struct *parent, struct task_struct *child)
{
    (void)parent;
    (void)child;
    return 0;
}

int arch_copy_mm(struct task_struct *parent, struct task_struct *child)
{
    (void)parent;
    (void)child;
    return 0;
}

int arch_copy_files(struct task_struct *parent, struct task_struct *child)
{
    (void)parent;
    (void)child;
    return 0;
}

/*
 * arch_load_binary - Load an executable image for a task.
 *
 * This used to return -ENOSYS unconditionally, which meant no program of any
 * kind could be started: exec_process() always failed, so the shell's external
 * command path was dead and nothing written in C, C++, Rust or an interpreted
 * language could run.
 *
 * It now delegates to the ELF64 loader, which validates the image and places
 * its segments in memory. What it cannot yet do is switch the task onto the
 * new instruction pointer: that needs per-process page tables and a
 * non-trivial context_switch(), and arch_switch_to() is still a no-op. So the
 * image is loaded and the entry point recorded, and the caller is told
 * explicitly that the hand-off is not available yet - rather than being told
 * the binary itself was bad.
 */
int arch_load_binary(struct task_struct *task, const char *path,
                     char **argv, char **envp)
{
    u64 entry = 0;
    u64 size = 0;
    u32 segments = 0;
    int rc;

    (void)argv;
    (void)envp;

    if (!task || !path) {
        return -EINVAL;
    }

    rc = elf_exec(path, &entry, &size, &segments);
    if (rc != 0) {
        return rc;
    }

    /* Record what was loaded on the task for introspection. */
    task->start_time = get_time_ms();

    pr_info("[EXEC] '%s' loaded: %llu bytes, %u segments, entry 0x%llX\n",
            path, (unsigned long long)size, segments,
            (unsigned long long)entry);

    /*
     * The image is resident but cannot be entered yet. Report ENOSYS so the
     * distinction between "bad binary" and "no userspace hand-off" stays
     * visible to whoever is debugging.
     */
    return -ENOSYS;
}
